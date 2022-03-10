import itertools
import logging
import os
import sys
from concurrent import futures
from typing import Optional, Dict, Any, Callable, List, Set, Tuple, TextIO

import tapa.autobridge
from haoda.report.xilinx import rtl as report
from tapa import util
from tapa.task import Task
from tapa.verilog import xilinx as rtl

from .instance import Instance

_logger = logging.getLogger().getChild(__name__)

class InputError(Exception):
  pass


def get_floorplan(
    directive: Optional[Dict[str, Any]],
    enable_synth_util: bool,
    floorplan_pre_assignments: Optional[TextIO],
    work_dir: str,
    rtl_dir: str,
    top_task: Task,
    ctrl_instance_name: str,
    post_syn_rpt_getter: Callable[[str], str],
    task_getter: Callable[[str], Task],
    fifo_width_getter: Callable[[Task, str], int],
    cpp_getter: Callable[[str], str],
    **kwargs,
) -> Tuple[Dict[str, str], Dict[str, int], List[str]]:
  """
  get the target region of each vertex
  get the pipeline level of each edge
  TODO: remove the "directive" input, which is confusing
  Repalce it by the "floorplan_pre_assignments" option
  """
  # run logic synthesis to get an accurate area estimation
  if enable_synth_util:
    get_post_synth_area(
      rtl_dir,
      directive,
      top_task,
      post_syn_rpt_getter,
      task_getter,
      cpp_getter,
    )

  _logger.info('generating partitioning constraints')
  vertex_to_slot, slot_to_vertices, topology = tapa.autobridge.generate_floorplan(
      top_task,
      work_dir,
      fifo_width_getter,
      directive['connectivity'],
      directive['part_num'],
      floorplan_pre_assignments,
      **kwargs,
  )

  # extract the mapping from vertice to region
  # inst_to_region = _get_instance_to_region(floorplan)
  check_floorplan_validity(top_task, ctrl_instance_name, vertex_to_slot, slot_to_vertices)

  # extract the pblock definition tcl for each region
  tcl_definition_of_regions = _get_pblock_tcl(topology)

  # determine partitioning of each FIFO, assign each pipeline register to a region
  fifo_pipeline_level, fifo_pipeline_reg_to_region = get_fifo_pipelines(
    top_task, vertex_to_slot, topology)
  inst_to_region = {**vertex_to_slot, **fifo_pipeline_reg_to_region}

  return inst_to_region, fifo_pipeline_level, tcl_definition_of_regions


def get_post_synth_area(
    rtl_dir,
    directive,
    top_task,
    post_syn_rpt_getter: Callable[[str], str],
    task_getter: Callable[[str], Task],
    cpp_getter: Callable[[str], str],
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
          directive['part_num'],
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
  with futures.ThreadPoolExecutor(max_workers=os.cpu_count()) as executor:
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


def _get_pblock_tcl(topoogy):
  tcl = []
  for properties in topoogy.values():
    tcl.append(properties['tcl'])
  return tcl


def _get_instance_to_region(
  directive,
) -> Dict[str, str]:
  instance_dict = {}
  for region, instances in directive.items():
    for instance_obj in instances:
      if isinstance(instance_obj, str):
        instance_obj = rtl.sanitize_array_name(instance_obj)
        instance_dict[instance_obj] = region

  return instance_dict


def _get_topology(
  directive,
) -> Dict[str, Dict[str, List[str]]]:
  topology = {}
  for region, instances in directive.items():
    for instance_obj in instances:
      if not isinstance(instance_obj, str):
        topology[region] = instance_obj

  return topology


def get_fifo_pipelines(
  top_task,
  instance_dict,
  topology,
) -> Tuple[Dict[str, int], Dict[str, str]]:
  fifo_partition_count: Dict[str, int] = {}
  fifo_pipeline_reg_to_region: Dict[str, str] = {}

  for name, meta in top_task.fifos.items():
    name = rtl.sanitize_array_name(name)
    producer_region = instance_dict[util.get_instance_name(
        meta['produced_by'])]
    consumer_region = instance_dict[util.get_instance_name(
        meta['consumed_by'])]

    # the src and dst are at the same region, no pipelining
    if producer_region == consumer_region:
      fifo_partition_count[name] = 1
      fifo_pipeline_reg_to_region[name] = producer_region

    # the src and dst are at different regions
    else:
      if producer_region not in topology:
        raise InputError(f'missing topology info for {producer_region}')
      if consumer_region not in topology[producer_region]:
        raise InputError(f'{consumer_region} is not reachable from '
                          f'{producer_region}')

      idx = 0  # makes linter happy
      # TODO: move this side effect out of the block
      for idx, region in enumerate((
          producer_region,
          *topology[producer_region][consumer_region],
          consumer_region,
      )):
        fifo_pipeline_reg_to_region[rtl.fifo_partition_name(name, idx)] = region
      fifo_partition_count[name] = idx + 1

  return fifo_partition_count, fifo_pipeline_reg_to_region


def check_floorplan_validity(
  top_task,
  ctrl_instance_name,
  instance_dict,
  directive,
) -> None:
  # check if any instance is missing
  all_instances = {ctrl_instance_name: ''}
  for instance in top_task.instances:
    for arg in instance.args:
      if arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
        all_instances[rtl.async_mmap_instance_name(arg.mmap_name)] = ''
    all_instances[instance.name] = ''

  program_instance_set = set(all_instances)
  floorplanned_instance_set = set(instance_dict.keys())
  missing_instances = program_instance_set - floorplanned_instance_set
  if missing_instances:
    raise InputError('missing region assignment: {%s}' %
                      ', '.join(missing_instances))

  # check if any instance is assigned more than once
  directive_instance_set: Set[str] = set()
  for region, instances in directive.items():
    subset: Set[str] = set()
    for instance_obj in instances:
      if isinstance(instance_obj, str):
        instance_obj = rtl.sanitize_array_name(instance_obj)
        instance_dict[instance_obj] = region
        subset.add(instance_obj)
    intersect = subset & directive_instance_set
    if intersect:
      raise InputError('more than one region assignment: %s' % intersect)
    directive_instance_set.update(subset)

  unnecessary_instances = (directive_instance_set - program_instance_set -
                            rtl.BUILTIN_INSTANCES)
  if unnecessary_instances:
    raise InputError('unnecessary region assignment: {%s}' %
                      ', '.join(unnecessary_instances))
