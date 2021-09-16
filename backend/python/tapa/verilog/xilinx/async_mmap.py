import logging
from typing import Iterator, Optional

import tapa.instance
from tapa.verilog import ast, util
from tapa.verilog.xilinx.const import (ISTREAM_SUFFIXES, OSTREAM_SUFFIXES,
                                       STREAM_PORT_DIRECTION)
from tapa.verilog.xilinx.typing import IOPort

__all__ = [
    'ASYNC_MMAP_SUFFIXES',
    'async_mmap_arg_name',
    'async_mmap_width',
    'generate_async_mmap_ports',
    'generate_async_mmap_signals',
    'generate_async_mmap_ioports',
]

_logger = logging.getLogger().getChild(__name__)

ASYNC_MMAP_SUFFIXES = {
    'read_addr': OSTREAM_SUFFIXES,
    'read_data': ISTREAM_SUFFIXES,
    'write_addr': OSTREAM_SUFFIXES,
    'write_data': OSTREAM_SUFFIXES,
    'write_resp': ISTREAM_SUFFIXES,
}


def async_mmap_arg_name(arg: str, tag: str, suffix: str) -> str:
  return util.wire_name(f'{arg}_{tag}', suffix)


def async_mmap_width(
    tag: str,
    suffix: str,
    data_width: int,
) -> Optional[ast.Width]:
  if suffix in {ISTREAM_SUFFIXES[0], OSTREAM_SUFFIXES[0]}:
    if tag.endswith('addr'):
      data_width = 64
    elif tag == 'write_resp':
      data_width = 8
    return ast.Width(msb=ast.Constant(data_width - 1), lsb=ast.Constant(0))
  return None


def generate_async_mmap_ports(
    tag: str, port: str, arg: str,
    instance: tapa.instance.Instance) -> Iterator[ast.PortArg]:
  # TODO: reuse functions that generate i/ostream ports
  prefix = port + '_' + tag
  for suffix in ASYNC_MMAP_SUFFIXES[tag]:
    arg_name = async_mmap_arg_name(
        arg=arg,
        tag=tag,
        suffix=suffix,
    )
    port_name = instance.task.module.find_port(prefix=prefix, suffix=suffix)
    if port_name is not None:
      # Make sure Eot is always 1'b0.
      if suffix == ISTREAM_SUFFIXES[0]:
        arg_name = f"{{1'b0, {arg_name}}}"

      _logger.debug('`%s.%s` is connected to async_mmap port `%s.%s`',
                    instance.name, port_name, arg, tag)
      yield ast.make_port_arg(port=port_name, arg=arg_name)

    # Generate peek ports.
    if ASYNC_MMAP_SUFFIXES[tag] is ISTREAM_SUFFIXES:
      port_name = instance.task.module.find_port(prefix + '_peek', suffix)
      if port_name is not None:
        # Ignore read enable from peek ports.
        if STREAM_PORT_DIRECTION[suffix] == 'input':
          _logger.debug('`%s.%s` is connected to async_mmap port `%s.%s`',
                        instance.name, port_name, arg, tag)
        else:
          arg_name = ''

        yield ast.make_port_arg(port=port_name, arg=arg_name)


def generate_async_mmap_signals(tag: str, arg: str,
                                data_width: int) -> Iterator[ast.Wire]:
  for suffix in ASYNC_MMAP_SUFFIXES[tag]:
    yield ast.Wire(
        name=async_mmap_arg_name(arg=arg, tag=tag, suffix=suffix),
        width=async_mmap_width(
            tag=tag,
            suffix=suffix,
            data_width=data_width,
        ),
    )


def generate_async_mmap_ioports(tag: str, arg: str,
                                data_width: int) -> Iterator[IOPort]:
  for suffix in ASYNC_MMAP_SUFFIXES[tag]:
    if suffix in {ISTREAM_SUFFIXES[-1], *OSTREAM_SUFFIXES[::2]}:
      ioport_type = ast.Output
    else:
      ioport_type = ast.Input
    yield ioport_type(
        name=async_mmap_arg_name(arg=arg, tag=tag, suffix=suffix),
        width=async_mmap_width(
            tag=tag,
            suffix=suffix,
            data_width=data_width,
        ),
    )
