import configparser
import shutil
import subprocess
from typing import Tuple, TextIO, Dict

from .task import Task
from .instance import Instance


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


def get_instance_name(item: Tuple[str, int]) -> str:
  return '_'.join(map(str, item))


def get_module_name(module: str) -> str:
  return f'{module}'


def parse_connectivity(vitis_config_ini: TextIO) -> Dict[str, str]:
  """parse the .ini config file. 
  
    Example:
    [connectivity]
    sp=serpens_1.edge_list_ch0:HBM[0]
  
    Output:
    {'edge_list_ch0': 'HBM[0]'}
  """
  if vitis_config_ini is None:
    return {}

  class MultiDict(dict):

    def __setitem__(self, key, value):
      if isinstance(value, list) and key in self:
        self[key].extend(value)
      else:
        super().__setitem__(key, value)

  config = configparser.RawConfigParser(dict_type=MultiDict, strict=False)
  config.read_file(vitis_config_ini)

  arg_name_to_external_port = {}
  for connectivity in config['connectivity']['sp'].splitlines():
    if not connectivity:
      continue

    dot = connectivity.find('.')
    colon = connectivity.find(':')
    kernel = connectivity[:dot]
    kernel_arg = connectivity[dot + 1:colon]
    port = connectivity[colon + 1:]

    arg_name_to_external_port[kernel_arg] = port

  return arg_name_to_external_port

def parse_connectivity_and_check_completeness(
    vitis_config_ini: TextIO,
    top_task: Task,
) -> Dict[str, str]:
  arg_name_to_external_port = parse_connectivity(vitis_config_ini)

  # check that every MMAP/ASYNC_MMAP port has a physical mapping
  for arg_list in top_task.args.values():
    for arg in arg_list:
      if arg.cat in {Instance.Arg.Cat.ASYNC_MMAP, Instance.Arg.Cat.MMAP}:
        if arg.name not in arg_name_to_external_port:
          raise AssertionError(f'Missing physical binding for {arg.name} in {vitis_config_ini}')

  return arg_name_to_external_port

def parse_port(port: str) -> Tuple[str, int]:
  bra = port.find('[')
  ket = port.find(']')
  colon = port.find(':')
  if colon != -1:
    ket = colon  # use the first channel if a range is specified
  port_cat = port[:bra] 
  port_id = int(port[bra + 1:ket])
  return port_cat, port_id
