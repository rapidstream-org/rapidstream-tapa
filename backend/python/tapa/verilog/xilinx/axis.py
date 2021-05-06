from typing import Optional

from tapa.verilog import ast

__all__ = [
    'AXIS_PORT_WIDTHS',
    'STREAM_TO_AXIS',
    'AXIS_CONSTANTS',
    'get_axis_port_width_int',
]

# width=0 means configurable
AXIS_PORT_WIDTHS = dict(
    TDATA=0,
    TLAST=1,
    TVALID=1,
    TREADY=1,
    TKEEP=0,
)

# {port_suffix: [axis_port_suffixes in order]}
STREAM_TO_AXIS = {
    '_dout':    ['TDATA', 'TLAST'],
    '_empty_n': ['TVALID'],
    '_read':    ['TREADY'],
    '_din':     ['TDATA', 'TLAST'],
    '_full_n':  ['TREADY'],
    '_write':   ['TVALID'],
}

# {port_suffix: bit_to_fill}
AXIS_CONSTANTS = {
    'TKEEP':    1
}

def get_axis_port_width_int(port: str, data_width: int) -> int:
  width = AXIS_PORT_WIDTHS[port]
  if width == 0:
    if port == 'TDATA':
      width = data_width
    elif port == 'TKEEP':
      width = data_width // 8
  return width
