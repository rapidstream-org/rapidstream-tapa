import logging
import re
from typing import Callable, Dict, List

from tapa import util
from tapa.hardware import (
    get_async_mmap_area,
    get_hbm_controller_area,
    get_zero_area,
)
from tapa.task import Task
from tapa.verilog import xilinx as rtl
from tapa.verilog.xilinx.async_mmap import (
    ADDR_CHANNEL_DATA_WIDTH,
    RESP_CHANNEL_DATA_WIDTH,
)

# TODO: resolve cyclic dependency
from .instance import Instance

_logger = logging.getLogger().getChild(__name__)


def get_fifo_edges(
    top_task: Task,
    fifo_width_getter: Callable[[Task, str], int],
):
  """
  get the edges corresponding to stream FIFOs in the tapa code
  """
  fifo_edges = {}
  # Generate edges for FIFOs instantiated in the top task.
  for fifo_name, fifo in top_task.fifos.items():
    fifo_edges[rtl.sanitize_array_name(fifo_name)] = {
        'produced_by': 'TASK_VERTEX_' + util.get_instance_name(fifo['produced_by']),
        'consumed_by': 'TASK_VERTEX_' + util.get_instance_name(fifo['consumed_by']),
        'width': fifo_width_getter(top_task, fifo_name),
        'depth': top_task.fifos[fifo_name]['depth'],
        'instance': rtl.sanitize_array_name(fifo_name),
        'category': 'FIFO_EDGE',
    }

  return type_marked(fifo_edges, 'FIFO_EDGE')


def get_axi_edges(
    top_task: Task,
    read_only_args: List[str],
    write_only_args: List[str],
):
  """
  get the edges corresponding to the AXI links between tasks and DDR/HBM ports
  # FIXME: we are messing up between Arg and Port
  """
  axi_edges = {}
  port_name_to_width = get_port_name_to_width(top_task)

  for arg_list in top_task.args.values():
    for arg in arg_list:
      if arg.cat in {Instance.Arg.Cat.ASYNC_MMAP, Instance.Arg.Cat.MMAP}:
        # total width of an AXI channel
        # wr_data, wr_addr, rd_data, rd_addr, resp + other control signals (approximately 50)
        arg_width = port_name_to_width[arg.name]

        if any(re.match(ro, arg.name) for ro in read_only_args):
          _logger.info('%s is specified as read-only', arg.name)
          total_connection_width = arg_width + ADDR_CHANNEL_DATA_WIDTH + 50
        elif any(re.match(wo, arg.name) for wo in write_only_args):
          _logger.info('%s is specified as write-only', arg.name)
          total_connection_width = arg_width + ADDR_CHANNEL_DATA_WIDTH + RESP_CHANNEL_DATA_WIDTH + 50
        else:
          _logger.warning('%s is assumed to be both read from and written to. '
                       'If not, please use --read-only-args or --write-only-args '
                       'for better optimization results.' , arg.name)
          total_connection_width = arg_width + arg_width + ADDR_CHANNEL_DATA_WIDTH + ADDR_CHANNEL_DATA_WIDTH + RESP_CHANNEL_DATA_WIDTH + 50

        # the edge represents the AXI pipeline between the DDR/HBM IP and the mmap module
        axi_edges[get_axi_edge_name(arg)] = {
          'produced_by': 'PORT_VERTEX_' + get_physical_port_vertex_name(arg.name),
          'consumed_by': 'TASK_VERTEX_' + arg.instance.name,
          'width': total_connection_width,
          'depth': 0,
          'category': 'AXI_EDGE',
          'port_name': arg.name,
        }

  return type_marked(axi_edges, 'AXI_EDGE')


def get_async_mmap_edges(
    top_task,
):
  """
  In fact the async_mmap are directly wired to the task that uses it.
  Therefore we create edges of infinite width so that they will be
  binded together by the solver
  """
  async_mmap_edges = {}
  for arg_name, arg_list in top_task.args.items():
    for arg in arg_list:
      if arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
        e_name = f'async_mmap_infinite_from_{rtl.async_mmap_instance_name(arg.mmap_name)}_to_{arg.instance.name}'
        async_mmap_edges[e_name] = {
          'produced_by': 'ASYNC_MMAP_VERTEX_' + rtl.async_mmap_instance_name(arg.mmap_name),
          'consumed_by': 'TASK_VERTEX_' + arg.instance.name,
          'width': 1000000,
          'depth': 0,
          'category': 'ASYNC_MMAP_EDGE',
        }

  return type_marked(async_mmap_edges, 'ASYNC_MMAP_EDGE')


def get_edges(
    top_task: Task,
    fifo_width_getter: Callable[[Task, str], int],
    read_only_args: List[str],
    write_only_args: List[str],
):
  all_edges = {}
  all_edges.update(get_fifo_edges(top_task, fifo_width_getter))
  all_edges.update(get_axi_edges(top_task, read_only_args, write_only_args))
  all_edges.update(get_async_mmap_edges(top_task))

  return all_edges


##########################################################################


def get_task_vertices(top_task):
  task_vertices = {}

  for inst in top_task.instances:
    # Total area handles hierarchical designs for AutoBridge.
    task_area = make_autobridge_area(inst.task.total_area)
    task_vertices[inst.name] = {
      'module': inst.task.name,
      'instance': inst.name,
      'area':task_area,
      'category': 'TASK_VERTEX',
    }
    _logger.debug('area of %s: %s', inst.task.name, task_area)

  return type_marked(task_vertices, 'TASK_VERTEX')


def get_ctrl_vertices(top_task):
  # Self area for top task represents mainly the control_s_axi instance.
  ctrl_vertices = {rtl.ctrl_instance_name(top_task.name):
    {
      'module': top_task.name,
      'instance': rtl.ctrl_instance_name(top_task.name),
      'area': make_autobridge_area(top_task.self_area),
      'category': 'CTRL_VERTEX',
    }
  }
  return type_marked(ctrl_vertices, 'CTRL_VERTEX')


def get_async_mmap_vertices(top_task):
  """
  unlike mmap which will be merged with the task taking it
  async_mmap will be generated as an individual instance
  """
  async_mmap_vertices = {}

  port_name_to_width = get_port_name_to_width(top_task)
  for instance in top_task.instances:
    for arg in instance.args:
      if arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
        v_name = rtl.async_mmap_instance_name(arg.mmap_name)
        data_channel_width = max(port_name_to_width[arg.name], 32)
        v_area = get_async_mmap_area(data_channel_width)
        async_mmap_vertices[v_name] = {
          'module': 'async_mmap',
          'instance': v_name,
          'area': v_area,
          'category': 'ASYNC_MMAP_VERTEX',
        }

  return type_marked(async_mmap_vertices, 'ASYNC_MMAP_VERTEX')


def get_port_vertices(
    arg_name_to_external_port,
):
  """
  FIXME: currently assume the DDR/HBM channels are 512 bit wide
  """
  port_vertices = {}

  for arg_name, external_port_name in arg_name_to_external_port.items():
    port_cat, port_id = util.parse_port(external_port_name)
    if port_cat == 'HBM':
      area = get_hbm_controller_area()
    # the ddr will not overlap with user logic
    elif port_cat == 'DDR':
      area = get_zero_area()
    elif port_cat == 'PLRAM':
      area = get_zero_area()
    else:
      raise NotImplementedError(f'unrecognized port type {port_cat}')

    port_vertices[get_physical_port_vertex_name(arg_name)] = {
      'module': f'external_{port_cat}_controller',
      'top_arg_name': arg_name,
      'area': area,
      'category': 'PORT_VERTEX',
      'port_cat': port_cat,
      'port_id': port_id,
    }

  return type_marked(port_vertices, 'PORT_VERTEX')


def get_vertices(top_task: Task, arg_name_to_external_port: Dict[str, str]):
  all_vertices = {}
  all_vertices.update(get_task_vertices(top_task))
  all_vertices.update(get_async_mmap_vertices(top_task))
  all_vertices.update(get_port_vertices(arg_name_to_external_port))

  return all_vertices


#################################################################################


def type_marked(name_to_obj, obj_type):
  return {f'{obj_type}_{name}': obj for name, obj in name_to_obj.items()}

def get_physical_port_vertex_name(arg_name):
  return f'{arg_name}_external_controller'

def get_axi_edge_name(port_arg):
  port_vertex = get_physical_port_vertex_name(port_arg.name)
  return f'axi_edge_from_{port_vertex}_to_{port_arg.instance.name}'

def get_port_name_to_width(top_task):
  """
  FIXME: it is very confusing here. we are using "ports" to get the width table
  but we are using "args" to look up the table
  """
  return {port.name: port.width for port in top_task.ports.values()}

def make_autobridge_area(area: Dict[str, int]) -> Dict[str, int]:
  return {{'BRAM_18K': 'BRAM'}.get(k, k): v for k, v in area.items()}
