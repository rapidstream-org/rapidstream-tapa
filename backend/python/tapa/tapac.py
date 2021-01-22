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
from typing import Any, Dict, Optional

import haoda.backend.xilinx

import tapa.core

logging.basicConfig(
    level=logging.WARNING,
    format=
    '%(levelname).1s%(asctime)s.%(msecs)03d %(name)s:%(lineno)d] %(message)s',
    datefmt='%m%d %H:%M:%S',
)

_logger = logging.getLogger().getChild(__name__)


def main():
  parser = argparse.ArgumentParser(prog='tapac', description='TAPA compiler')
  parser.add_argument('--verbose',
                      '-v',
                      action='count',
                      dest='verbose',
                      help='increase verbosity')
  parser.add_argument('--quiet',
                      '-q',
                      action='count',
                      dest='quiet',
                      help='decrease verbosity')
  parser.add_argument('--tapacc',
                      type=str,
                      metavar='file',
                      dest='tapacc',
                      help='override tapacc')
  parser.add_argument(
      '--work-dir',
      type=str,
      metavar='dir',
      dest='work_dir',
      help='use a specific working directory instead of a temporary one')
  parser.add_argument('--top',
                      type=str,
                      dest='top',
                      metavar='TASK_NAME',
                      help='name of the top-level task')
  parser.add_argument('--clock-period',
                      type=str,
                      dest='clock_period',
                      help='clock period in nanoseconds')
  parser.add_argument('--part-num',
                      type=str,
                      dest='part_num',
                      help='part number')
  parser.add_argument('--platform',
                      type=str,
                      dest='platform',
                      help='SDAccel platform')
  parser.add_argument('--cflags',
                      type=str,
                      dest='cflags',
                      help='cflags for kernel, space separated')
  parser.add_argument('--output',
                      '-o',
                      type=str,
                      dest='output_file',
                      metavar='file',
                      help='output file')
  parser.add_argument('--frt-interface',
                      type=str,
                      dest='frt_interface',
                      metavar='file',
                      help='output FRT interface file')
  parser.add_argument(type=str,
                      dest='input_file',
                      metavar='file',
                      help='tapa source code or tapa program json')

  group = parser.add_argument_group('steps')
  group.add_argument('--run-tapacc',
                     action='count',
                     dest='run_tapacc',
                     help='run tapacc and create program.json')
  group.add_argument('--extract-cpp',
                     action='count',
                     dest='extract_cpp',
                     help='extract HLS C++ files from program.json')
  group.add_argument('--run-hls',
                     action='count',
                     dest='run_hls',
                     help='run HLS and generate RTL tarballs')
  group.add_argument('--extract-rtl',
                     action='count',
                     dest='extract_rtl',
                     help='untar RTL tarballs')
  group.add_argument('--instrument-rtl',
                     action='count',
                     dest='instrument_rtl',
                     help='instrument RTL')
  group.add_argument('--pack-xo',
                     action='count',
                     dest='pack_xo',
                     help='package as Xilinx object')

  group = parser.add_argument_group('floorplanning')
  group.add_argument('--connectivity',
                     type=argparse.FileType('r'),
                     dest='connectivity',
                     metavar='file',
                     help='input connectivity specification for mmaps')
  group.add_argument('--directive',
                     type=argparse.FileType('r'),
                     dest='directive',
                     metavar='file',
                     help='input partitioning directive json file')
  group.add_argument('--constraint',
                     type=argparse.FileType('w'),
                     dest='constraint',
                     metavar='file',
                     help='output partitioning constraint tcl file')
  group.add_argument('--register-level',
                     type=int,
                     dest='register_level',
                     metavar='INT',
                     help='override register level of top-level signals')

  args = parser.parse_args()
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
  cflag_list = []
  if args.cflags is not None:
    cflag_list = args.cflags.strip().split()

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
    clang_version = match[1]
    clang_include = os.path.join('/', 'usr', 'lib', 'clang', clang_version,
                                 'include')
    if not os.path.isdir(clang_include):
      parser.error(f'missing clang include directory: {clang_include}')
    tapacc_cmd += '-I', clang_include
    tapacc_cmd += cflag_list

    proc = subprocess.run(tapacc_cmd,
                          stdout=subprocess.PIPE,
                          universal_newlines=True,
                          check=False)
    if proc.returncode != 0:
      parser.exit(status=proc.returncode)
    tapa_program_json_dict = json.loads(proc.stdout)

    # Use -MM to find all user headers
    input_file_basename = os.path.basename(args.input_file)
    input_file_dirname = os.path.dirname(args.input_file) or '.'
    deps: str = subprocess.check_output([
        'clang++-' + clang_version,
        '-MM',
        input_file_basename,
        '-I',
        tapa_include_dir,
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

  cflags = ' -I' + os.path.join(
      os.path.dirname(tapa.__file__),
      'assets',
      'cpp',
  )
  if args.cflags is not None:
    cflags += ' ' + args.cflags
  with tapa_program_json() as tapa_program_json_obj:
    program = tapa.core.Program(tapa_program_json_obj,
                                cflags=cflags,
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
    program.instrument_rtl(directive, args.register_level or 0)

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
