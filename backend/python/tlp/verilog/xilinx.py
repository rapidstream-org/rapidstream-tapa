"""Helpers for Xilinx verilog.

This module defines constants and helper functions for verilog files generated
for Xilinx devices.
"""

import collections
import copy
import itertools
import os
import shutil
import sys
import tempfile
from typing import (IO, BinaryIO, Callable, Dict, Iterable, Iterator, Optional,
                    Tuple, Union)

from pyverilog.ast_code_generator import codegen
from pyverilog.vparser import parser

import tlp.instance
from haoda.backend import xilinx as backend
from tlp.verilog import ast

from .util import (REGISTER_LEVEL, Pipeline, match_fifo_name,
                   sanitize_fifo_name, wire_name)

# const strings

RTL_SUFFIX = '.v'

# width=0 means configurable
M_AXI_PORT_WIDTHS = dict(
    ADDR=0,
    BURST=2,
    CACHE=4,
    DATA=0,
    ID=1,
    LAST=1,
    LEN=8,
    LOCK=1,
    PROT=3,
    QOS=4,
    READY=1,
    REGION=4,
    RESP=2,
    SIZE=3,
    STRB=0,
    USER=1,
    VALID=1,
)

# [(name, direction), ...]
M_AXI_ADDR_PORTS = (
    ('ADDR', 'output'),
    ('BURST', 'output'),
    ('CACHE', 'output'),
    ('ID', 'output'),
    ('LEN', 'output'),
    ('LOCK', 'output'),
    ('PROT', 'output'),
    ('QOS', 'output'),
    ('READY', 'input'),
    ('REGION', 'output'),
    ('SIZE', 'output'),
    ('USER', 'output'),
    ('VALID', 'output'),
)

# {channel: [(name, direction), ...]}
M_AXI_PORTS: Dict[str, Tuple[Tuple[str, str], ...]] = collections.OrderedDict(
    AR=M_AXI_ADDR_PORTS,
    AW=M_AXI_ADDR_PORTS,
    B=(
        ('ID', 'input'),
        ('READY', 'output'),
        ('RESP', 'input'),
        ('USER', 'input'),
        ('VALID', 'input'),
    ),
    R=(
        ('DATA', 'input'),
        ('ID', 'input'),
        ('LAST', 'input'),
        ('READY', 'output'),
        ('RESP', 'input'),
        ('USER', 'input'),
        ('VALID', 'input'),
    ),
    W=(
        ('DATA', 'output'),
        ('ID', 'output'),
        ('LAST', 'output'),
        ('READY', 'input'),
        ('STRB', 'output'),
        ('USER', 'output'),
        ('VALID', 'output'),
    ),
)

M_AXI_PREFIX = backend.M_AXI_PREFIX

M_AXI_SUFFIXES = (
    '_ARADDR',
    '_ARBURST',
    '_ARCACHE',
    '_ARID',
    '_ARLEN',
    '_ARLOCK',
    '_ARPROT',
    '_ARQOS',
    '_ARREADY',
    '_ARREGION',
    '_ARSIZE',
    '_ARUSER',
    '_ARVALID',
    '_AWADDR',
    '_AWBURST',
    '_AWCACHE',
    '_AWID',
    '_AWLEN',
    '_AWLOCK',
    '_AWPROT',
    '_AWQOS',
    '_AWREADY',
    '_AWREGION',
    '_AWSIZE',
    '_AWUSER',
    '_AWVALID',
    '_BID',
    '_BREADY',
    '_BRESP',
    '_BUSER',
    '_BVALID',
    '_RDATA',
    '_RID',
    '_RLAST',
    '_RREADY',
    '_RRESP',
    '_RUSER',
    '_RVALID',
    '_WDATA',
    '_WID',
    '_WLAST',
    '_WREADY',
    '_WSTRB',
    '_WUSER',
    '_WVALID',
)

M_AXI_PARAM_PREFIX = 'C_M_AXI_'

M_AXI_PARAM_SUFFIXES = (
    '_ID_WIDTH',
    '_ADDR_WIDTH',
    '_DATA_WIDTH',
    '_AWUSER_WIDTH',
    '_ARUSER_WIDTH',
    '_WUSER_WIDTH',
    '_RUSER_WIDTH',
    '_BUSER_WIDTH',
    '_USER_VALUE',
    '_PROT_VALUE',
    '_CACHE_VALUE',
    '_WSTRB_WIDTH',
)

M_AXI_PARAMS = ('C_M_AXI_DATA_WIDTH', 'C_M_AXI_WSTRB_WIDTH')

ISTREAM_SUFFIXES = (
    '_dout',
    '_empty_n',
    '_read',
)

OSTREAM_SUFFIXES = (
    '_din',
    '_full_n',
    '_write',
)

FIFO_READ_PORTS = (
    'if_dout',
    'if_empty_n',
    'if_read',
    'if_read_ce',
)

FIFO_WRITE_PORTS = (
    'if_din',
    'if_full_n',
    'if_write',
    'if_write_ce',
)

HANDSHAKE_CLK = 'ap_clk'
HANDSHAKE_RST = 'ap_rst_n_inv'
HANDSHAKE_RST_N = 'ap_rst_n'
HANDSHAKE_START = 'ap_start'
HANDSHAKE_DONE = 'ap_done'
HANDSHAKE_IDLE = 'ap_idle'
HANDSHAKE_READY = 'ap_ready'

HANDSHAKE_INPUT_PORTS = (
    HANDSHAKE_CLK,
    HANDSHAKE_RST_N,
    HANDSHAKE_START,
)
HANDSHAKE_OUTPUT_PORTS = (
    HANDSHAKE_DONE,
    HANDSHAKE_IDLE,
    HANDSHAKE_READY,
)

# const ast nodes

START = ast.Identifier(HANDSHAKE_START)
DONE = ast.Identifier(HANDSHAKE_DONE)
IDLE = ast.Identifier(HANDSHAKE_IDLE)
READY = ast.Identifier(HANDSHAKE_READY)
TRUE = ast.IntConst("1'b1")
FALSE = ast.IntConst("1'b0")
SENS_TYPE = 'posedge'
CLK = ast.Identifier(HANDSHAKE_CLK)
RST = ast.Identifier(HANDSHAKE_RST)
RST_N = ast.Identifier(HANDSHAKE_RST_N)
CLK_SENS_LIST = ast.SensList((ast.Sens(CLK, type=SENS_TYPE),))
ALL_SENS_LIST = ast.SensList((ast.Sens(None, type='all'),))
STATE = ast.Identifier('tlp_state')

# type aliases

Directive = Tuple[int, str]
IOPort = Union[ast.Input, ast.Output, ast.Inout]
Signal = Union[ast.Reg, ast.Wire, ast.Pragma]
Logic = Union[ast.Assign, ast.Always, ast.Initial]

# registered ast nodes

START_Q = Pipeline(START.name)
DONE_Q = Pipeline(DONE.name)
IDLE_Q = Pipeline(IDLE.name)
RST_N_Q = Pipeline(RST_N.name)


class Module:
  """AST and helpers for a verilog module.

  Attributes:
    ast: The ast.Source node.
    directives: Tuple of Directives.
    _handshake_output_ports: A mapping from ap_done, ap_idle, ap_ready signal
        names to their ast.Assign nodes.
    _last_io_port_idx: Last index of an IOPort in module_def.items.
    _last_signal_idx: Last index of ast.Wire or ast.Reg in module_def.items.
    _last_param_idx: Last index of ast.Parameter in module_def.items.
    _last_instance_idx: Last index of ast.InstanceList in module_def.items.
    _last_logic_idx: Last index of ast.Assign or ast.Always in module_def.items.
  """

  def __init__(self, files: Iterable[str]):
    """Construct a Module from files. """
    if not files:
      return
    self.ast: ast.Source
    self.directives: Tuple[Directive, ...]
    self.ast, self.directives = parser.parse(files)
    self._handshake_output_ports: Dict[str, ast.Assign] = {}
    self._calculate_indices()

  def _calculate_indices(self) -> None:
    for idx, item in enumerate(self._module_def.items):
      if isinstance(item, ast.Decl):
        if any(
            isinstance(x, (ast.Input, ast.Output, ast.Input))
            for x in item.list):
          self._last_io_port_idx = idx
        elif any(isinstance(x, (ast.Wire, ast.Reg)) for x in item.list):
          self._last_signal_idx = idx
        elif any(isinstance(x, ast.Parameter) for x in item.list):
          self._last_param_idx = idx
      elif isinstance(item, (ast.Assign, ast.Always)):
        self._last_logic_idx = idx
        if isinstance(item, ast.Assign):
          if item.left.var.name in HANDSHAKE_OUTPUT_PORTS:
            self._handshake_output_ports[item.left.var.name] = item
      elif isinstance(item, ast.InstanceList):
        self._last_instance_idx = idx

  @property
  def _module_def(self) -> ast.ModuleDef:
    _module_defs = [
        x for x in self.ast.description.definitions
        if isinstance(x, ast.ModuleDef)
    ]
    if len(_module_defs) != 1:
      raise ValueError('unexpected number of modules')
    return _module_defs[0]

  @property
  def name(self) -> str:
    return self._module_def.name

  @name.setter
  def name(self, name: str) -> None:
    self._module_def.name = name

  @property
  def ports(self) -> Dict[str, IOPort]:
    port_lists = (
        x.list for x in self._module_def.items if isinstance(x, ast.Decl))
    return collections.OrderedDict(
        (x.name, x)
        for x in itertools.chain.from_iterable(port_lists)
        if isinstance(x, (ast.Input, ast.Output, ast.Inout)))

  @property
  def params(self) -> Dict[str, ast.Parameter]:
    param_lists = (
        x.list for x in self._module_def.items if isinstance(x, ast.Decl))
    return collections.OrderedDict(
        (x.name, x)
        for x in itertools.chain.from_iterable(param_lists)
        if isinstance(x, ast.Parameter))

  @property
  def code(self) -> str:
    return '\n'.join(directive for _, directive in self.directives
                    ) + codegen.ASTCodeGenerator().visit(self.ast)

  def _increment_idx(self, length: int, target: str) -> None:
    attrs = 'io_port', 'signal', 'param', 'logic', 'instance'
    if target not in attrs:
      raise ValueError('target must be one of %s' % str(attrs))

    for attr in attrs:
      if attr == target:
        continue
      attr = '_last_%s_idx' % attr
      idx = '_last_%s_idx' % target
      if getattr(self, attr) > getattr(self, idx):
        setattr(self, attr, getattr(self, attr) + length)
    setattr(self, idx, getattr(self, idx) + length)

  def _filter(self, func: Callable[[ast.Node], bool], target: str) -> None:
    self._module_def.items = tuple(filter(func, self._module_def.items))
    self._calculate_indices()

  def add_ports(self, ports: Iterable[IOPort]) -> 'Module':
    port_tuple = tuple(ports)
    self._module_def.portlist.ports += tuple(
        ast.Port(name=port.name, width=None, dimensions=None, type=None)
        for port in port_tuple)
    self._module_def.items = (
        self._module_def.items[:self._last_io_port_idx + 1] + port_tuple +
        self._module_def.items[self._last_io_port_idx + 1:])
    self._increment_idx(len(port_tuple), 'io_port')
    return self

  def add_signals(self, signals: Iterable[Signal]) -> 'Module':
    signal_tuple = tuple(signals)
    self._module_def.items = (
        self._module_def.items[:self._last_signal_idx + 1] + signal_tuple +
        self._module_def.items[self._last_signal_idx + 1:])
    self._increment_idx(len(signal_tuple), 'signal')
    return self

  def add_pipeline(self, q: Pipeline, init: ast.Node) -> None:
    """Add registered signals and logics for q initialized by init.

    Args:
        q (Pipeline): The pipelined variable.
        init (ast.Node): Value used to drive the first stage of the pipeline.
    """
    self.add_signals(q.signals)
    self.add_logics((
        ast.Always(
            sens_list=CLK_SENS_LIST,
            statement=ast.make_block(
                ast.NonblockingSubstitution(left=q[i + 1], right=q[i])
                for i in range(REGISTER_LEVEL)),
        ),
        ast.Assign(left=q[0], right=init),
    ))

  def del_signals(self, prefix: str = '', suffix: str = '') -> None:

    def func(item: ast.Node) -> bool:
      if isinstance(item, ast.Decl):
        item = item.list[0]
        if isinstance(item, (ast.Reg, ast.Wire)):
          name: str = item.name
          if name.startswith(prefix) and name.endswith(suffix):
            return False
      return True

    self._filter(func, 'signal')

  def add_params(self, params: Iterable[ast.Parameter]) -> 'Module':
    param_tuple = tuple(params)
    self._module_def.items = (
        self._module_def.items[:self._last_param_idx + 1] + param_tuple +
        self._module_def.items[self._last_param_idx + 1:])
    self._increment_idx(len(param_tuple), 'param')
    return self

  def del_params(self, prefix: str = '', suffix: str = '') -> None:

    def func(item: ast.Node) -> bool:
      if isinstance(item, ast.Decl):
        item = item.list[0]
        if isinstance(item, ast.Parameter):
          name: str = item.name
          if name.startswith(prefix) and name.endswith(suffix):
            return False
      return True

    self._filter(func, 'param')

  def add_instance(
      self,
      module_name: str,
      instance_name: str,
      ports: Iterable[ast.ParamArg],
      params: Iterable[ast.ParamArg] = ()
  ) -> 'Module':
    self._module_def.items = (
        self._module_def.items[:self._last_instance_idx + 1] +
        (ast.InstanceList(module=module_name,
                          parameterlist=params,
                          instances=(ast.Instance(module=None,
                                                  name=instance_name,
                                                  parameterlist=None,
                                                  portlist=ports),)),) +
        self._module_def.items[self._last_instance_idx + 1:])
    self._increment_idx(1, 'instance')
    return self

  def add_logics(self, logics: Iterable[Logic]) -> 'Module':
    logic_tuple = tuple(logics)
    self._module_def.items = (
        self._module_def.items[:self._last_logic_idx + 1] + logic_tuple +
        self._module_def.items[self._last_logic_idx + 1:])
    self._increment_idx(len(logic_tuple), 'logic')
    return self

  def del_logics(self) -> None:

    def func(item: ast.Node) -> bool:
      if isinstance(item, (ast.Assign, ast.Always, ast.Initial)):
        return False
      return True

    self._filter(func, 'param')

  def del_pragmas(self, pragma: Iterable[str]) -> None:

    def func(item: ast.Node) -> bool:
      if isinstance(item, ast.Pragma):
        if item.entry.name == pragma or item.entry.name in pragma:
          return False
      return True

    self._filter(func, 'signal')

  def add_fifo_instance(
      self,
      name: str,
      width: int,
      depth: int,
  ) -> 'Module':
    name = sanitize_fifo_name(name)
    rst_q = Pipeline(f'{name}__rst')
    self.add_pipeline(rst_q, init=ast.Unot(RST_N))

    def ports() -> Iterator[ast.PortArg]:
      yield ast.make_port_arg(port='clk', arg=CLK)
      yield ast.make_port_arg(port='reset', arg=rst_q[-1])
      yield from (
          ast.make_port_arg(port=port_name, arg=wire_name(name, arg_suffix))
          for port_name, arg_suffix in zip(FIFO_READ_PORTS, ISTREAM_SUFFIXES))
      yield ast.make_port_arg(port=FIFO_READ_PORTS[-1], arg=TRUE)
      yield from (
          ast.make_port_arg(port=port_name, arg=wire_name(name, arg_suffix))
          for port_name, arg_suffix in zip(FIFO_WRITE_PORTS, OSTREAM_SUFFIXES))
      yield ast.make_port_arg(port=FIFO_WRITE_PORTS[-1], arg=TRUE)

    return self.add_instance(
        module_name='relay_station',
        instance_name=name,
        ports=ports(),
        params=(
            ast.ParamArg(paramname='DATA_WIDTH', argname=ast.Constant(width)),
            ast.ParamArg(paramname='ADDR_WIDTH',
                         argname=ast.Constant((depth - 1).bit_length())),
            ast.ParamArg(paramname='DEPTH', argname=ast.Constant(depth)),
            ast.ParamArg(paramname='LEVEL',
                         argname=ast.Constant(REGISTER_LEVEL + 1)),
        ))

  def add_async_mmap_instance(
      self,
      name: str,
      tags: Iterable[str],
      data_width: int,
      addr_width: int = 64,
      buffer_size: Optional[int] = None,
      max_wait_time: int = 3,
      max_burst_len: Optional[int] = None,
      reg_name: str = '',
  ) -> 'Module':
    rst_q = Pipeline(f'{name}__rst')
    self.add_pipeline(rst_q, init=ast.Unot(RST_N))

    paramargs = [
        ast.ParamArg(paramname='DataWidth', argname=ast.Constant(data_width)),
        ast.ParamArg(paramname='DataWidthBytesLog',
                     argname=ast.Constant((data_width // 8 - 1).bit_length())),
    ]
    portargs = [
        ast.make_port_arg(port='clk', arg=CLK),
        ast.make_port_arg(port='rst', arg=rst_q[-1]),
    ]
    paramargs.append(
        ast.ParamArg(paramname='AddrWidth', argname=ast.Constant(addr_width)))
    if buffer_size:
      paramargs.extend((ast.ParamArg(paramname='BufferSize',
                                     argname=ast.Constant(buffer_size)),
                        ast.ParamArg(paramname='BufferSizeLog',
                                     argname=ast.Constant(
                                         (buffer_size - 1).bit_length()))))
    max_wait_time = max(1, max_wait_time)
    paramargs.append(
        ast.ParamArg(paramname='WaitTimeWidth',
                     argname=ast.Constant(max_wait_time.bit_length())))
    portargs.append(
        ast.make_port_arg(port='max_wait_time',
                          arg="{}'d{}".format(max_wait_time.bit_length(),
                                              max_wait_time)))
    if max_burst_len is None:
      max_burst_len = max(0, 4096 // data_width - 1)
    paramargs.append(
        ast.ParamArg(paramname='BurstLenWidth',
                     argname=ast.Constant(max(1, max_burst_len.bit_length()))))
    portargs.append(
        ast.make_port_arg(port='max_burst_len',
                          arg="{}'d{}".format(
                              max(1, max_burst_len.bit_length()),
                              max_burst_len)))

    for channel, ports in M_AXI_PORTS.items():
      for port, direction in ports:
        width: Optional[int] = M_AXI_PORT_WIDTHS[port]
        if width == 0:
          if port == 'ADDR':
            width = addr_width
          elif port == 'DATA':
            width = data_width
          elif port == 'STRB':
            width = data_width // 8
        elif width == 1 and port not in {'ID', 'LOCK'}:
          width = None
        if width is not None:
          width = ast.Width(msb=ast.Constant(width - 1), lsb=ast.Constant(0))
        portargs.append(
            ast.make_port_arg(port=f'{M_AXI_PREFIX}{channel}{port}',
                              arg=f'{M_AXI_PREFIX}{name}_{channel}{port}'))

    tags = set(tags)
    for tag in 'read_addr', 'read_data', 'write_addr', 'write_data':
      for suffix in async_mmap_suffixes(tag=tag):
        if tag in tags:
          arg = async_mmap_arg_name(arg=name, tag=tag, suffix=suffix)
          if tag.endswith('_addr') and suffix.endswith('_din'):
            elem_size_bytes_m1 = data_width // 8 - 1
            arg = "{name} + {{{arg}[{}:0], {}'d0}}".format(
                addr_width - elem_size_bytes_m1.bit_length() - 1,
                elem_size_bytes_m1.bit_length(),
                arg=arg,
                name=reg_name or name)
        else:
          if suffix.endswith('_read') or suffix.endswith('_write'):
            arg = "1'b0"
          elif suffix.endswith('_din'):
            arg = "'d0"
          else:
            arg = ''
        portargs.append(ast.make_port_arg(port=tag + suffix, arg=arg))

    return self.add_instance(module_name='async_mmap',
                             instance_name=name + '_m_axi',
                             ports=portargs,
                             params=paramargs)

  def find_port(self, prefix: str, suffix: str) -> Optional[str]:
    '''Find an IO port with given prefix and suffix in this module.'''
    for port_name in self.ports:
      if port_name.startswith(prefix) and port_name.endswith(suffix):
        return port_name
    return None

  def add_m_axi(self,
                name: str,
                data_width: int,
                addr_width: int = 64) -> 'Module':
    for channel, ports in M_AXI_PORTS.items():
      io_ports = []
      for port, direction in ports:
        width: Optional[int] = M_AXI_PORT_WIDTHS[port]
        if width == 0:
          if port == 'ADDR':
            width = addr_width
          elif port == 'DATA':
            width = data_width
          elif port == 'STRB':
            width = data_width // 8
        elif width == 1 and port != 'ID':
          width = None
        if width is not None:
          width = ast.Width(msb=ast.Constant(width - 1), lsb=ast.Constant(0))
        io_ports.append((ast.Input if direction == 'input' else ast.Output)(
            name=f'{M_AXI_PREFIX}{name}_{channel}{port}', width=width))

      self.add_ports(io_ports)
    return self

  def cleanup(self) -> None:
    self.del_params(prefix='ap_ST_fsm_state')
    self.del_signals(prefix='ap_CS_fsm')
    self.del_signals(prefix='ap_NS_fsm')
    self.del_signals(HANDSHAKE_RST)
    self.del_signals(HANDSHAKE_DONE)
    self.del_signals(HANDSHAKE_IDLE)
    self.del_signals(HANDSHAKE_READY)
    self.del_logics()
    self.del_pragmas('fsm_encoding')
    self.add_signals(
        map(ast.Wire,
            (HANDSHAKE_RST, HANDSHAKE_DONE, HANDSHAKE_IDLE, HANDSHAKE_READY)))
    self.add_pipeline(RST_N_Q, init=RST_N)
    self.add_logics([ast.Assign(left=RST, right=ast.Unot(RST_N_Q[-1]))])


def is_data_port(port: str) -> bool:
  return (port.endswith(ISTREAM_SUFFIXES[0]) or
          port.endswith(OSTREAM_SUFFIXES[0]))


def is_m_axi_port(port: Union[str, IOPort]) -> bool:
  if not isinstance(port, str):
    port = port.name
  return (port.startswith(M_AXI_PREFIX) and
          '_' + port.split('_')[-1] in M_AXI_SUFFIXES)


def is_m_axi_param(param: Union[str, ast.Parameter]) -> bool:
  if not isinstance(param, str):
    param = param.name
  param_split = param.split('_')
  return (len(param_split) > 5 and param.startswith(M_AXI_PARAM_PREFIX) and
          ''.join(map('_{}'.format, param_split[-2:])) in M_AXI_PARAM_SUFFIXES)


def is_m_axi_unique_param(param: Union[str, ast.Parameter]) -> bool:
  if not isinstance(param, str):
    param = param.name
  return param in M_AXI_PARAMS


def rename_m_axi_name(mapping: Dict[str, str], name: str, idx1: int,
                      idx2: int) -> str:
  try:
    name_snippets = name.split('_')
    return '_'.join(name_snippets[:idx1] +
                    [mapping['_'.join(name_snippets[idx1:idx2])]] +
                    name_snippets[idx2:])
  except KeyError:
    pass
  raise ValueError("'%s' is a result of renaming done by Vivado HLS; " %
                   '_'.join(name_snippets[idx1:idx2]) +
                   'please use a different variable name')


def rename_m_axi_port(mapping: Dict[str, str], port: IOPort) -> IOPort:
  new_port = copy.copy(port)
  new_port.name = rename_m_axi_name(mapping, port.name, 2, -1)
  if port.width is not None and isinstance(port.width.msb, ast.Minus):
    new_port.width = copy.copy(new_port.width)
    new_port.width.msb = copy.copy(new_port.width.msb)
    new_port.width.msb.left = ast.Identifier(
        rename_m_axi_name(mapping, port.width.msb.left.name, 3, -2))
  return new_port


def rename_m_axi_param(mapping: Dict[str, str],
                       param: ast.Parameter) -> ast.Parameter:
  new_param = copy.copy(param)
  new_param.name = rename_m_axi_name(mapping, param.name, 3, -2)
  return new_param


def generate_handshake_ports(
    instance: tlp.instance.Instance,
    rst_q: Pipeline,
) -> Iterator[ast.PortArg]:
  yield ast.make_port_arg(port=HANDSHAKE_CLK, arg=CLK)
  yield ast.make_port_arg(port=HANDSHAKE_RST_N, arg=rst_q[-1])
  yield ast.make_port_arg(port=HANDSHAKE_START, arg=instance.start)
  for port in HANDSHAKE_OUTPUT_PORTS:
    yield ast.make_port_arg(
        port=port,
        arg="" if instance.is_autorun else wire_name(instance.name, port),
    )


def generate_m_axi_ports(
    port: str,
    arg: str,
    arg_reg: str = '',
) -> Iterator[ast.PortArg]:
  for suffix in M_AXI_SUFFIXES:
    yield ast.make_port_arg(port=M_AXI_PREFIX + port + suffix,
                            arg=M_AXI_PREFIX + arg + suffix)
  yield ast.make_port_arg(port=port + '_offset', arg=arg_reg or arg)


def fifo_port_name(fifo: str, suffix: str) -> str:
  """Return the port name of the fifo generated via HLS.

  Args:
      fifo (str): Name of the fifo.
      suffix (str): One of the suffixes in ISTREAM_SUFFIXES or OSTREAM_SUFFIXES.

  Returns:
      str: Port name of the fifo generated via HLS.
  """
  match = match_fifo_name(fifo)
  if match is not None:
    return f'{match[0]}_fifo_V_{match[1]}{suffix}'
  return f'{fifo}_fifo_V{suffix}'


def generate_istream_ports(port: str, arg: str) -> Iterator[ast.PortArg]:
  for suffix in ISTREAM_SUFFIXES:
    yield ast.make_port_arg(port=fifo_port_name(port, suffix),
                            arg=wire_name(arg, suffix))


def generate_ostream_ports(port: str, arg: str) -> Iterator[ast.PortArg]:
  for suffix in OSTREAM_SUFFIXES:
    yield ast.make_port_arg(port=fifo_port_name(port, suffix),
                            arg=wire_name(arg, suffix))


def generate_peek_ports(verilog, port: str, arg: str) -> Iterator[ast.PortArg]:
  match = match_fifo_name(port)
  if match is not None:
    port = f'{match[0]}_peek_val_{match[1]}'
  else:
    port = f'{port}_peek_val'
  for suffix in verilog.ISTREAM_SUFFIXES[:1]:
    yield ast.make_port_arg(port=port, arg=wire_name(arg, suffix))


def async_mmap_suffixes(tag: str) -> Tuple[str, str, str]:
  if tag in {'read_addr', 'write_addr', 'write_data'}:
    return OSTREAM_SUFFIXES
  if tag == 'read_data':
    return ISTREAM_SUFFIXES
  raise ValueError('invalid tag `%s`; tag must be one of `read_addr`, '
                   '`read_data`, `write_addr`, or `write_data`' % tag)


def async_mmap_arg_name(arg: str, tag: str, suffix: str) -> str:
  return wire_name(f'{arg}_{tag}', suffix)


def async_mmap_width(tag: str, suffix: str,
                     data_width: int) -> Optional[ast.Width]:
  if suffix in {ISTREAM_SUFFIXES[0], OSTREAM_SUFFIXES[0]}:
    if tag.endswith('addr'):
      data_width = 64
    return ast.Width(msb=ast.Constant(data_width - 1), lsb=ast.Constant(0))
  return None


def generate_async_mmap_ports(
    tag: str, port: str, arg: str,
    instance: tlp.instance.Instance) -> Iterator[ast.PortArg]:
  prefix = port + '_' + tag + '_V_'
  if tag.endswith('_data'):
    prefix += 'data_V_'
  for suffix in async_mmap_suffixes(tag):
    port_name = instance.task.module.find_port(prefix=prefix, suffix=suffix)
    if port_name is not None:
      yield ast.make_port_arg(port=port_name,
                              arg=async_mmap_arg_name(arg=arg,
                                                      tag=tag,
                                                      suffix=suffix))


def generate_async_mmap_signals(tag: str, arg: str,
                                data_width: int) -> Iterator[ast.Wire]:
  for suffix in async_mmap_suffixes(tag):
    yield ast.Wire(name=async_mmap_arg_name(arg=arg, tag=tag, suffix=suffix),
                   width=async_mmap_width(tag=tag,
                                          suffix=suffix,
                                          data_width=data_width))


def generate_async_mmap_ioports(tag: str, arg: str,
                                data_width: int) -> Iterator[IOPort]:
  for suffix in async_mmap_suffixes(tag):
    if suffix in {ISTREAM_SUFFIXES[-1], *OSTREAM_SUFFIXES[::2]}:
      ioport_type = ast.Output
    else:
      ioport_type = ast.Input
    yield ioport_type(name=async_mmap_arg_name(arg=arg, tag=tag, suffix=suffix),
                      width=async_mmap_width(tag=tag,
                                             suffix=suffix,
                                             data_width=data_width))


def pack(top_name: str, rtl_dir: str, ports: Iterable[tlp.instance.Port],
         output_file: Union[str, BinaryIO]) -> None:
  port_tuple = tuple(ports)
  if isinstance(output_file, str):
    xo_file = output_file
  else:
    xo_file = tempfile.mktemp(prefix='tlp_' + top_name + '_', suffix='.xo')
  with tempfile.NamedTemporaryFile(mode='w+',
                                   prefix='tlp_' + top_name + '_',
                                   suffix='_kernel.xml') as kernel_xml_obj:
    print_kernel_xml(name=top_name, ports=port_tuple, kernel_xml=kernel_xml_obj)
    kernel_xml_obj.flush()
    with backend.PackageXo(
        xo_file=xo_file,
        top_name=top_name,
        kernel_xml=kernel_xml_obj.name,
        hdl_dir=rtl_dir,
        m_axi_names=(port.name for port in port_tuple if port.cat in {
            tlp.instance.Instance.Arg.Cat.MMAP,
            tlp.instance.Instance.Arg.Cat.ASYNC_MMAP
        })) as proc:
      stdout, stderr = proc.communicate()
    if proc.returncode == 0 and os.path.exists(xo_file):
      if not isinstance(output_file, str):
        with open(xo_file, 'rb') as xo_obj:
          shutil.copyfileobj(xo_obj, output_file)
    else:
      sys.stdout.write(stdout.decode('utf-8'))
      sys.stderr.write(stderr.decode('utf-8'))
  if not isinstance(output_file, str):
    os.remove(xo_file)


def print_kernel_xml(name: str, ports: Iterable[tlp.instance.Port],
                     kernel_xml: IO[str]) -> None:
  """Generate kernel.xml file.

  Args:
    name: name of the kernel.
    ports: Iterable of tlp.instance.Port.
    kernel_xml: file object to write to.
  """
  args = []
  for port in ports:
    port_name = backend.S_AXI_NAME
    if port.cat == tlp.instance.Instance.Arg.Cat.SCALAR:
      cat = backend.Cat.SCALAR
    elif port.cat in {
        tlp.instance.Instance.Arg.Cat.MMAP,
        tlp.instance.Instance.Arg.Cat.ASYNC_MMAP
    }:
      cat = backend.Cat.MMAP
      port_name = M_AXI_PREFIX + port.name
    elif port.cat == tlp.instance.Instance.Arg.Cat.ISTREAM:
      cat = backend.Cat.ISTREAM
    elif port.cat == tlp.instance.Instance.Arg.Cat.OSTREAM:
      cat = backend.Cat.OSTREAM
    else:
      raise ValueError(f'unexpected port.cat: {port.cat}')

    args.append(
        backend.Arg(cat=cat,
                    name=port.name,
                    port=port_name,
                    ctype=port.ctype,
                    width=port.width))
  backend.print_kernel_xml(name, args, kernel_xml)


OTHER_MODULES = {
    'fifo_bram':
        backend.BRAM_FIFO_TEMPLATE.format(name='fifo_bram',
                                          width=32,
                                          depth=32,
                                          addr_width=(32 - 1).bit_length()),
    'fifo_srl':
        backend.SRL_FIFO_TEMPLATE.format(name='fifo_srl',
                                         width=32,
                                         depth=32,
                                         addr_width=(32 - 1).bit_length()),
    'fifo':
        backend.AUTO_FIFO_TEMPLATE.format(name='fifo',
                                          width=32,
                                          depth=32,
                                          addr_width=(32 - 1).bit_length()),
}
