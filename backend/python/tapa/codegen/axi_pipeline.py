from typing import List

from pyverilog.vparser.ast import Input, Output, Parameter
from pyverilog.ast_code_generator.codegen import ASTCodeGenerator

from tapa.verilog.xilinx.m_axi import M_AXI_SUFFIXES_COMPACT
from tapa.instance import Instance
from tapa.util import get_max_addr_width


class AXI:
  def __init__(self, name, data_width, addr_width, pipeline_level):
    self.name = name
    self.data_width = data_width
    self.addr_width = addr_width
    self.pipeline_level = pipeline_level

class IOPort:
  def __init__(self, name: str, direction: str, width: str):
    self.name = name
    self.direction = direction
    self.width = width


def _dfs(node, filter_func):
  if filter_func(node):
    yield node
  for c in node.children():
    yield from _dfs(c, filter_func)


def indented(code: List[str]) -> List[str]:
  return [f'  {line}' for line in code]


def parse_ports(ast) -> List[IOPort]:

  io_list = []

  is_input = lambda node: isinstance(node, Input)
  is_output = lambda node: isinstance(node, Output)
  is_io = lambda node: is_input(node) or is_output(node)

  codegen = ASTCodeGenerator()
  for node in _dfs(ast, is_io):
    if is_input(node):
      direction = 'input'
    else:
      direction = 'output'
    width = codegen.visit(node.width) if node.width else ''
    io_list.append(IOPort(node.name, direction, width))

  return io_list


def get_top(top_name, io_list: List[IOPort]):
  top = []

  top.append(f'`timescale 1 ns / 1 ps ')
  top.append(f'module {top_name} (')
  for io in io_list:
    top.append(f'  {io.direction} {io.width} {io.name},')
  top[-1] = top[-1].replace(',', '')
  top.append(f');')

  return top


def get_params(ast) -> List[str]:
  params = []

  codegen = ASTCodeGenerator()
  for node in _dfs(ast, lambda node: isinstance(node, Parameter)):
    params.append(codegen.visit(node))

  return indented(params)


def get_wire_decl(io_list: List[IOPort]):
  wire_decl = []
  for io in io_list:
    if 'm_axi' in io.name:
      wire_decl.append(f'wire {io.width} {io.name}_inner;')

  return indented(wire_decl)


def get_pipeline_inst(axi_list: List[AXI]):
  pp_inst = []
  for axi in axi_list:
    pp_inst.append(f'axi_pipeline #(')
    pp_inst.append(f'  .C_M_AXI_ADDR_WIDTH({axi.addr_width}),')
    pp_inst.append(f'  .C_M_AXI_DATA_WIDTH({axi.data_width}),')
    pp_inst.append(f'  .PIPELINE_LEVEL    ({axi.pipeline_level})') # FIXME: adjust based on floorplanning
    pp_inst.append(f') {axi.name} (')

    for axi_port in M_AXI_SUFFIXES_COMPACT:
      pp_inst.append(f'  .out{axi_port}  (m_axi_{axi.name}{axi_port}   ),')

    for axi_port in M_AXI_SUFFIXES_COMPACT:
      pp_inst.append(f'  .in{axi_port}  (m_axi_{axi.name}{axi_port}_inner  ),')

    pp_inst.append(f'  .ap_clk      (ap_clk)')
    pp_inst.append(f');')

  return indented(pp_inst)


def get_top_inst(top_name, io_list: List[IOPort]):
  top_inst = []

  top_inst.append(f'{top_name} {top_name}_0 (')

  for io in io_list:
    if 'm_axi' in io.name:
      top_inst.append(f'  .{io.name}({io.name}_inner),')
    else:
      top_inst.append(f'  .{io.name}({io.name}),')

  top_inst[-1] = top_inst[-1].replace(',', '')
  top_inst.append(f');')
  return indented(top_inst)


def get_end():
  return ['endmodule']


def get_axi_pipeline_wrapper(orig_top_name: str, top_name_suffix: str, top_task):
  """
  Given the original top RTL module
  Generate a wrapper that (1) instantiate the previous top module
  (2) pipeline all AXI interfaces
  """
  # the address space may not start from 0
  addr_width = 64

  ast = top_task.module.ast
  axi_list = []
  for port in top_task.ports.values():
    if port.cat in {Instance.Arg.Cat.MMAP, Instance.Arg.Cat.ASYNC_MMAP}:
      pipeline_level = top_task.module.get_axi_pipeline_level(port.name)
      axi_list.append(AXI(port.name, port.width, addr_width, pipeline_level))

  io_list = parse_ports(ast)

  wrapper = []
  wrapper += get_top(orig_top_name, io_list) + ['\n\n']
  wrapper += get_params(ast)
  wrapper += get_wire_decl(io_list) + ['\n\n']
  wrapper += get_pipeline_inst(axi_list) + ['\n\n']
  wrapper += get_top_inst(f'{orig_top_name}{top_name_suffix}', io_list) + ['\n\n']
  wrapper += get_end()

  return '\n'.join(wrapper)
