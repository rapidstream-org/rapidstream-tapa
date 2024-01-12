import hashlib
import json
import logging
import os
import re
import shutil
import subprocess
import sys
from typing import *

import click

import tapa
import tapa.core
import tapa.steps.common
import tapa.util
from tapa.common.graph import Graph as TapaGraph

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.pass_context
@click.option('--input',
              '-f',
              required=True,
              multiple=True,
              type=click.Path(dir_okay=False, readable=True, exists=True),
              default=(),
              help='Input file, usually TAPA C++ source code.')
@click.option('--top',
              '-t',
              metavar='TASK',
              required=True,
              type=str,
              help='Name of the top-level task.')
@click.option('--cflags',
              '-c',
              multiple=True,
              type=str,
              default=(),
              help='Compiler flags for the kernel, may appear many times.')
@click.option('--tapacc',
              type=click.Path(dir_okay=False, readable=True, exists=True),
              help='Specify a `tapacc` instead of searching in `PATH`.')
@click.option('--tapa-clang',
              type=click.Path(dir_okay=False, readable=True, exists=True),
              help='Specify a `tapa-clang` instead of searching in `PATH`.')
def analyze(ctx, input: Tuple[str, ...], top: str, cflags: Tuple[str, ...],
            tapacc: str, tapa_clang: str) -> None:

  tapacc = find_clang_binary('tapacc', tapacc)
  tapa_clang = find_clang_binary('tapa-clang', tapa_clang)

  work_dir = tapa.steps.common.get_work_dir()
  cflags += ('-std=c++17',)

  flatten_files = run_flatten(tapa_clang, input, cflags, work_dir)
  tapacc_cflags, system_cflags = find_tapacc_cflags(tapacc, cflags)

  graph_dict = run_tapacc(tapacc, flatten_files, top,
                          tapacc_cflags + system_cflags)
  graph_dict['cflags'] = tapacc_cflags

  # Flatten the graph
  graph_dict = TapaGraph(None, graph_dict).get_flatten_graph().to_dict()

  tapa.steps.common.store_tapa_program(tapa.core.Program(graph_dict, work_dir))

  tapa.steps.common.store_persistent_context('graph', graph_dict)
  tapa.steps.common.store_persistent_context('settings', {})

  tapa.steps.common.is_pipelined('analyze', True)


def find_clang_binary(name: str, override: Optional[str]) -> str:
  """Find executable from PATH if not overrided.

  From PATH and user `override` value, look for a clang-based executable
  `name` and then verify if that is an excutable binary.

  Args:
    name: The name of the binary.
    override: A user specified path of the binary, or None

  Returns:
    Verified binary path.

  Raises:
    click.BadParameter: If unable to find the usable binary of given name.
  """

  # Lookup binary from PATH and override
  if override is None:
    binary = shutil.which(name)
  else:
    binary = override
  if binary is None:
    raise click.BadParameter('Cannot find the binary file',
                             param=override,
                             param_hint=name)

  # Check if the binary is working
  version = subprocess.check_output([binary, '--version'],
                                    universal_newlines=True)
  match = re.compile(R'version (\d+)(\.\d+)*').search(version)
  if match is None:
    raise click.BadParameter(f'failed to parse output: {version}',
                             param=override,
                             param_hint=name)

  return binary


def find_tapacc_cflags(
    tapacc: str, cflags: Tuple[str,
                               ...]) -> Tuple[Tuple[str, ...], Tuple[str, ...]]:
  """Append tapa, system and vendor libraries to tapacc cflags.

  Args:
    tapacc: The path of the tapacc binary.
    cflags: User-given CFLAGS.

  Returns:
    A tuple, the first element is the CFLAGS with vendor libraries for HLS,
    including the tapa and HLS vnedor libraries, and the second element is
    the CFLAGS for system libraries, such as clang and libc++.

  Raises:
    click.UsageError: Unable to find the include folders.
  """

  # Add TAPA include files to tapacc cflags
  tapa_include = os.path.join(os.path.dirname(tapa.__file__), '..', '..', '..',
                              'src')

  # Find clang include location
  tapacc_version = subprocess.check_output(
      [tapacc, '-version'],
      universal_newlines=True,
  )
  match = re.compile(R'LLVM version (\d+)(\.\d+)*').search(tapacc_version)
  if match is None:
    _logger.error(f'failed to parse tapacc output: {tapacc_version}')
    sys.exit(-1)

  # Add vendor include files to tapacc cflags
  vendor_include_paths = ()
  for vendor_path in tapa.util.get_vendor_include_paths():
    vendor_include_paths += ('-isystem', vendor_path)
    _logger.info('added vendor include path `%s`', vendor_path)

  # Add system include files to tapacc cflags
  system_includes = ['-stdlib=libc++']
  system_includes.extend(
      ['-isystem', f"/usr/lib/llvm-{match[1]}/include/c++/v1/"])
  system_includes.extend(
      ['-isystem', f"/usr/include/clang/{match[1]}/include/"])
  system_includes.extend(['-isystem', f"/usr/lib/clang/{match[1]}/include/"])

  return (cflags[:] + ('-I', tapa_include) + vendor_include_paths,
          tuple(system_includes))


def run_and_check(cmd: Tuple[str, ...]) -> str:
  """Run command and check return code.

  Args:
    cmd: The command to execute.

  Returns:
    Stdout of the command execution.
  """
  proc = subprocess.run(cmd,
                        stdout=subprocess.PIPE,
                        universal_newlines=True,
                        check=False)
  if proc.returncode != 0:
    _logger.error(
        'command %s failed with exit code %d',
        cmd,
        proc.returncode,
    )
    exit(proc.returncode)

  return proc.stdout


def run_flatten(tapa_clang: str, files: Tuple[str, ...],
                cflags: Tuple[str, ...], work_dir: str) -> Tuple[str, ...]:
  """Flatten input files.

  Preprocess input C/C++ files so that all macros are expanded, and all
  header files, excluding system and TAPA header files, are inlined.

  Args:
    tapa_clang: The path of the tapa-clang binary.
    files: C/C++ files to flatten.
    cflags: User specified CFLAGS.
    work_dir: Working directory of TAPA, for output of the flatten files.

  Returns:
    Tuple of the flattened output files.
  """

  flatten_folder = os.path.join(work_dir, 'flatten')
  os.makedirs(flatten_folder, exist_ok=True)
  flatten_files = []

  for file in files:
    # Generate hash-based file name for flattened files
    hash = hashlib.sha256()
    hash.update(os.path.abspath(file).encode())
    flatten_name = ('flatten-' + hash.hexdigest()[:8] + '-' +
                    os.path.basename(file))
    flatten_path = os.path.join(flatten_folder, flatten_name)
    flatten_files.append(flatten_path)

    # Output flatten code to the file
    with open(flatten_path, 'w') as output_fp:
      # FIXME: workaround by -D__SYNTHESIS__.  However, this should not be
      #        hardcoded for all targets.  TAPA should provide a
      #        target-specific directive for the users.
      tapa_clang_cmd = (tapa_clang, '-E', '-nostdinc', '-nostdinc++', '-CC',
                        '-P', '-D__SYNTHESIS__') + cflags + (file,)
      flatten_code = run_and_check(tapa_clang_cmd)
      formated_code = tapa.util.clang_format(flatten_code)
      output_fp.write(formated_code)

  return tuple(flatten_files)


def run_tapacc(tapacc: str, files: Tuple[str, ...], top: str,
               cflags: Tuple[str, ...]) -> Dict:
  """Execute tapacc and return the program description.

  Args:
    tapacc: The path of the tapacc binary.
    files: C/C++ files to flatten.
    cflags: User specified CFLAGS with TAPA specific headers.

  Returns:
    Output description of the TAPA program.
  """

  tapacc_args = ('-top', top, '--') + cflags
  tapacc_cmd = (tapacc,) + files + tapacc_args
  _logger.info('Running tapacc command: %s', ' '.join(tapacc_cmd))

  return json.loads(run_and_check(tapacc_cmd))
