#!/usr/bin/python3
import argparse
import io
import json
import logging
import os
import os.path
import re
import shutil
import subprocess
import sys
from typing import Any, Dict, List, Optional

import haoda.backend.xilinx
from absl import flags

import tapa.core

logging.basicConfig(
    level=logging.WARNING,
    format=
    '%(levelname).1s%(asctime)s.%(msecs)03d %(name)s:%(lineno)d] %(message)s',
    datefmt='%m%d %H:%M:%S',
)

_logger = logging.getLogger().getChild(__name__)


def create_parser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(
      prog='tapac',
      description='Compiles TAPA C++ code into packaged RTL.',
  )
  parser.add_argument(
      '-v',
      '--verbose',
      action='count',
      dest='verbose',
      help='Increase logging verbosity.',
  )
  parser.add_argument(
      '-q',
      '--quiet',
      action='count',
      dest='quiet',
      help='Decrease logging verbosity.',
  )
  parser.add_argument(
      '--tapacc',
      type=str,
      metavar='file',
      dest='tapacc',
      help='Use a specific ``tapacc`` binary instead of searching in ``PATH``.',
  )
  parser.add_argument(
      '--work-dir',
      type=str,
      metavar='dir',
      dest='work_dir',
      help='Use a specific working directory instead of a temporary one.',
  )
  parser.add_argument(
      '--top',
      type=str,
      dest='top',
      metavar='TASK_NAME',
      help='Name of the top-level task.',
  )
  parser.add_argument(
      '--clock-period',
      type=str,
      dest='clock_period',
      help='Target clock period in nanoseconds.',
  )
  parser.add_argument(
      '--part-num',
      type=str,
      dest='part_num',
      help='Target FPGA part number.',
  )
  parser.add_argument(
      '--platform',
      type=str,
      dest='platform',
      help='Target Vitis platform.',
  )
  parser.add_argument(
      '--cflags',
      action='append',
      default=[],
      dest='cflags',
      help='Compiler flags for the kernel, may appear many times.',
  )
  parser.add_argument(
      '-o',
      '--output',
      type=str,
      dest='output_file',
      metavar='file',
      help='Output file.',
  )
  parser.add_argument(
      '--frt-interface',
      type=str,
      dest='frt_interface',
      metavar='file',
      help='Output FRT interface file (deprecated).',
  )
  parser.add_argument(
      type=str,
      dest='input_file',
      metavar='file',
      help='Input file, usually TAPA C++ source code.',
  )

  group = parser.add_argument_group(
      title='Compilation Steps',
      description='Selectively run compilation steps (advanced usage).',
  )
  group.add_argument(
      '--run-tapacc',
      action='count',
      dest='run_tapacc',
      help='Run ``tapacc`` and create ``program.json``.',
  )
  group.add_argument(
      '--extract-cpp',
      action='count',
      dest='extract_cpp',
      help='Extract HLS C++ files from ``program.json``.',
  )
  group.add_argument(
      '--run-hls',
      action='count',
      dest='run_hls',
      help='Run HLS and generate RTL tarballs.',
  )
  group.add_argument(
      '--extract-rtl',
      action='count',
      dest='extract_rtl',
      help='Extract RTL tarballs.',
  )
  group.add_argument(
      '--instrument-rtl',
      action='count',
      dest='instrument_rtl',
      help='Instrument RTL.',
  )
  group.add_argument(
      '--pack-xo',
      action='count',
      dest='pack_xo',
      help='Package RTL as a Xilinx object file.',
  )

  group = parser.add_argument_group(
      title='Floorplanning',
      description=('Coarse-grained floorplanning via AutoBridge '
                   '(advanced usage).'),
  )
  group.add_argument(
      '--connectivity',
      type=argparse.FileType('r'),
      dest='connectivity',
      metavar='file',
      help=('Input ``connectivity.ini`` specification for mmaps. '
            'This is the same file passed to ``v++``.'),
  )
  group.add_argument(
      '--directive',
      type=argparse.FileType('r'),
      dest='directive',
      metavar='file',
      help=('Use a specific partitioning directive json file '
            'instead of generating one via AutoBridge.'),
  )
  group.add_argument(
      '--constraint',
      type=argparse.FileType('w'),
      dest='constraint',
      metavar='file',
      help='Output partitioning ``constraint.tcl`` file for ``v++``.',
  )
  group.add_argument(
      '--register-level',
      type=int,
      dest='register_level',
      metavar='INT',
      help=('Use a specific register level of top-level scalar signals '
            'instead of inferring from the floorplanning directive.'),
  )
  group.add_argument(
      '--max-usage',
      type=float,
      dest='max_usage',
      metavar='0-1',
      help=('Use a specific floorplan usage threshold instead of using the '
            'conservative default in AutoBridge.'),
  )
  group.add_argument(
      '--enable-synth-util',
      dest='enable_synth_util',
      action='store_true',
      help=('Enable post-synthesis resource utilization report '
            'for floorplanning.'),
      default=False,
  )
  group.add_argument(
      '--disable-synth-util',
      dest='enable_synth_util',
      action='store_false',
      help=('Disable post-synthesis resource utilization report '
            'for floorplanning.'),
      default=True,
  )
  group.add_argument(
      '--force-dag',
      dest='force_dag',
      action='store_true',
      help='Forces DAG for floorplanning.',
      default=False,
  )

  return parser


def main(argv: Optional[List[str]] = None):
  argv = flags.FLAGS(argv or sys.argv[1:], known_only=True)

  parser = create_parser()
  args = parser.parse_args(argv)
  verbose = 0 if args.verbose is None else args.verbose
  quiet = 0 if args.quiet is None else args.quiet
  logging_level = (quiet -
                   verbose) * 10 + logging.getLogger().getEffectiveLevel()
  logging_level = max(logging.DEBUG, min(logging.CRITICAL, logging_level))
  logging.getLogger().setLevel(logging_level)
  _logger.info('logging level set to %s', logging.getLevelName(logging_level))

  all_steps = True
  last_step = ''
  for arg in ('run_tapacc', 'extract_cpp', 'run_hls', 'extract_rtl',
              'instrument_rtl', 'pack_xo'):
    if getattr(args, arg) is not None:
      all_steps = False
      last_step = arg
  if all_steps:
    last_step = arg

  if last_step == 'run_tapacc' and args.output_file is None:
    parser.error('output file must be set if tapacc is the last step')
  if last_step == 'pack_xo' and args.output_file is None:
    parser.error('output file must be set if pack xo is the last step')
  if not all_steps and last_step != 'run_tapacc' and args.work_dir is None:
    parser.error("steps beyond run tapacc won't work with --work-dir unset")
  cflag_list = ['-std=c++17'] + args.cflags

  if all_steps or args.run_tapacc is not None:
    tapacc_cmd = []
    # set tapacc executable
    if args.tapacc is None:
      tapacc = shutil.which('tapacc')
    else:
      tapacc = args.tapacc
    if tapacc is None:
      parser.error('cannot find tapacc')
    tapacc_cmd += tapacc, args.input_file

    # set top-level task name
    if args.top is None:
      parser.error('tapacc cannot run without --top')
    tapa_include_dir = os.path.join(
        os.path.dirname(tapa.__file__),
        '..',
        '..',
        '..',
        'src',
    )
    tapacc_cmd += '-top', args.top, '--', '-I', tapa_include_dir

    # find clang include location
    tapacc_version = subprocess.check_output(
        [tapacc, '-version'],
        universal_newlines=True,
    )
    match = re.compile(R'LLVM version (\d+)(\.\d+)*').search(tapacc_version)
    if match is None:
      parser.error(f'failed to parse tapacc output: {tapacc_version}')
    clang_include = os.path.join(
        os.path.dirname(tapa.core.__file__),
        'assets',
        'clang',
    )
    if not os.path.isdir(clang_include):
      parser.error(f'missing clang include directory: {clang_include}')
    tapacc_cmd += '-isystem', clang_include

    # Append include paths that are automatically available in vendor tools.
    vendor_include_paths = []
    for vendor_env in 'XILINX_HLS', 'XILINX_VITIS', 'XILINX_VIVADO':
      vendor_path = os.environ.get(vendor_env)
      if vendor_path is not None:
        vendor_include_paths += [
            '-isystem', os.path.join(vendor_path, 'include')
        ]
    tapacc_cmd += vendor_include_paths

    tapacc_cmd += cflag_list

    proc = subprocess.run(tapacc_cmd,
                          stdout=subprocess.PIPE,
                          universal_newlines=True,
                          check=False)
    if proc.returncode != 0:
      _logger.error(
          'tapacc command %s failed with exit code %d',
          tapacc_cmd,
          proc.returncode,
      )
      parser.exit(status=proc.returncode)
    tapa_program_json_dict = json.loads(proc.stdout)

    # Use -MM to find all user headers
    input_file_basename = os.path.basename(args.input_file)
    input_file_dirname = os.path.dirname(args.input_file) or '.'
    deps: str = subprocess.check_output([
        os.environ.get('CXX', 'g++'),
        '-MM',
        input_file_basename,
        '-I',
        tapa_include_dir,
        *vendor_include_paths,
        *cflag_list,
    ],
                                        cwd=input_file_dirname,
                                        universal_newlines=True)
    # partition -MM output at '.o: '
    deps = deps.rstrip('\n').partition('.o: ')[-1].replace('\\\n', ' ')
    # split at non-escaped space and replace escaped spaces with spaces
    dep_set = {x.replace('\\ ', ' ') for x in re.split(r'(?<!\\) ', deps)}
    dep_set = set(
        filter(
            lambda x: not os.path.isabs(x) and x not in {
                '',
                input_file_basename,
                os.path.join(tapa_include_dir, 'tapa.h'),
            }, dep_set))
    for dep in dep_set:
      tapa_program_json_dict.setdefault('headers', {})
      with open(os.path.join(input_file_dirname, dep), 'r') as dep_fp:
        tapa_program_json_dict['headers'][dep] = dep_fp.read()
    tapa_program_json_str = json.dumps(tapa_program_json_dict, indent=2)

    # save program.json if work_dir is set or run_tapacc is the last step
    if args.work_dir is not None or last_step == 'run_tapacc':
      if last_step == 'run_tapacc':
        tapa_program_json_file = args.output_file
      else:
        tapa_program_json_file = os.path.join(args.work_dir, 'program.json')
      os.makedirs(os.path.dirname(tapa_program_json_file) or '.', exist_ok=True)
      with open(tapa_program_json_file, 'w') as output_fp:
        output_fp.write(tapa_program_json_str)
    tapa_program_json = lambda: io.StringIO(tapa_program_json_str)
  else:
    if args.input_file.endswith('.json') or args.work_dir is None:
      tapa_program_json = lambda: open(args.input_file)
    else:
      tapa_program_json = lambda: open(
          os.path.join(args.work_dir, 'program.json'))

  with tapa_program_json() as tapa_program_json_obj:
    program = tapa.core.Program(tapa_program_json_obj,
                                cflags=' '.join(args.cflags),
                                work_dir=args.work_dir)

  if args.frt_interface is not None and program.frt_interface is not None:
    with open(args.frt_interface, 'w') as output_fp:
      output_fp.write(program.frt_interface)

  if all_steps or args.extract_cpp is not None:
    program.extract_cpp()

  if all_steps or args.run_hls is not None:
    program.run_hls(**_get_device_info(parser, args))

  if all_steps or args.extract_rtl is not None:
    program.extract_rtl()

  if all_steps or args.instrument_rtl is not None:
    directive: Optional[Dict[str, Any]] = None
    if args.directive is not None and args.constraint is not None:
      # Read floorplan from `args.directive` and write TCL commands to
      # `args.constraint`.
      directive = dict(
          floorplan=json.load(args.directive),
          constraint=args.constraint,
      )
    elif args.directive is not None:
      parser.error('only directive given; constraint needed as output')
    elif args.constraint is not None:
      # Generate floorplan using AutoBridge, optionally with a connectivity file
      # that is used for `v++ --link` to specify mmap connections, and write TCL
      # commands to `args.constraint`.
      directive = dict(
          connectivity=args.connectivity,
          part_num=_get_device_info(parser, args)['part_num'],
          constraint=args.constraint,
      )
    if args.register_level is not None:
      if args.register_level <= 0:
        parser.error('register level must be positive')
    kwargs = {}
    if args.max_usage is not None:
      kwargs['user_max_usage_ratio'] = args.max_usage
    if args.force_dag is not None:
      kwargs['force_dag'] = args.force_dag
    program.instrument_rtl(
        directive,
        args.register_level or 0,
        args.enable_synth_util,
        **kwargs,
    )

  if all_steps or args.pack_xo is not None:
    with open(args.output_file, 'wb') as packed_obj:
      program.pack_rtl(packed_obj)


def _get_device_info(
    parser: argparse.ArgumentParser,
    args: argparse.Namespace,
    # Intentionally parse device_info only once.
    # pylint: disable=dangerous-default-value
    device_info: Dict[str, str] = {},
) -> Dict[str, str]:
  if not device_info:
    device_info.update(
        haoda.backend.xilinx.parse_device_info(
            parser,
            args,
            platform_name='platform',
            part_num_name='part_num',
            clock_period_name='clock_period',
        ))
  return device_info


if __name__ == '__main__':
  main()
