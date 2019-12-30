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
from typing import (IO, BinaryIO, Dict, Iterable, Iterator, Optional, Tuple,
                    Type, Union)

from pyverilog.ast_code_generator import codegen
from pyverilog.vparser import ast, parser

import haoda.backend.xilinx
import tlp.core

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
    LOCK=2,
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
M_AXI_PORTS = collections.OrderedDict(
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
)  # type: Dict[str, Tuple[Tuple[str, str], ...]]

M_AXI_PREFIX = 'm_axi_'

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
    '_V_dout',
    '_V_empty_n',
    '_V_read',
)

OSTREAM_SUFFIXES = (
    '_V_din',
    '_V_full_n',
    '_V_write',
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

HANDSHAKE_INPUT_PORTS = (
    'ap_clk',
    'ap_rst_n',
    'ap_start',
)
HANDSHAKE_OUTPUT_PORTS = (
    'ap_done',
    'ap_idle',
    'ap_ready',
)
HANDSHAKE_START = HANDSHAKE_INPUT_PORTS[-1]
HANDSHAKE_DONE = HANDSHAKE_OUTPUT_PORTS[0]

# const ast nodes

START = ast.Identifier(HANDSHAKE_START)
DONE = ast.Identifier(HANDSHAKE_DONE)
TRUE = ast.Identifier("1'b1")
FALSE = ast.Identifier("1'b0")
SENS_TYPE = 'posedge'
CLK = ast.Identifier(HANDSHAKE_INPUT_PORTS[0])
CLK_SENS_LIST = ast.SensList((ast.Sens(CLK, type=SENS_TYPE),))
RESET_COND = ast.Eq(left=ast.Identifier(HANDSHAKE_INPUT_PORTS[1]), right=FALSE)

# type aliases

Directive = Tuple[int, str]
IOPort = Union[ast.Input, ast.Output, ast.Inout]
Signal = Union[ast.Reg, ast.Wire]
Logic = Union[ast.Assign, ast.Always]


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
    self.ast, self.directives = parser.parse(
        files)  # type: ast.Source,  Tuple[Directive, ...]
    self._handshake_output_ports = {}  # type: Dict[str, ast.Assign]
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

  def add_params(self, params: Iterable[Signal]) -> 'Module':
    param_tuple = tuple(params)
    self._module_def.items = (
        self._module_def.items[:self._last_param_idx + 1] + param_tuple +
        self._module_def.items[self._last_param_idx + 1:])
    self._increment_idx(len(param_tuple), 'param')
    return self

  def add_instance(self,
                   module_name: str,
                   instance_name: str,
                   ports: Iterable[ast.ParamArg],
                   params: Iterable[ast.ParamArg] = ()) -> 'Module':
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

  def add_fifo_instance(
      self,
      name: str,
      width: int,
      depth: int,
  ) -> 'Module':

    def ports() -> Iterator[ast.PortArg]:
      yield make_port_arg(port='clk', arg='ap_clk')
      yield make_port_arg(port='reset', arg='ap_rst_n_inv')
      yield from (
          make_port_arg(port=port_name, arg=name + arg_suffix)
          for port_name, arg_suffix in zip(FIFO_READ_PORTS, ISTREAM_SUFFIXES))
      yield make_port_arg(port=FIFO_READ_PORTS[-1], arg="1'b1")
      yield from (
          make_port_arg(port=port_name, arg=name + arg_suffix)
          for port_name, arg_suffix in zip(FIFO_WRITE_PORTS, OSTREAM_SUFFIXES))
      yield make_port_arg(port=FIFO_WRITE_PORTS[-1], arg="1'b1")

    return self.add_instance(module_name='fifo',
                             instance_name=name,
                             ports=ports(),
                             params=(
                                 ast.ParamArg(paramname='DATA_WIDTH',
                                              argname=ast.Constant(width)),
                                 ast.ParamArg(paramname='ADDR_WIDTH',
                                              argname=ast.Constant(
                                                  (depth - 1).bit_length())),
                                 ast.ParamArg(paramname='DEPTH',
                                              argname=ast.Constant(depth)),
                             ))

  def add_async_mmap_instance(self,
                              name: str,
                              tags: Iterable[str],
                              data_width: int,
                              addr_width: int = 64,
                              buffer_size: Optional[int] = None,
                              max_wait_time: Optional[int] = 3,
                              max_burst_len: Optional[int] = 255) -> 'Module':
    paramargs = [
        ast.ParamArg(paramname='DataWidth', argname=ast.Constant(data_width)),
        ast.ParamArg(paramname='DataWidthBytesLog',
                     argname=ast.Constant((data_width // 8 - 1).bit_length())),
    ]
    portargs = [
        make_port_arg(port='clk', arg=HANDSHAKE_INPUT_PORTS[0]),
        make_port_arg(port='rst', arg=HANDSHAKE_INPUT_PORTS[1] + '_inv'),
    ]
    paramargs.append(
        ast.ParamArg(paramname='AddrWidth', argname=ast.Constant(addr_width)))
    if buffer_size:
      paramargs.extend((ast.ParamArg(paramname='BufferSize',
                                     argname=ast.Constant(buffer_size)),
                        ast.ParamArg(paramname='BufferSizeLog',
                                     argname=ast.Constant(
                                         (buffer_size - 1).bit_length()))))
    if max_wait_time:
      paramargs.append(
          ast.ParamArg(paramname='WaitTimeWidth',
                       argname=ast.Constant(max_wait_time.bit_length())))
      portargs.append(
          make_port_arg(port='max_wait_time',
                        arg="{}'d{}".format(max_wait_time.bit_length(),
                                            max_wait_time)))
    if max_burst_len:
      paramargs.append(
          ast.ParamArg(paramname='BurstLenWidth',
                       argname=ast.Constant(max_burst_len.bit_length())))
      portargs.append(
          make_port_arg(port='max_burst_len',
                        arg="{}'d{}".format(max_burst_len.bit_length(),
                                            max_burst_len)))

    for channel, ports in M_AXI_PORTS.items():
      for port, direction in ports:
        width = M_AXI_PORT_WIDTHS[port]  # type: Optional[int]
        if width == 0:
          if port == 'ADDR':
            width = addr_width
          elif port == 'DATA':
            width = data_width
          elif port == 'STRB':
            width = data_width // 8
        elif width == 1:
          width = None
        if width is not None:
          width = ast.Width(msb=ast.Constant(width - 1), lsb=ast.Constant(0))
        portargs.append(
            make_port_arg(port='m_axi_{channel}{port}'.format(channel=channel,
                                                              port=port),
                          arg='m_axi_{name}_{channel}{port}'.format(
                              name=name, channel=channel, port=port)))

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
                name=name)
        else:
          if suffix.endswith('_read') or suffix.endswith('_write'):
            arg = "1'b0"
          else:
            arg = ''
        portargs.append(make_port_arg(port=tag + suffix[2:], arg=arg))

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

  def add_m_axi(self, name: str, data_width: int,
                addr_width: int = 64) -> 'Module':
    for channel, ports in M_AXI_PORTS.items():
      io_ports = []
      for port, direction in ports:
        width = M_AXI_PORT_WIDTHS[port]  # type: Optional[int]
        if width == 0:
          if port == 'ADDR':
            width = addr_width
          elif port == 'DATA':
            width = data_width
          elif port == 'STRB':
            width = data_width // 8
        elif width == 1:
          width = None
        if width is not None:
          width = ast.Width(msb=ast.Constant(width - 1), lsb=ast.Constant(0))
        io_ports.append((ast.Input if direction == 'input' else ast.Output)(
            name='m_axi_{name}_{channel}{port}'.format(name=name,
                                                       channel=channel,
                                                       port=port),
            width=width))

      self.add_ports(io_ports)
    return self

  def replace_assign(self, signal: str, content: ast.Node) -> 'Module':
    signal = 'ap_' + signal
    if signal not in self._handshake_output_ports:
      raise ValueError('signal has to be one of {}, got {}'.format(
          tuple(self._handshake_output_ports), signal))
    self._handshake_output_ports[signal].right = content
    return self


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


def make_port_arg(port: str, arg: str) -> ast.PortArg:
  return ast.PortArg(portname=port, argname=ast.Identifier(name=arg))


def make_operation(operator: Type[ast.Operator],
                   nodes: Iterable[ast.Node]) -> ast.Operator:
  """Make an operation node out of an iterable of Nodes.

  Note that the nodes appears in the reverse order of the iterable.

  Args:
    op: Operator.
    nodes: Iterable of Node. If empty, raises StopIteration.

  Returns:
    Operator of the result.
  """
  iterator = iter(nodes)
  node = next(iterator)
  try:
    return operator(make_operation(operator, iterator), node)
  except StopIteration:
    return node


def generate_handshake_ports(name: str) -> Iterator[ast.PortArg]:
  for port in HANDSHAKE_INPUT_PORTS:
    yield make_port_arg(port=port, arg=port)
  for port in HANDSHAKE_OUTPUT_PORTS:
    yield make_port_arg(port=port, arg=name + '_' + port)


def generate_m_axi_ports(port: str, arg: str) -> Iterator[ast.PortArg]:
  for suffix in M_AXI_SUFFIXES:
    yield make_port_arg(port=M_AXI_PREFIX + port + suffix,
                        arg=M_AXI_PREFIX + arg + suffix)
  yield make_port_arg(port=port + '_offset', arg=arg)


def generate_istream_ports(port: str, arg: str) -> Iterator[ast.PortArg]:
  for suffix in ISTREAM_SUFFIXES:
    yield make_port_arg(port=port + suffix, arg=arg + suffix)


def generate_ostream_ports(port: str, arg: str) -> Iterator[ast.PortArg]:
  for suffix in OSTREAM_SUFFIXES:
    yield make_port_arg(port=port + suffix, arg=arg + suffix)


def async_mmap_suffixes(tag: str) -> Tuple[str, str, str]:
  if tag in {'read_addr', 'write_addr', 'write_data'}:
    return OSTREAM_SUFFIXES
  if tag == 'read_data':
    return ISTREAM_SUFFIXES
  raise ValueError('invalid tag `%s`; tag must be one of `read_addr`, '
                   '`read_data`, `write_addr`, or `write_data`' % tag)


def async_mmap_arg_name(arg: str, tag: str, suffix: str) -> str:
  return arg + '_' + tag + suffix


def async_mmap_width(tag: str, suffix: str,
                     data_width: int) -> Optional[ast.Width]:
  if suffix in {'_V_din', '_V_dout'}:
    if tag.endswith('addr'):
      data_width = 64
    return ast.Width(msb=ast.Constant(data_width - 1), lsb=ast.Constant(0))
  return None


def generate_async_mmap_ports(tag: str, port: str, arg: str,
                              instance: tlp.core.Instance
                             ) -> Iterator[ast.PortArg]:
  prefix = 'tlp_' + port + '_' + tag + '_V_'
  for suffix in async_mmap_suffixes(tag):
    port_name = instance.task.module.find_port(prefix=prefix, suffix=suffix)
    if port_name is not None:
      yield make_port_arg(port=port_name,
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
    if suffix in {'_V_din', '_V_write', '_V_read'}:
      ioport_type = ast.Output
    else:
      ioport_type = ast.Input
    yield ioport_type(name=async_mmap_arg_name(arg=arg, tag=tag, suffix=suffix),
                      width=async_mmap_width(tag=tag,
                                             suffix=suffix,
                                             data_width=data_width))


def pack(top_name: str, rtl_dir: str, ports: Iterable[tlp.core.Port],
         output_file: Union[str, BinaryIO]) -> None:
  port_tuple = tuple(ports)
  if isinstance(output_file, str):
    xo_file = output_file
  else:
    xo_file = tempfile.mktemp(prefix='tlp_' + top_name + '_', suffix='.xo')
  with tempfile.NamedTemporaryFile(mode='w+',
                                   prefix='tlp_' + top_name + '_',
                                   suffix='_kernel.xml') as kernel_xml_obj:
    print_kernel_xml(top_name=top_name,
                     ports=port_tuple,
                     kernel_xml=kernel_xml_obj)
    with haoda.backend.xilinx.PackageXo(
        xo_file=xo_file,
        top_name=top_name,
        kernel_xml=kernel_xml_obj.name,
        hdl_dir=rtl_dir,
        m_axi_names=(port.name for port in port_tuple if port.cat in {
            tlp.core.Instance.Arg.Cat.MMAP, tlp.core.Instance.Arg.Cat.ASYNC_MMAP
        })) as proc:
      stdout, stderr = proc.communicate()
    if proc.returncode == 0:
      if not isinstance(output_file, str):
        with open(xo_file, 'rb') as xo_obj:
          shutil.copyfileobj(xo_obj, output_file)
    else:
      sys.stdout.write(stdout.decode('utf-8'))
      sys.stderr.write(stderr.decode('utf-8'))
  if not isinstance(output_file, str):
    os.remove(xo_file)


def print_kernel_xml(top_name: str, ports: Iterable[tlp.core.Port],
                     kernel_xml: IO[str]) -> None:
  """Generate kernel.xml file.

  Args:
    top_name: name of the top-level kernel function.
    ports: Iterable of tlp.core.Port.
    kernel_xml: file object to write to.
  """
  m_axi_ports = ''
  args = ''
  offset = 0x10
  for arg_id, port in enumerate(ports):
    host_size = port.width // 8
    size = max(4, host_size)
    rtl_port_name = 's_axi_control'
    addr_qualifier = 0
    if port.cat in {
        tlp.core.Instance.Arg.Cat.MMAP, tlp.core.Instance.Arg.Cat.ASYNC_MMAP
    }:
      size = host_size = 8  # m_axi interface must have 64-bit addresses
      rtl_port_name = M_AXI_PREFIX + port.name
      addr_qualifier = 1
      m_axi_ports += haoda.backend.xilinx.M_AXI_PORT_TEMPLATE.format(
          name=port.name, width=port.width).rstrip('\n')
    args += haoda.backend.xilinx.ARG_TEMPLATE.format(
        name=port.name,
        addr_qualifier=addr_qualifier,
        arg_id=arg_id,
        port_name=rtl_port_name,
        c_type=port.ctype,
        size=size,
        offset=offset,
        host_size=host_size).rstrip('\n')
    offset += size + 4
  kernel_xml.write(
      haoda.backend.xilinx.KERNEL_XML_TEMPLATE.format(top_name=top_name,
                                                      ports=m_axi_ports,
                                                      args=args))
  kernel_xml.flush()


OTHER_MODULES = {
    'fifo_bram':
        haoda.backend.xilinx.BRAM_FIFO_TEMPLATE.format(
            name='fifo_bram',
            width=32,
            depth=32,
            addr_width=(32 - 1).bit_length()),
    'fifo_srl':
        haoda.backend.xilinx.SRL_FIFO_TEMPLATE.format(
            name='fifo_srl',
            width=32,
            depth=32,
            addr_width=(32 - 1).bit_length()),
    'fifo':
        haoda.backend.xilinx.AUTO_FIFO_TEMPLATE.format(
            name='fifo', width=32, depth=32, addr_width=(32 - 1).bit_length()),
}
