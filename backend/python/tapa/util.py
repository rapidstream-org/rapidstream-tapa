import argparse
import configparser
import logging
import os.path
import shutil
import subprocess
import time
from typing import Dict, Iterator, TextIO, Tuple, Optional

import absl.logging
import coloredlogs

from .task import Task
from .instance import Instance

_logger = logging.getLogger().getChild(__name__)


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
          raise AssertionError(
              f'Missing physical binding for {arg.name} in {vitis_config_ini}')

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


def get_max_addr_width(part_num: str) -> int:
  """ get the max addr width based on the memory capacity """
  if part_num.startswith('xcu280'):
    addr_width = 35  # 8GB of HBM capacity or 32 GB of DDR capacity
  elif part_num.startswith('xcu250'):
    addr_width = 36  # 64GB of DDR capacity
  else:
    addr_width = 64

  return addr_width


def get_vendor_include_paths() -> Iterator[str]:
  """Yields include paths that are automatically available in vendor tools."""
  try:
    for line in subprocess.check_output(
        ['frt_get_xlnx_env'],
        universal_newlines=True,
    ).split('\0'):
      if not line:
        continue
      key, value = line.split('=', maxsplit=1)
      if key == 'XILINX_HLS':
        yield os.path.join(value, 'include')
  except FileNotFoundError:
    _logger.warn('not adding vendor include paths; please update FRT')


def nproc() -> int:
  return int(subprocess.check_output(['nproc']))


def setup_logging(verbose: Optional[int],
                  quiet: Optional[int],
                  work_dir: Optional[str],
                  program_name: str = 'tapac') -> None:
  verbose = 0 if verbose is None else verbose
  quiet = 0 if quiet is None else quiet
  logging_level = (quiet - verbose) * 10 + logging.INFO
  logging_level = max(logging.DEBUG, min(logging.CRITICAL, logging_level))

  coloredlogs.install(
      level=logging_level,
      fmt='%(levelname).1s%(asctime)s %(name)s:%(lineno)d] %(message)s',
      datefmt='%m%d %H:%M:%S.%f',
  )

  log_dir = None
  if work_dir is not None:
    log_dir = os.path.join(work_dir, 'log')
    os.makedirs(log_dir, exist_ok=True)

  # The following is copied and modified from absl-py.
  log_dir, file_prefix, symlink_prefix = absl.logging.find_log_dir_and_names(
      program_name,
      log_dir=log_dir,
  )
  time_str = time.strftime('%Y%m%d-%H%M%S', time.localtime(time.time()))
  basename = f'{file_prefix}.INFO.{time_str}.{os.getpid()}'
  filename = os.path.join(log_dir, basename)
  symlink = os.path.join(log_dir, symlink_prefix + '.INFO')
  try:
    if os.path.islink(symlink):
      os.unlink(symlink)
    os.symlink(os.path.basename(filename), symlink)
  except EnvironmentError:
    # If it fails, we're sad but it's no error. Commonly, this fails because the
    # symlink was created by another user so we can't modify it.
    pass

  handler = logging.FileHandler(filename, encoding='utf-8')
  handler.setFormatter(
      logging.Formatter(
          fmt=('%(levelname).1s%(asctime)s.%(msecs)03d '
               '%(name)s:%(lineno)d] %(message)s'),
          datefmt='%m%d %H:%M:%S',
      ))
  handler.setLevel(logging.DEBUG)
  logging.getLogger().addHandler(handler)

  _logger.info('logging level set to %s', logging.getLevelName(logging_level))
