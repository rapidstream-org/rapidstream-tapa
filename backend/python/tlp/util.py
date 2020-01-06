import shutil
import subprocess
from typing import Iterator, Tuple

from tlp.verilog import ast


def clang_format(code: str, *args: str) -> str:
  """Apply clang-format with given arguments, if possible."""
  for version in range(10, 4, -1):
    clang_format_exe = shutil.which('clang-format-%d' % version)
    if clang_format_exe is not None:
      break
  else:
    clang_format_exe = shutil.which('clang-format')
  if clang_format_exe is not None:
    proc = subprocess.run([clang_format_exe, *args],
                          input=code,
                          stdout=subprocess.PIPE,
                          check=True,
                          universal_newlines=True)
    proc.check_returncode()
    return proc.stdout
  return code


def generate_peek_ports(verilog, port: str, arg: str) -> Iterator[ast.PortArg]:
  for suffix in verilog.ISTREAM_SUFFIXES[:1]:
    yield ast.make_port_arg(port='tlp_' + port + '_peek', arg=arg + suffix)


def get_instance_name(item: Tuple[str, int]) -> str:
  return '_'.join(map(str, item))
