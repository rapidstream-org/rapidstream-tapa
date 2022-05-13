import itertools
import json
import logging
import os
import sys
from collections import defaultdict
from concurrent import futures
from typing import Optional, Dict, Callable, List, Tuple, TextIO

from autobridge.main import annotate_floorplan
from haoda.report.xilinx import rtl as report

from tapa import util
from tapa.hardware import (
  get_ctrl_instance_region,
  get_port_region,
  get_slr_count,
)
from tapa.task import Task
from tapa.task_graph import get_edges, get_vertices

_logger = logging.getLogger().getChild(__name__)

class InputError(Exception):
  pass


def generate_floorplan(
    part_num: str,
    physical_connectivity: TextIO,
    read_only_args: List[str],
    write_only_args: List[str],
    user_floorplan_pre_assignments: Optional[TextIO],
    autobridge_dir: str,
    top_task: Task,
    fifo_width_getter: Callable[[Task, str], int],
    **kwargs,
) -> Tuple[Dict, Dict]:
  """
  get the target region of each vertex
  get the pipeline level of each edge
  """
  config = get_floorplan_config(
    autobridge_dir,
    part_num,
    physical_connectivity,
    top_task,
    fifo_width_getter,
    user_floorplan_pre_assignments,
    read_only_args,
    write_only_args,
    **kwargs,
  )

  config_with_floorplan = annotate_floorplan(config)

  return config, config_with_floorplan


def get_floorplan_result(
    autobridge_dir: str,
    constraint: TextIO,
) -> Tuple[Dict[str, str], Dict[str, int], Dict[str, int], Dict[str, int]]:
  """ extract floorplan results from the checkpointed config file """
  try:
    config_with_floorplan = json.loads(open(f'{autobridge_dir}/post-floorplan-config.json', 'r').read())
  except:
    raise FileNotFoundError(f'no valid floorplanning results found in work directory {autobridge_dir}')

  # generate the constraint file
  vivado_tcl = get_vivado_tcl(config_with_floorplan)
  constraint.write('\n'.join(vivado_tcl))
  _logger.info('generate the floorplan constraint at %s', constraint.name)

  fifo_pipeline_level, axi_pipeline_level = extract_pipeline_level(config_with_floorplan)
  task_inst_to_slr = extract_task_locations(config_with_floorplan)
  fifo_to_depth = extract_fifo_depth(config_with_floorplan)

  return fifo_pipeline_level, axi_pipeline_level, task_inst_to_slr, fifo_to_depth

def extract_fifo_depth(config_with_floorplan) -> Dict[str, int]:
  """Get the adjusted FIFO depth"""
  fifo_to_depth = {}
  if config_with_floorplan.get('floorplan_status') == 'FAILED':
    return fifo_to_depth

  for e_name, props in config_with_floorplan['edges'].items():
    # skip axi edges
    if 'instance' in props:
      if 'adjusted_depth' not in props:
        _logger.warning('No latency balancing information found, skip FIFO depth adjusting')
        break

      fifo_to_depth[props['instance']] = props['adjusted_depth']

  return fifo_to_depth

def extract_task_locations(config_with_floorplan) -> Dict[str, int]:
  task_inst_to_slr = {}
  if config_with_floorplan.get('floorplan_status') == 'FAILED':
    return task_inst_to_slr

  for v_name, props in config_with_floorplan['vertices'].items():
    # pseudo vertices don't have instances
    if 'instance' in props:
      task_inst_to_slr[props['instance']] = props['SLR']
  return task_inst_to_slr


def extract_pipeline_level(
  config_with_floorplan,
) -> Tuple[Dict[str, str], Dict[str, int]]:
  """ extract the pipeline level of fifos and axi edges """
  if config_with_floorplan.get('floorplan_status') == 'FAILED':
    return {}, {}

  fifo_pipeline_level = {}
  for edge, properties in config_with_floorplan['edges'].items():
    if properties['category'] == 'FIFO_EDGE':
      fifo_pipeline_level[properties['instance']] = len(properties['path'])

  axi_pipeline_level = {}
  for edge, properties in config_with_floorplan['edges'].items():
    if properties['category'] == 'AXI_EDGE':
      # if the AXI module is at the same region as the external port, then no pipelining
      # sync with get_vivado_tcl
      level = len(properties['path']) - 1

      axi_pipeline_level[properties['port_name']] = level

  return fifo_pipeline_level, axi_pipeline_level


def _get_axi_pipeline_tcl(config_with_floorplan) -> List[str]:
  vivado_tcl = []
  region_to_axi_pipeline_inst = defaultdict(list)

  for edge, properties in config_with_floorplan['edges'].items():
    if properties['category'] == 'AXI_EDGE':
      port_name = properties['port_name']
      path = properties['path']
      pipeline_level = len(path) - 1  # sync with extract_pipeline_level

      if pipeline_level:
        # R and B channels go from AXI port to the task instance
        for i in range(pipeline_level):
          for ch in ('R_pipeline', 'B_pipeline'):
            region = path[i+1]
            pipeline_reg_id = i
            region_to_axi_pipeline_inst[region].append(f'{port_name}/{ch}/inst\\\\[{pipeline_reg_id}]\\\\.unit')

        # AR, AW, W channels go from the task instance to the AXI port
        reversed_path = [e for e in reversed(path)]
        for i in range(pipeline_level):
          for ch in ('AR_pipeline', 'AW_pipeline', 'W_pipeline'):
            region = reversed_path[i+1]
            pipeline_reg_id = i
            region_to_axi_pipeline_inst[region].append(f'{port_name}/{ch}/inst\\\\[{pipeline_reg_id}]\\\\.unit')

  for region, inst_list in region_to_axi_pipeline_inst.items():
    vivado_tcl.append(f'add_cells_to_pblock [get_pblocks {region}] [get_cells -regex {{')
    vivado_tcl += [f'  pfm_top_i/dynamic_region/.*/inst/{inst}' for inst in inst_list]
    vivado_tcl.append(f'}} ]')

  return vivado_tcl


def get_vivado_tcl(config_with_floorplan):
  if config_with_floorplan.get('floorplan_status') == 'FAILED':
    return ['# Floorplan failed']

  vivado_tcl = []

  vivado_tcl.append('puts "applying partitioning constraints generated by tapac"')
  vivado_tcl.append('write_checkpoint before_add_floorplan_constraints.dcp')

  # pblock definitions
  vivado_tcl += list(config_with_floorplan['floorplan_region_pblock_tcl'].values())

  region_to_inst = defaultdict(list)

  # floorplan the control_s_axi duplicates
  num_copy = get_slr_count(config_with_floorplan['part_num'])
  for i in range(num_copy):
    region_to_inst[f'pblock_dynamic_SLR{i}'].append(f'control_s_axi_U_slr_{i}')

  # tapa central FSM
  region_to_inst[f'pblock_dynamic_SLR0'].append('tapa_state.*')

  # floorplan vertices
  for vertex, properties in config_with_floorplan['vertices'].items():
    if properties['category'] == 'PORT_VERTEX':
      continue
    region = properties['floorplan_region']
    inst = properties['instance']

    # floorplan the task instances
    region_to_inst[region].append(inst)

    # floorplan some control signals
    region_to_inst[region].append(f'{inst}__state.*')

  # floorplan pipeline registers
  for edge, properties in config_with_floorplan['edges'].items():
    if properties['category'] == 'FIFO_EDGE':
      fifo_name = properties['instance']
      path = properties['path']
      if len(path) > 1:
        for i in range(len(path)):
          region_to_inst[path[i]].append(f'{fifo_name}/inst\\\\[{i}]\\\\.unit')
      else:
        region_to_inst[path[0]].append(f'{fifo_name}/.*.unit')

  # print out the constraints
  for region, inst_list in region_to_inst.items():
    vivado_tcl.append(f'add_cells_to_pblock [get_pblocks {region}] [get_cells -regex {{')
    # note the .* after inst
    # this is because we add a wrapper around user logic for AXI pipelining
    vivado_tcl += [f'  pfm_top_i/dynamic_region/.*/inst/.*/{inst}' for inst in inst_list]
    vivado_tcl.append(f'}} ]')

  # floorplan the axi pipelines
  vivado_tcl += _get_axi_pipeline_tcl(config_with_floorplan)

  # redundant clean up code for extra safety
  vivado_tcl.append('foreach pblock [get_pblocks -regexp CR_X\\\\d+Y\\\\d+_To_CR_X\\\\d+Y\\\\d+] {')
  vivado_tcl.append('  if {[get_property CELL_COUNT $pblock] == 0} {')
  vivado_tcl.append('    puts "WARNING: delete empty pblock $pblock "')
  vivado_tcl.append('    delete_pblocks $pblock')
  vivado_tcl.append('  }')
  vivado_tcl.append('}')

  vivado_tcl.append('foreach pblock [get_pblocks] {')
  vivado_tcl.append(f'  report_utilization -pblocks $pblock')
  vivado_tcl.append('}',)

  return vivado_tcl


def checkpoint_floorplan(config_with_floorplan, autobridge_dir):
  """ Save a copy of the region -> instances into a json file
  """
  if config_with_floorplan.get('floorplan_status') == 'FAILED':
    _logger.warning('failed to get a valid floorplanning')
    return

  region_to_inst = defaultdict(list)
  for vertex, properties in config_with_floorplan['vertices'].items():
    if properties['category'] == 'PORT_VERTEX':
      continue
    region = properties['floorplan_region']
    region_to_inst[region].append(vertex)

  open(f'{autobridge_dir}/floorplan-region-to-instances.json', 'w').write(
    json.dumps(region_to_inst, indent=2)
  )


def generate_new_connectivity_ini(config_with_floorplan, work_dir, top_name):
  """If the HBM binding is adjusted, generate the new binding configuration"""
  if config_with_floorplan.get('enable_hbm_binding_adjustment', False):
    new_binding = config_with_floorplan.get('new_hbm_binding', {})
    if not new_binding:
      _logger.warning('Adjusted HBM binding not recorded in post-floorplan config')
      return(1)

    cfg = []
    cfg += ['[connectivity]']
    for arg_name, port_id in new_binding.items():
      cfg += [f'sp={top_name}.{arg_name}:HBM[{port_id}]']

    cfg_path = f'{work_dir}/link_config.ini'
    _logger.info('generate new port binding at %s', cfg_path)
    open(cfg_path, 'w').write('\n'.join(cfg))


def get_floorplan_config(
    autobridge_dir: str,
    part_num: str,
    physical_connectivity: TextIO,
    top_task: Task,
    fifo_width_getter: Callable[[Task, str], int],
    user_floorplan_pre_assignments: Optional[TextIO],
    read_only_args: List[str],
    write_only_args: List[str],
    **kwargs,
) -> Dict:
  """ Generate a json encoding the task graph for the floorplanner
  """
  arg_name_to_external_port = util.parse_connectivity_and_check_completeness(
                                physical_connectivity,
                                top_task,
                              )

  edges = get_edges(top_task, fifo_width_getter, read_only_args, write_only_args)
  vertices = get_vertices(top_task, arg_name_to_external_port)
  floorplan_pre_assignments = get_floorplan_pre_assignments(
                                part_num,
                                user_floorplan_pre_assignments,
                                vertices,
                                **kwargs,
                              )

  grouping_constraints = get_grouping_constraints(edges)

  config = {
    'work_dir': autobridge_dir,
    'part_num': part_num,
    'edges': edges,
    'vertices': vertices,
    'floorplan_pre_assignments': floorplan_pre_assignments,
    'grouping_constraints': grouping_constraints,
  }

  config.update(kwargs)

  return config


def get_grouping_constraints(edges: Dict) -> List[List[str]]:
  """ specify which tasks must be placed in the same slot """
  grouping = []
  for edge, properties in edges.items():
    if properties['category'] == 'ASYNC_MMAP_EDGE':
      grouping.append([properties['produced_by'], properties['consumed_by']])
  return grouping


def get_floorplan_pre_assignments(
    part_num: str,
    user_floorplan_pre_assignments_io: Optional[TextIO],
    vertices: Dict[str, Dict],
    **kwargs: Dict,
) -> Dict[str, List[str]]:
  """ constraints of which modules must be assigned to which slots
      including user pre-assignments and system ones
      the s_axi_constrol is always assigned to the bottom left slot in U250/U280
  """
  floorplan_pre_assignments = defaultdict(list)

  # user pre assignment
  if user_floorplan_pre_assignments_io:
    user_pre_assignments = json.load(user_floorplan_pre_assignments_io)
    for region, modules in user_pre_assignments.items():
      floorplan_pre_assignments[region] += modules

  # port vertices pre assignment
  skip_pre_assign_hbm_ports = kwargs.get('enable_hbm_binding_adjustment', False)
  for v_name, properties in vertices.items():
    if properties['category'] == 'PORT_VERTEX':
      if skip_pre_assign_hbm_ports and properties['port_cat'] == 'HBM':
        continue
      region = get_port_region(part_num, properties['port_cat'], properties['port_id'])
      floorplan_pre_assignments[region].append(v_name)

  return floorplan_pre_assignments


def get_post_synth_area(
    rtl_dir,
    part_num,
    top_task,
    post_syn_rpt_getter: Callable[[str], str],
    task_getter: Callable[[str], Task],
    cpp_getter: Callable[[str], str],
    max_parallel_synth_jobs: int,
):
  def worker(module_name: str, idx: int) -> report.HierarchicalUtilization:
    _logger.debug('synthesizing %s', module_name)
    rpt_path = post_syn_rpt_getter(module_name)

    rpt_path_mtime = 0.
    if os.path.isfile(rpt_path):
      rpt_path_mtime = os.path.getmtime(rpt_path)

    # generate report if and only if C++ source is newer than report.
    if os.path.getmtime(cpp_getter(module_name)) > rpt_path_mtime:
      os.nice(idx % 19)
      with report.ReportDirUtil(
          rtl_dir,
          rpt_path,
          module_name,
          part_num,
          synth_kwargs={'mode': 'out_of_context'},
      ) as proc:
        stdout, stderr = proc.communicate()

      # err if output report does not exist or is not newer than previous
      if (not os.path.isfile(rpt_path) or
          os.path.getmtime(rpt_path) <= rpt_path_mtime):
        sys.stdout.write(stdout.decode('utf-8'))
        sys.stderr.write(stderr.decode('utf-8'))
        raise InputError(f'failed to generate report for {module_name}')

    with open(rpt_path) as rpt_file:
      return report.parse_hierarchical_utilization_report(rpt_file)

  _logger.info('generating post-synthesis resource utilization reports')
  _logger.info('this step runs logic synthesis of each task for accurate area info, it may take a while')
  worker_num = min(max_parallel_synth_jobs, util.nproc())
  _logger.info('spawn %d workers for parallel logic synthesis. '
               'use --max-parallel-synth-jobs to enable more workers.', worker_num)
  with futures.ThreadPoolExecutor(max_workers=worker_num) as executor:
    for utilization in executor.map(
        worker,
        {x.task.name for x in top_task.instances},
        itertools.count(0),
    ):
      # override self_area populated from HLS report
      bram = int(utilization['RAMB36']) * 2 + int(utilization['RAMB18'])
      task_getter(utilization.instance).total_area = {
          'BRAM_18K': bram,
          'DSP': int(utilization['DSP Blocks']),
          'FF': int(utilization['FFs']),
          'LUT': int(utilization['Total LUTs']),
          'URAM': int(utilization['URAM']),
      }
