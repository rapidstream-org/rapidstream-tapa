#!/usr/bin/python3
import argparse
import io
import json
import os
import os.path
import re
import shutil
import subprocess
import sys
from typing import Dict, List, Optional, TextIO, Tuple

import haoda.backend.xilinx
import tlp.core


def main():
  parser = argparse.ArgumentParser(
      prog='tlpc',
      description=
      'Task-Level Parallelization (TLP) for High-Level Synthesis (HLS) compiler'
  )
  parser.add_argument('--tlpcc',
                      type=str,
                      metavar='file',
                      dest='tlpcc',
                      help='override tlpcc')
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
  parser.add_argument('--directive',
                      type=argparse.FileType('r'),
                      dest='directive',
                      metavar='file',
                      help='input partitioning directive json file')
  parser.add_argument('--constraint',
                      type=argparse.FileType('w'),
                      dest='constraint',
                      metavar='file',
                      help='output partitioning constraint tcl file')
  parser.add_argument('--register-level',
                      type=int,
                      dest='register_level',
                      metavar='INT',
                      help='override register level of top-level signals')
  parser.add_argument(type=str,
                      dest='input_file',
                      metavar='file',
                      help='tlp source code or tlp program json')

  group = parser.add_argument_group('steps')
  group.add_argument('--run-tlpcc',
                     action='count',
                     dest='run_tlpcc',
                     help='run tlpcc and create program.json')
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

  args = parser.parse_args()

  all_steps = True
  last_step = ''
  for arg in ('run_tlpcc', 'extract_cpp', 'run_hls', 'extract_rtl',
              'instrument_rtl', 'pack_xo'):
    if getattr(args, arg) is not None:
      all_steps = False
      last_step = arg
  if all_steps:
    last_step = arg

  if last_step == 'run_tlpcc' and args.output_file is None:
    parser.error('output file must be set if tlpcc is the last step')
  if last_step == 'pack_xo' and args.output_file is None:
    parser.error('output file must be set if pack xo is the last step')
  if not all_steps and last_step != 'run_tlpcc' and args.work_dir is None:
    parser.error("steps beyond run tlpcc won't work with --work-dir unset")
  cflag_list = []
  if args.cflags is not None:
    cflag_list = args.cflags.strip().split()

  if all_steps or args.run_tlpcc is not None:
    tlpcc_cmd = []
    # set tlpcc executable
    if args.tlpcc is None:
      tlpcc = shutil.which('tlpcc')
    else:
      tlpcc = args.tlpcc
    if tlpcc is None:
      parser.error('cannot find tlpcc')
    tlpcc_cmd += tlpcc, args.input_file

    # set top-level task name
    if args.top is None:
      parser.error('tlpcc cannot run without --top')
    tlp_include_dir = os.path.join(os.path.dirname(sys.argv[0]), '..', '..',
                                   'src')
    tlpcc_cmd += '-top', args.top, '--', '-I', tlp_include_dir

    # find clang include location
    tlpcc_version = subprocess.check_output(
        [tlpcc, '-version'],
        universal_newlines=True,
    )
    match = re.compile(R'LLVM version (\d+)(\.\d+)*').search(tlpcc_version)
    if match is None:
      parser.error(f'failed to parse tlpcc output: {tlpcc_version}')
    clang_version = match[1]
    clang_include = os.path.join('/', 'usr', 'lib', 'clang', clang_version,
                                 'include')
    if not os.path.isdir(clang_include):
      parser.error(f'missing clang include directory: {clang_include}')
    tlpcc_cmd += '-I', clang_include
    tlpcc_cmd += cflag_list

    proc = subprocess.run(tlpcc_cmd,
                          stdout=subprocess.PIPE,
                          universal_newlines=True,
                          check=False)
    if proc.returncode != 0:
      parser.exit(status=proc.returncode)
    tlp_program_json_dict = json.loads(proc.stdout)

    # Use -MM to find all user headers
    input_file_basename = os.path.basename(args.input_file)
    input_file_dirname = os.path.dirname(args.input_file) or '.'
    deps: str = subprocess.check_output([
        'clang++-' + clang_version,
        '-MM',
        input_file_basename,
        '-I',
        tlp_include_dir,
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
                os.path.join(tlp_include_dir, 'tlp.h'),
            }, dep_set))
    for dep in dep_set:
      tlp_program_json_dict.setdefault('headers', {})
      with open(os.path.join(input_file_dirname, dep), 'r') as dep_fp:
        tlp_program_json_dict['headers'][dep] = dep_fp.read()
    tlp_program_json_str = json.dumps(tlp_program_json_dict, indent=2)

    # save program.json if work_dir is set or run_tlpcc is the last step
    if args.work_dir is not None or last_step == 'run_tlpcc':
      if last_step == 'run_tlpcc':
        tlp_program_json_file = args.output_file
      else:
        tlp_program_json_file = os.path.join(args.work_dir, 'program.json')
      os.makedirs(os.path.dirname(tlp_program_json_file) or '.', exist_ok=True)
      with open(tlp_program_json_file, 'w') as output_fp:
        output_fp.write(tlp_program_json_str)
    tlp_program_json = lambda: io.StringIO(tlp_program_json_str)
  else:
    if args.input_file.endswith('.json') or args.work_dir is None:
      tlp_program_json = lambda: open(args.input_file)
    else:
      tlp_program_json = lambda: open(
          os.path.join(args.work_dir, 'program.json'))

  cflags = ' -I' + os.path.join(
      os.path.dirname(tlp.__file__),
      'assets',
      'cpp',
  )
  if args.cflags is not None:
    cflags += ' ' + args.cflags
  with tlp_program_json() as tlp_program_json_obj:
    program = tlp.core.Program(tlp_program_json_obj,
                               cflags=cflags,
                               work_dir=args.work_dir)

  if all_steps or args.extract_cpp is not None:
    program.extract_cpp()

  if all_steps or args.run_hls is not None:
    program.run_hls(**haoda.backend.xilinx.parse_device_info(
        parser,
        args,
        platform_name='platform',
        part_num_name='part_num',
        clock_period_name='clock_period',
    ))

  if all_steps or args.extract_rtl is not None:
    program.extract_rtl()

  if all_steps or args.instrument_rtl is not None:
    directive: Optional[Tuple[Dict[str, List[str]]], TextIO] = None
    if args.directive is not None and args.constraint is not None:
      directive = json.load(args.directive), args.constraint
    elif args.directive is not None or args.constraint is not None:
      parser.error('only one of directive and constraint is given')
    if args.register_level is not None:
      if args.register_level <= 0:
        parser.error('register level must be positive')
    program.instrument_rtl(directive, args.register_level or 0)

  if all_steps or args.pack_xo is not None:
    with open(args.output_file, 'wb') as packed_obj:
      program.pack_rtl(packed_obj)


if __name__ == '__main__':
  main()
