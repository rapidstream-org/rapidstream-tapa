from typing import Iterator, Optional, Tuple

import tapa.instance
from tapa.verilog import ast, util
from tapa.verilog.xilinx.const import ISTREAM_SUFFIXES, OSTREAM_SUFFIXES
from tapa.verilog.xilinx.typing import IOPort

__all__ = [
    'async_mmap_suffixes',
    'async_mmap_arg_name',
    'async_mmap_width',
    'generate_async_mmap_ports',
    'generate_async_mmap_signals',
    'generate_async_mmap_ioports',
]


def async_mmap_suffixes(tag: str) -> Tuple[str, str, str]:
  if tag in {'read_addr', 'write_addr', 'write_data'}:
    return OSTREAM_SUFFIXES
  if tag == 'read_data':
    return ISTREAM_SUFFIXES
  raise ValueError('invalid tag `%s`; tag must be one of `read_addr`, '
                   '`read_data`, `write_addr`, or `write_data`' % tag)


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
    return ast.Width(msb=ast.Constant(data_width - 1), lsb=ast.Constant(0))
  return None


def generate_async_mmap_ports(
    tag: str, port: str, arg: str,
    instance: tapa.instance.Instance) -> Iterator[ast.PortArg]:
  prefix = port + '_' + tag + '_V_'
  for suffix in async_mmap_suffixes(tag):
    port_name = instance.task.module.find_port(prefix=prefix, suffix=suffix)
    if port_name is not None:
      yield ast.make_port_arg(
          port=port_name,
          arg=async_mmap_arg_name(
              arg=arg,
              tag=tag,
              suffix=suffix,
          ),
      )


def generate_async_mmap_signals(tag: str, arg: str,
                                data_width: int) -> Iterator[ast.Wire]:
  for suffix in async_mmap_suffixes(tag):
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
  for suffix in async_mmap_suffixes(tag):
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
