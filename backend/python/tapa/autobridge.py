import collections
import configparser
import json
import logging
import os.path
from typing import Any, Callable, Dict, List, Optional, TextIO

from tapa import util
from tapa.verilog import xilinx as rtl

from .instance import Instance
from .task import Task

_logger = logging.getLogger().getChild(__name__)


def generate_floorplan(
    top_task: Task,
    work_dir: str,
    fifo_width_getter: Callable[[Task, str], int],
    connectivity_fp: Optional[TextIO],
    part_num: str,
    max_usage: Optional[float],
) -> Dict[str, Any]:
  # pylint: disable=import-outside-toplevel
  import autobridge.HLSParser.tapa as autobridge

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
  }
  if max_usage is not None:
    config['MaxUsage'] = max_usage

  # Set floorplans for tasks connected to MMAP and the ctrl instance.
  vertices[rtl.ctrl_instance_name(top_task.name)] = top_task.name
  for connectivity in parse_connectivity(connectivity_fp):
    dot = connectivity.find('.')
    colon = connectivity.find(':')
    kernel = connectivity[:dot]
    kernel_arg = connectivity[dot + 1:colon]
    region = get_port_region(part_num, port=connectivity[colon + 1:])
    if not region:
      continue
    for arg in top_task.args[kernel_arg]:
      _logger.debug('%s is constrained to %s because of %s', arg.instance.name,
                    region, arg.name)
      config['OptionalFloorplan'][region].append(arg.instance.name)
      if arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
        config['OptionalFloorplan'][region].append(
            rtl.async_mmap_instance_name(arg.mmap_name))

  # Dedup items.
  for region, instances in config['OptionalFloorplan'].items():
    instances[:] = list(dict.fromkeys(instances))

  # ASYNC_MMAP is outside of its instantiating module and needs its own
  # floorplanning constraint.
  for instance in top_task.instances:
    for arg in instance.args:
      if arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
        floorplan_vertex_name = rtl.async_mmap_instance_name(arg.mmap_name)
        vertices[floorplan_vertex_name] = floorplan_vertex_name
        area[floorplan_vertex_name] = {
            'BRAM': 0,
            'DSP': 0,
            'FF': 0,
            'LUT': 2000,
            'URAM': 0,
        }

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
      }

  # Generate edges for FIFOs instantiated in the top task.
  for fifo_name, fifo in top_task.fifos.items():
    edges[rtl.sanitize_array_name(fifo_name)] = {
        'produced_by': util.get_instance_name(fifo['produced_by']),
        'consumed_by': util.get_instance_name(fifo['consumed_by']),
        'width': fifo_width_getter(top_task, fifo_name),
    }

  # Dump the input to AutoBridge.
  with open(os.path.join(work_dir, 'autobridge_config.json'), 'w') as fp:
    json.dump(config, fp, indent=2)

  # Generate floorplan using AutoBridge.
  floorplan = autobridge.generate_constraints(config)

  # Dump the output from AutoBridge.
  with open(os.path.join(work_dir, 'autobridge_floorplan.json'), 'w') as fp:
    json.dump(floorplan, fp, indent=2)

  return floorplan


def make_autobridge_area(area: Dict[str, int]) -> Dict[str, int]:
  return {{'BRAM_18K': 'BRAM'}.get(k, k): v for k, v in area.items()}


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


def get_port_region(part_num: str, port: str) -> str:
  bra = port.find('[')
  ket = port.find(']')
  port_cat = port[:bra]
  port_id = int(port[bra + 1:ket])
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
