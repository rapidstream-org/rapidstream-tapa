import collections
import configparser
import json
import logging
import os.path
from typing import Any, Callable, Dict, List, Optional, TextIO, Tuple

import autobridge.HLSParser.tapa

from tapa import util
from tapa.verilog import xilinx as rtl
from tapa.verilog.xilinx.async_mmap import ADDR_CHANNEL_DATA_WIDTH, RESP_CHANNEL_DATA_WIDTH

from .instance import Instance
from .task import Task

# TODO: use a better calibrated model
AREA_PER_HBM = {
    'LUT': 5000,
    'FF': 6500,
    'BRAM': 0,
    'URAM': 0,
    'DSP': 0,
}

# TODO: check that the FIFOs are 32 in depth
AREA_OF_M_AXI = {
    32: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 377,
        'LUT': 786,
        'URAM': 0,
    },
    64: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 375,
        'LUT': 848,
        'URAM': 0,
    },
    128: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 373,
        'LUT': 971,
        'URAM': 0,
    },
    256: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 371,
        'LUT': 1225,
        'URAM': 0,
    },
    512: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 369,
        'LUT': 1735,
        'URAM': 0,
    },
    1024: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 367,
        'LUT': 2755,
        'URAM': 0,
    },
}

_logger = logging.getLogger().getChild(__name__)

_PLACEHOLDER_REGION_PREFIX = 'PLACEHOLDER_'
_DUMMY_OFF_CHIP_MEMORY_PORT_PREFIX = 'DUMMY_PORT'
_DUMMY_VERTEX_NAME = 'DUMMY_VERTEX'


def generate_floorplan(
    top_task: Task,
    work_dir: str,
    fifo_width_getter: Callable[[Task, str], int],
    connectivity_fp: Optional[TextIO],
    part_num: str,
    **kwargs,
) -> Dict[str, Any]:
  _logger.info('total area estimation: %s', top_task.total_area)

  vertices = {x.name: x.task.name for x in top_task.instances}
  edges = {}

  # Self area for top task represents mainly the control_s_axi instance.
  area = {top_task.name: make_autobridge_area(top_task.self_area)}
  for instance in top_task.instances:
    if instance.task.name not in area:
      # Total area handles hierarchical designs for AutoBridge.
      task_area = make_autobridge_area(instance.task.total_area)
      area[instance.task.name] = task_area
      _logger.debug('area of %s: %s', instance.task.name, task_area)

  config = {
      'CompiledBy': 'TAPA',
      'Board': part_num.split('-')[0][2:].upper(),
      'OptionalFloorplan': collections.defaultdict(list),
      'Vertices': vertices,
      'Edges': edges,
      'Area': area,
      'GroupingConstraints': [],
      'ExtraKeyWordArgs': kwargs,
  }

  # Remember the number of ports instantiated (for shell area estimation).
  region_to_shell_area = collections.defaultdict(dict)

  # Set floorplans for tasks connected to MMAP and the ctrl instance.
  vertices[rtl.ctrl_instance_name(top_task.name)] = top_task.name
  ddr_list: List[int] = []

  width_table = {port.name: port.width for port in top_task.ports.values()}

  # connectivity refers to the .ini file with the arg to port mapping
  for connectivity in parse_connectivity(connectivity_fp):
    if not connectivity:
      continue

    kernel_arg, port = _parse_connectivity(connectivity)

    region = get_port_region(part_num, port=port)
    port_cat, port_id = parse_port(port)
    if port_cat == 'DDR':
      ddr_list.append(port_id)
    elif port_cat == 'HBM':
      accumulate_area(region_to_shell_area[region], AREA_PER_HBM)
    if not region:
      continue

    _add_connectivity_constraints(config, region, kernel_arg, top_task, width_table)

  config['DDR'] = list(dict.fromkeys(ddr_list))

  for region, shell_area in region_to_shell_area.items():
    placeholder_name = _PLACEHOLDER_REGION_PREFIX + region
    # Each placeholder is both a task name and a task instance name.
    vertices[placeholder_name] = placeholder_name
    # Placeholders are created per region.
    accumulate_area(area.setdefault(placeholder_name, {}), shell_area)
    # Placeholders are bound to their own region.
    config['OptionalFloorplan'][region].append(placeholder_name)

  # always constrain s_axi_control to the bottom right corner
  ctrl_region = get_ctrl_instance_region(part_num)
  config['OptionalFloorplan'][ctrl_region].append(rtl.ctrl_instance_name(top_task.name))

  user_pre_assignments_json = kwargs.get('pre_assignments', '')
  if user_pre_assignments_json:
    user_pre_assignments = json.loads(open(user_pre_assignments_json, 'r').read())
    for region, modules in user_pre_assignments.items():
      config['OptionalFloorplan'][region] += modules

  # ASYNC_MMAP is outside of its instantiating module and needs its own
  # floorplanning constraint.
  for instance in top_task.instances:
    for arg in instance.args:
      if arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
        floorplan_vertex_name = rtl.async_mmap_instance_name(arg.mmap_name)
        vertices[floorplan_vertex_name] = floorplan_vertex_name
        # TODO: use a better calibrated model
        width = max(width_table[arg.name], 32)
        area[floorplan_vertex_name] = AREA_OF_M_AXI[width]

  # Generate edges for scalar connections from the ctrl instance.
  for port in top_task.ports.values():
    width = port.width
    if port.cat in {Instance.Arg.Cat.MMAP, Instance.Arg.Cat.ASYNC_MMAP}:
      width = 64
    for arg in top_task.args[port.name]:
      edges[arg.instance.get_instance_arg(port.name)] = {
          'produced_by': rtl.ctrl_instance_name(top_task.name),
          'consumed_by': arg.instance.name,
          'width': width,
          'depth': 1,
      }

  # Generate edges for FIFOs instantiated in the top task.
  for fifo_name, fifo in top_task.fifos.items():
    edges[rtl.sanitize_array_name(fifo_name)] = {
        'produced_by': util.get_instance_name(fifo['produced_by']),
        'consumed_by': util.get_instance_name(fifo['consumed_by']),
        'width': fifo_width_getter(top_task, fifo_name),
        'depth': top_task.fifos[fifo_name]['depth']
    }

  # Dedup items.
  for region, instances in config['OptionalFloorplan'].items():
    instances[:] = dict.fromkeys(instances)
    region_area = {}
    for instance in instances:
      accumulate_area(region_area, area[vertices[instance]])
    _logger.info('hard constraint area in %s: %s', region, region_area)

  # Dump the input to AutoBridge.
  with open(os.path.join(work_dir, 'autobridge_config.json'), 'w') as fp:
    json.dump(config, fp, indent=2)

  # Generate floorplan using AutoBridge.
  floorplan = autobridge.HLSParser.tapa.generate_constraints(config)

  # Remove placeholders.
  for pblock in floorplan:
    filtered_list = []
    for v in floorplan[pblock]:
      if isinstance(v, str):
        if v.startswith(_PLACEHOLDER_REGION_PREFIX):
          continue
        if v.startswith(_DUMMY_OFF_CHIP_MEMORY_PORT_PREFIX):
          continue        
      filtered_list.append(v)
    floorplan[pblock] = filtered_list

  # Dump the output from AutoBridge.
  with open(os.path.join(work_dir, 'autobridge_floorplan.json'), 'w') as fp:
    json.dump(floorplan, fp, indent=2)

  return floorplan


def make_autobridge_area(area: Dict[str, int]) -> Dict[str, int]:
  return {{'BRAM_18K': 'BRAM'}.get(k, k): v for k, v in area.items()}


def accumulate_area(lhs: Dict[str, int], rhs: Dict[str, int]) -> None:
  """Accumulate area represented by rhs into lhs."""
  for k, v in rhs.items():
    lhs.setdefault(k, 0)
    lhs[k] += v


def parse_connectivity(fp: TextIO) -> List[str]:
  if fp is None:
    return []

  class MultiDict(dict):

    def __setitem__(self, key, value):
      if isinstance(value, list) and key in self:
        self[key].extend(value)
      else:
        super().__setitem__(key, value)

  config = configparser.RawConfigParser(dict_type=MultiDict, strict=False)
  config.read_file(fp)
  return config['connectivity']['sp'].splitlines()


def parse_port(port: str) -> Tuple[str, int]:
  bra = port.find('[')
  ket = port.find(']')
  colon = port.find(':')
  if colon != -1:
    ket = colon  # use the first channel if a range is specified
  return port[:bra], int(port[bra + 1:ket])


def get_port_region(part_num: str, port: str) -> str:
  port_cat, port_id = parse_port(port)
  if port_cat == 'PLRAM':
    return ''
  if part_num.startswith('xcu280-'):
    if port_cat == 'HBM' and 0 <= port_id < 32:
      return f'COARSE_X{port_id // 16}Y0'
    if port_cat == 'DDR' and 0 <= port_id < 2:
      return f'COARSE_X1Y{port_id}'
  elif part_num.startswith('xcu250-'):
    if port_cat == 'DDR' and 0 <= port_id < 4:
      return f'COARSE_X1Y{port_id}'
  raise NotImplementedError(f'unknown port {port} for {part_num}')


def get_ctrl_instance_region(part_num: str) -> str:
  if part_num.startswith('xcu250-') or part_num.startswith('xcu280-'):
    return 'COARSE_X1Y0'
  raise NotImplementedError(f'unknown {part_num}')


def _add_connectivity_constraints(
    config: Dict[str, Any],
    region: str,
    kernel_arg: str,
    top_task: Task,
    width_table: Dict[str, int]
) -> None:
  """
  based on the .ini configuration file:
  (1) use a dummy node to represent the DDR/HBM port
  (2) update the dataflow graph to add edges between dummy nodes and the task instance that access the port
  (3) TODO: adjust the AXI pipeline according to the floorplanning results
  """
  # async_mmap should be floorplanned together with its neighbor task instance
  for arg in top_task.args[kernel_arg]:
    if arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
      config['GroupingConstraints'].append([arg.instance.name, rtl.async_mmap_instance_name(arg.mmap_name)])

  for arg in top_task.args[kernel_arg]:
    # total width of an AXI channel
    # wr_data, wr_addr, rd_data, rd_addr, resp + other control signals (approximately 50)
    arg_width = width_table[arg.name]
    total_connection_width = arg_width + arg_width + ADDR_CHANNEL_DATA_WIDTH + ADDR_CHANNEL_DATA_WIDTH + RESP_CHANNEL_DATA_WIDTH + 50
    dummy_name = f'{_DUMMY_OFF_CHIP_MEMORY_PORT_PREFIX}_{region}_{arg.instance.name}_{arg.name}'

    config['OptionalFloorplan'][region].append(dummy_name)

    # dummy node represents where the mmap connects to
    config['Vertices'][dummy_name] = _DUMMY_VERTEX_NAME

    # the edge represents the AXI pipeline between the DDR/HBM IP and the mmap module
    config['Edges'][f'{dummy_name}_ch'] = {
      'produced_by': dummy_name,
      'consumed_by': arg.instance.name,
      'width': total_connection_width,
      'depth': 2,
    }

  config['Area'][_DUMMY_VERTEX_NAME] = {
    "BRAM": 0,
    "DSP": 0,
    "FF": 0,
    "LUT": 0,
    "URAM": 0
  }


def _parse_connectivity(connectivity: str) -> Tuple[str, str]:
  """
  parse the .ini config file. Example: 
  [connectivity]
  sp=serpens_1.edge_list_ch0:HBM[0]
  """
  dot = connectivity.find('.')
  colon = connectivity.find(':')
  kernel = connectivity[:dot]
  kernel_arg = connectivity[dot + 1:colon]
  port = connectivity[colon + 1:]
  return kernel_arg, port
