import re
from copy import deepcopy
from typing import Iterator, List

from tapa.task import Task
from pyverilog.vparser.ast import (
  InstanceList,
  PortArg,
  Node,
  ModuleDef,
  Decl,
  Wire,
  Input,
  Output,
  Parameter,
  Instance,
  Identifier,
  ParamArg,
)

DONT_TOUCH_PORTS = (
  'CLK',
  'ACLK',
  'ARESET',
  'RESET',
  'ACLK_EN',
  'ap_clk',
  'ap_rst',
  'ap_rst_n'
)

S_AXI_AW_AND_W_PORTS = (
  'AWVALID',
  'AWREADY',
  'AWADDR',
  'WVALID',
  'WREADY',
  'WDATA',
  'WSTRB',
)

S_AXI_AR_AND_R_PORTS = (
  'ARVALID',
  'ARREADY',
  'ARADDR',
  'RVALID',
  'RREADY',
  'RDATA',
  'RRESP',
)

S_AXI_B_PORTS = (
  'BVALID',
  'BREADY',
  'BRESP',
)

CONTROL_SIGNALS = (
  'ap_start',
  'interrupt',
  'ap_ready',
  'ap_done',
  'ap_idle',
)


def _get_nodes(node, *target_types) -> Iterator[Node]:
  for target_type in target_types:
    if isinstance(node, target_type):
      yield node
  for c in node.children():
    yield from _get_nodes(c, *target_types)


def get_ctrl_instance(top_task: Task, ctrl_inst_name) -> InstanceList:
  ctrl_instance_list = []
  for inst_list in top_task.module.get_nodes_of_type(InstanceList):
    if inst_list.module == ctrl_inst_name:
      ctrl_instance_list.append(inst_list)

  if len(ctrl_instance_list) != 1:
    raise NotImplementedError('%d control_s_axi instances detected', len(ctrl_instance_list))

  return ctrl_instance_list[0]


def get_updated_ctrl_instance(
    ctrl_instance: InstanceList,
    suffix: str,
    is_root: bool,
) -> InstanceList:
  """Append a suffix to the argname of some PortArgs"""
  inst_list = deepcopy(ctrl_instance)

  # update instance name. inst.name is not an Identifier node but a str
  assert len(inst_list.instances) == 1
  inst_list.instances[0].name += suffix

  if is_root:
    for port_arg in _get_nodes(inst_list, PortArg):
      if any(p == port_arg.portname for p in
          (DONT_TOUCH_PORTS +
          CONTROL_SIGNALS +
          S_AXI_AR_AND_R_PORTS +
          S_AXI_B_PORTS)
      ):
        continue
      else:
        port_arg.argname.name += suffix

  else:
    for port_arg in _get_nodes(inst_list, PortArg):
      if any(re.match(p, port_arg.portname) for p in DONT_TOUCH_PORTS):
        continue
      elif port_arg.portname in S_AXI_AR_AND_R_PORTS:
        if port_arg.portname == 'ARVALID':
          port_arg.argname.name = '1\'b0'
        else:
          port_arg.argname.name = ''
      elif port_arg.portname in S_AXI_B_PORTS:
        if port_arg.portname == 'BREADY':
          port_arg.argname.name = '1\'b1'
        else:
          port_arg.argname.name = ''
      elif port_arg.portname in CONTROL_SIGNALS:
        port_arg.argname.name = ''
      else:
        port_arg.argname.name += suffix

  return inst_list


def get_updated_wire_names(ctrl_instance: InstanceList) -> List[str]:
  """Return the modified wires when updating ctrl instances"""
  updated_wires = []
  for port_arg in _get_nodes(ctrl_instance, PortArg):
    if any(re.match(p, port_arg.portname) for p in
        (DONT_TOUCH_PORTS +
        CONTROL_SIGNALS +
        S_AXI_AR_AND_R_PORTS +
        S_AXI_B_PORTS)
    ):
      continue
    else:
      updated_wires.append(port_arg.argname.name)

  return updated_wires


def get_updated_wire_decls(top_task: Task, updated_wire_names) -> List[Decl]:
  """Based on the names, get the decl nodes from the ast"""
  wire_decls = []
  for decl in top_task.module.get_nodes_of_type(Decl):
    # not include the input/output Decl
    for wire in _get_nodes(decl, Wire):
      if wire.name in updated_wire_names:
        wire_decls.append(deepcopy(decl))

    for io in _get_nodes(decl, Input, Output):
      if io.name in updated_wire_names:
        wire_decls.append(Decl(list=[Wire(**io.__dict__)]))

  return wire_decls


def remove_decls(_curr_ast, updated_wire_names) -> Node:
  """Based on the names, remove the decl nodes from the ast"""
  curr_ast = deepcopy(_curr_ast)
  module_def = get_module_def(curr_ast)

  def filter_func(node):
    if isinstance(node, Decl):
      for wire in _get_nodes(node, Wire):
        if wire.name in updated_wire_names:
          return False

    return True

  module_def.items = tuple(filter(filter_func, module_def.items))

  return curr_ast


def add_updated_wire_decls(top_task, slr_num, updated_wire_names) -> Node:
  """Add new decl nodes to the ast"""
  orig_wire_decls = get_updated_wire_decls(top_task, updated_wire_names)

  for i in range(slr_num):
    suffix = f'_slr_{i}'

    def add_suffix(x):
      x = deepcopy(x)
      for node in _get_nodes(x, Wire):
        node.name += suffix
      return x

    updated_wire_decls = [add_suffix(x) for x in orig_wire_decls]

    top_task.module.add_signals(updated_wire_decls)


def get_module_def(ast: Node) -> ModuleDef:
  """Assume only one ModuleDef in the ast"""
  module_def_list = []
  for item in _get_nodes(ast, ModuleDef):
    module_def_list.append(item)

  if len(module_def_list) != 1:
    raise NotImplementedError('%d modules detected in the same file', len(module_def_list))

  return module_def_list[0]


def remove_ctrl_instance_list(ast: Node) -> Node:
  ast = deepcopy(ast)
  module_def = get_module_def(ast)

  module_def.items = tuple(filter(
    lambda x: not isinstance(x, InstanceList), module_def.items
  ))

  return ast


def get_paramarg_list(top_task: Task) -> List[ParamArg]:
  """The ParamArg for the axi interconnect"""
  param_list = []
  for param in top_task.module.get_nodes_of_type(Parameter):
    param_list.append(ParamArg(
      paramname=param.name,
      argname=Identifier(param.name),
    ))
  return param_list


def get_s_axi_write_broadcastor(top_task, slr_num) -> InstanceList:
  """The interconnect to connect the s_axi_control instances"""
  param_list = get_paramarg_list(top_task)
  portlist = []

  portlist.append(
    PortArg(
      portname='ap_clk',
      argname=Identifier('ap_clk'),
    )
  )

  # ports connecting to each s_axi_control instances
  for i in range(slr_num):
    for axi_port in S_AXI_AW_AND_W_PORTS:
      port_name = f's_axi_control_{axi_port}_slr_{i}'
      portlist.append(
        PortArg(
          portname=port_name,
          argname=Identifier(port_name),
        )
      )

  # ports connecting to the top AXI interface
  for axi_port in S_AXI_AW_AND_W_PORTS:
    port_name = f's_axi_control_{axi_port}'
    portlist.append(
      PortArg(
        portname=port_name,
        argname=Identifier(port_name),
      )
    )

  inst = Instance(
    module=f'a_axi_write_broadcastor_1_to_{slr_num}',
    name='s_axi_write_broadcastor',
    portlist=portlist,
    parameterlist=param_list,
  )

  inst_list = InstanceList(
    module=f'a_axi_write_broadcastor_1_to_{slr_num}',
    parameterlist=param_list,
    instances=[inst],
  )

  return inst_list


def add_instance_list(ast: Node, inst_list: InstanceList) -> Node:
  """Add an instance_list to the ModuleDef in an ast"""
  ast = deepcopy(ast)
  module_def = get_module_def(ast)
  module_def.items = module_def.items + (inst_list,)
  return ast


def duplicate_s_axi_ctrl(top_task: Task, slr_num: int) -> None:
  """Set an s_axi_control for each SLR"""
  ctrl_inst_name = f'{top_task.name}_control_s_axi'
  ctrl_inst: InstanceList = get_ctrl_instance(top_task, ctrl_inst_name)

  # remove the ctrl instance
  top_task.module.del_instances(suffix='_control_s_axi')

  # add updated ctrl instances
  for i in range(slr_num):
    suffix = f'_slr_{i}'
    _ctrl_inst = get_updated_ctrl_instance(ctrl_inst, suffix, i==0)
    top_task.module.add_instancelist(_ctrl_inst)

  # add the updated wires
  updated_wire_names = get_updated_wire_names(ctrl_inst)
  add_updated_wire_decls(top_task, slr_num, updated_wire_names)

  # remove the outdated wires
  for wire_name in updated_wire_names:
    top_task.module.del_signals(prefix=wire_name, suffix=wire_name)

  # add the s_axi write channel interconnect
  interconnect = get_s_axi_write_broadcastor(top_task, slr_num)
  top_task.module.add_instancelist(interconnect)
