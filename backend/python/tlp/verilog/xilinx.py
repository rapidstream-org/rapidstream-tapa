"""Helpers for Xilinx verilog.

This module defines constants and helper functions for verilog files generated
for Xilinx devices.
"""

from typing import (
    Dict,
    Iterable,
    Iterator,
    Tuple,
    Type,
    Union,
)

import collections
import enum
import itertools

from pyverilog.vparser import ast
from pyverilog.vparser import parser
from pyverilog.ast_code_generator import codegen

# const strings

RTL_SUFFIX = '.v'

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


class FIFOStyle(enum.Enum):
  BRAM = 'bram'
  SRL = 'srl'


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
        ast.Port(name=port.name, width=None, type=None) for port in port_tuple)
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
      style: FIFOStyle = FIFOStyle.BRAM,
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

    return self.add_instance(module_name='fifo_' + style.value,
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


OTHER_MODULES = {
    'fifo_bram':
        '''`timescale 1ns/1ps

module fifo_bram
#(parameter
    MEM_STYLE   = "block",
    DATA_WIDTH  = 512,
    ADDR_WIDTH  = 4,
    DEPTH       = 16
)
(
    // system signal
    input  wire                  clk,
    input  wire                  reset,

    // write
    output wire                  if_full_n,
    input  wire                  if_write_ce,
    input  wire                  if_write,
    input  wire [DATA_WIDTH-1:0] if_din,

    // read
    output wire                  if_empty_n,
    input  wire                  if_read_ce,
    input  wire                  if_read,
    output wire [DATA_WIDTH-1:0] if_dout
);
//------------------------Parameter----------------------

//------------------------Local signal-------------------
(* ram_style = MEM_STYLE *)
reg  [DATA_WIDTH-1:0] mem[0:DEPTH-1];
reg  [DATA_WIDTH-1:0] q_buf = 1'b0;
reg  [ADDR_WIDTH-1:0] waddr = 1'b0;
reg  [ADDR_WIDTH-1:0] raddr = 1'b0;
wire [ADDR_WIDTH-1:0] wnext;
wire [ADDR_WIDTH-1:0] rnext;
wire                  push;
wire                  pop;
reg  [ADDR_WIDTH-1:0] usedw = 1'b0;
reg                   full_n = 1'b1;
reg                   empty_n = 1'b0;
reg  [DATA_WIDTH-1:0] q_tmp = 1'b0;
reg                   show_ahead = 1'b0;
reg  [DATA_WIDTH-1:0] dout_buf = 1'b0;
reg                   dout_valid = 1'b0;


//------------------------Instantiation------------------

//------------------------Task and function--------------

//------------------------Body---------------------------
assign if_full_n  = full_n;
assign if_empty_n = dout_valid;
assign if_dout    = dout_buf;
assign push       = full_n & if_write_ce & if_write;
assign pop        = empty_n & if_read_ce & (~dout_valid | if_read);
assign wnext      = !push                ? waddr :
                    (waddr == DEPTH - 1) ? 1'b0  :
                    waddr + 1'b1;
assign rnext      = !pop                 ? raddr :
                    (raddr == DEPTH - 1) ? 1'b0  :
                    raddr + 1'b1;

// waddr
always @(posedge clk) begin
    if (reset == 1'b1)
        waddr <= 1'b0;
    else
        waddr <= wnext;
end

// raddr
always @(posedge clk) begin
    if (reset == 1'b1)
        raddr <= 1'b0;
    else
        raddr <= rnext;
end

// usedw
always @(posedge clk) begin
    if (reset == 1'b1)
        usedw <= 1'b0;
    else if (push & ~pop)
        usedw <= usedw + 1'b1;
    else if (~push & pop)
        usedw <= usedw - 1'b1;
end

// full_n
always @(posedge clk) begin
    if (reset == 1'b1)
        full_n <= 1'b1;
    else if (push & ~pop)
        full_n <= (usedw != DEPTH - 1);
    else if (~push & pop)
        full_n <= 1'b1;
end

// empty_n
always @(posedge clk) begin
    if (reset == 1'b1)
        empty_n <= 1'b0;
    else if (push & ~pop)
        empty_n <= 1'b1;
    else if (~push & pop)
        empty_n <= (usedw != 1'b1);
end

// mem
always @(posedge clk) begin
    if (push)
        mem[waddr] <= if_din;
end

// q_buf
always @(posedge clk) begin
    q_buf <= mem[rnext];
end

// q_tmp
always @(posedge clk) begin
    if (reset == 1'b1)
        q_tmp <= 1'b0;
    else if (push)
        q_tmp <= if_din;
end

// show_ahead
always @(posedge clk) begin
    if (reset == 1'b1)
        show_ahead <= 1'b0;
    else if (push && usedw == pop)
        show_ahead <= 1'b1;
    else
        show_ahead <= 1'b0;
end

// dout_buf
always @(posedge clk) begin
    if (reset == 1'b1)
        dout_buf <= 1'b0;
    else if (pop)
        dout_buf <= show_ahead? q_tmp : q_buf;
end

// dout_valid
always @(posedge clk) begin
    if (reset == 1'b1)
        dout_valid <= 1'b0;
    else if (pop)
        dout_valid <= 1'b1;
    else if (if_read_ce & if_read)
        dout_valid <= 1'b0;
end

endmodule
'''
}
