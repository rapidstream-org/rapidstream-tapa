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
from typing import Any, Dict, List, Optional, Tuple

import haoda.backend.xilinx
from absl import flags

import tapa.core
import tapa.util
from tapa.bitstream import get_vitis_script
from tapa.floorplan_dse import run_floorplan_dse
from tapa.hardware import is_part_num_supported

_logger = logging.getLogger().getChild(__name__)

flags.DEFINE_integer(
    'recursionlimit',
    3000,
    'override Python recursion limit; RTL parsing may require a deep stack',
)


def create_parser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(
      prog='tapac',
      description='Compiles TAPA C++ code into packaged RTL.',
  )
  parser.add_argument(
      '-V',
      '--version',
      action='version',
      version=tapa.__version__,
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
  parser.add_argument(
      '--other-hls-configs',
      type=str,
      dest='other_hls_configs',
      default='',
      help='Additional compile options for Vitis HLS. '
           ' E.g., --other-hls-configs "config_compile -unsafe_math_optimizations" ',
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
      '--run-hls',
      action='count',
      dest='run_hls',
      help='Run HLS and generate RTL tarballs.',
  )
  group.add_argument(
      '--generate-task-rtl',
      action='count',
      dest='generate_task_rtl',
      help='Generate the RTL for each task',
  )
  group.add_argument(
      '--run-floorplanning',
      action='count',
      dest='run_floorplanning',
      help='Floorplan the design.',
  )
  group.add_argument(
      '--generate-top-rtl',
      action='count',
      dest='generate_top_rtl',
      help='Generate the RTL for the top-level task',
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
  # this is a helper option with a straightforward name to
  # help users enable floorplan. If provided, parsers will enforce
  # the option of --floorplan-output
  group.add_argument(
      '--enable-floorplan',
      action='store_true',
      dest='enable_floorplan',
      default=False,
      help='Enable the floorplanning step. This option could be skipped if '
           'the --floorplan-output option is given.',
  )
  group.add_argument(
      '--run-floorplan-dse',
      action='store_true',
      dest='run_floorplan_dse',
      default=False,
      help='Generate multiple floorplan configurations',
  )
  group.add_argument(
      '--floorplan-dse-step',
      type=int,
      dest='floorplan_dse_step',
      metavar='INT',
      default=500,
      help=('The minimal gap of slr_crossing_width between two design points.'),
  )
  group.add_argument(
      '--enable-hbm-binding-adjustment',
      action='store_true',
      dest='enable_hbm_binding_adjustment',
      default=False,
      help='Allow the top arguments to be binded to different physical ports '
           'based on the floorplan results. Overwrite the binding from the '
           '--connectivity option',
  )
  group.add_argument(
      '--floorplan-output',
      type=argparse.FileType('w'),
      dest='floorplan_output',
      metavar='file',
      help='Specify the name of the output tcl file that encodes the floorplan results. '
           'If provided this option, floorplan will be enabled automatically.',
  )
  group.add_argument(
      '--constraint',
      type=argparse.FileType('w'),
      dest='floorplan_output',
      metavar='file',
      help='[deprecated] Specify the name of the output tcl file that encodes the floorplan results.',
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
      '--min-area-limit',
      type=float,
      dest='min_area_limit',
      default=0.65,
      metavar='0-1',
      help=('The floorplanner will try to find solution with the resource usage '
            'of each slot betweeen min-area-limit and max-area-limit'),
  )
  group.add_argument(
      '--max-area-limit',
      type=float,
      dest='max_area_limit',
      default=0.85,
      metavar='0-1',
      help=('The floorplanner will try to find solution with the resource usage '
            'of each slot betweeen min-area-limit and max-area-limit'),
  )
  group.add_argument(
      '--min-slr-width-limit',
      type=int,
      dest='min_slr_width_limit',
      default=10000,
      help=('The floorplanner will try to find solution with the number of SLR crossing wires '
            'of each die boundary betweeen min-slr-width-limit and max-slr-width-limit'),
  )
  group.add_argument(
      '--max-slr-width-limit',
      type=int,
      dest='max_slr_width_limit',
      default=15000,
      help=('The floorplanner will try to find solution with the number of SLR crossing wires '
            'of each die boundary betweeen min-slr-width-limit and max-slr-width-limit'),
  )
  group.add_argument(
      '--max-search-time',
      type=int,
      dest='max_search_time',
      default=600,
      help=('The max runtime (in seconds) of each ILP solving process'),
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
      '--max-parallel-synth-jobs',
      type=int,
      dest='max_parallel_synth_jobs',
      default=8,
      help='Limit the number of parallel synthesize jobs if enable_synth_util is set',
  )
  group.add_argument(
      '--floorplan-pre-assignments',
      type=argparse.FileType('r'),
      dest='floorplan_pre_assignments',
      help='Providing a json file of type Dict[str, List[str]] '
           'storing the manual assignments to be used in floorplanning. '
           'The key is the region name, the value is a list of modules.'
           'Replace the outdated --directive option.'
  )
  group.add_argument(
      '--read-only-args',
      action='append',
      dest='read_only_args',
      default=[],
      type=str,
      help='Optionally specify which mmap/async_mmap arguments of the top function are read-only. '
           'Regular expression supported. '
  )
  group.add_argument(
      '--write-only-args',
      action='append',
      dest='write_only_args',
      default=[],
      type=str,
      help='Optionally specify which mmap/async_mmap arguments of the top function are write-only. '
           'Regular expression supported. '
  )

  strategies = parser.add_argument_group(
      title='Strategy',
      description=('Choose different strategy in floorplanning and codegen '
                   '(advanced usage).'),
  )
  strategies.add_argument(
      '--additional-fifo-pipelining',
      dest='additional_fifo_pipelining',
      action='store_true',
      help='Pipelining a FIFO whose source and destination are in the same region'
  )
  strategies.add_argument(
      '--floorplan-strategy',
      dest='floorplan_strategy',
      type=str,
      default='HALF_SLR_LEVEL_FLOORPLANNING',
      choices=['QUICK_FLOORPLANNING', 'SLR_LEVEL_FLOORPLANNING', 'HALF_SLR_LEVEL_FLOORPLANNING'],
      help='Override the automatic choosed floorplanning method. '
           'QUICK_FLOORPLANNING: use iterative bi-partitioning, which has the best scalability. '
           'Typically used for designs with hundreds of tasks. '
           'SLR_LEVEL_FLOORPLANNING: only partition the device into SLR level slots. '
           'Do not perform half-SLR-level floorplanning. '
           'HALF_SLR_LEVEL_FLOORPLANNING: partition the device into half-SLR level slots. '

  )
  strategies.add_argument(
      '--floorplan-opt-priority',
      dest='floorplan_opt_priority',
      type=str,
      default='AREA_PRIORITIZED',
      choices=['AREA_PRIORITIZED', 'SLR_CROSSING_PRIORITIZED'],
      help='AREA_PRIORITIZED: give priority to the area usage ratio of each slot. '
           'SLR_CROSSING_PRIORITIZED: give priority to the number of SLR crossing wires. '
  )
  return parser


def parse_steps(args, parser) -> Tuple[bool, str]:
  """ extract which steps of tapac to execute
  """
  manual_step_args = ('run_tapacc', 'run_hls', 'generate_task_rtl',
              'run_floorplanning', 'generate_top_rtl', 'pack_xo')

  if any(getattr(args, arg) for arg in manual_step_args):
    all_steps = False
    for idx, arg in enumerate(manual_step_args):
      if getattr(args, arg):
        last_step = arg
        _logger.info('Step %d: %s is enabled.', idx, arg)
      else:
        _logger.warning('Step %d: %s is *SKIPPED*.', idx, arg)
  else:
    _logger.info('Executing all steps of tapac')
    all_steps = True
    last_step = manual_step_args[-1]

  if last_step == 'run_tapacc' and args.output_file is None:
    parser.error('output file must be set if tapacc is the last step')
  if last_step == 'pack_xo' and args.output_file is None:
    parser.error('output file must be set if pack xo is the last step')
    if not args.output_file.endswith('.xo'):
      parser.error('the output file must be a .xo object')
  if not all_steps and last_step != 'run_tapacc' and args.work_dir is None:
    parser.error("steps beyond run tapacc won't work with --work-dir unset")

  # enable_floorplan is False by default
  # if --enable-floorplan is set, --floorplan-output must be set
  if args.enable_floorplan and args.floorplan_output is None:
    parser.error('Floorplan is enabled but floorplan output file is not provided')

  # if --floorplan-output is set, enable floorplan anyway for backward compatibility
  if not args.enable_floorplan and args.floorplan_output:
    args.enable_floorplan = True
    _logger.warning('The floorplan option is automatically enabled because a '
                    'floorplan output file is provided')

  # if floorplanning is enabled
  if (args.enable_floorplan and args.floorplan_output) or args.run_floorplan_dse:
    # check if the device is supported
    part_num = _get_device_info(parser, args)['part_num']
    if not is_part_num_supported(part_num):
      parser.error(
          f'The part_num {part_num} is not supported for floorplanning. '
          'Contact the authors to add support for this device.')

    # check if connectivity is given
    if not args.connectivity:
      parser.error('Must provide --connectivity to enable floorplanning.')

    # if target U280, warn if port binding adjustment is not enabled
    if part_num.startswith('xcu280'):
      if not args.enable_hbm_binding_adjustment:
        _logger.warning('HBM port binding adjustment is not enabled. '
                        'Use --enable-hbm-binding-adjustment to allow '
                        'optimal port binding selection.')

  if args.run_floorplan_dse:
    if not args.work_dir:
      parser.error('--work-dir must be set to enable floorplan DSE.')

  if args.enable_hbm_binding_adjustment:
    if not _get_device_info(parser, args)['part_num'].startswith('xcu280'):
      parser.error('--enable-hbm-binding-adjustment only works with U280')
    if not args.work_dir:
      parser.error('--work-dir must be set to enable automatic HBM binding adjustment.')
    if args.floorplan_strategy == 'QUICK_FLOORPLANNING':
      parser.error('--enable-hbm-binding-adjustment not supported yet for QUICK_FLOORPLANNING')

    _logger.warning('HBM port binding adjustment is enabled. The final binding may be '
                    'different from %s', args.connectivity.name)

  return all_steps, last_step


def main(argv: Optional[List[str]] = None):
  argv = sys.argv if argv is None else [sys.argv[0]] + argv
  argv = flags.FLAGS(argv, known_only=True)[1:]

  parser = create_parser()
  args = parser.parse_args(argv)

  tapa.util.setup_logging(args.verbose, args.quiet, args.work_dir)

  _logger.info('tapa version: %s', tapa.__version__)

  # RTL parsing may require a deep stack
  sys.setrecursionlimit(flags.FLAGS.recursionlimit)
  _logger.info('Python recursion limit set to %d', flags.FLAGS.recursionlimit)

  all_steps, last_step = parse_steps(args, parser)

  cflag_list = ['-std=c++17'] + args.cflags

  if args.run_floorplan_dse:
    _logger.info('Floorplan DSE is enabled')
    run_floorplan_dse(args)
    return

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
    for vendor_path in tapa.util.get_vendor_include_paths():
      vendor_include_paths += ['-isystem', vendor_path]
      _logger.info('added vendor include path `%s`', vendor_path)
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
    tapa_program_json_dict['cflags'] = cflag_list
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
    tapa_program_json = lambda: tapa_program_json_dict
  else:
    if args.input_file.endswith('.json') or args.work_dir is None:
      tapa_program_json = lambda: json.load(args.input_file)
    else:
      tapa_program_json = lambda: json.load(open(os.path.join(args.work_dir, 'program.json'), 'r'))

  tapa_program_json_obj = tapa_program_json()
  program = tapa.core.Program(tapa_program_json_obj, work_dir=args.work_dir)

  if args.frt_interface is not None and program.frt_interface is not None:
    with open(args.frt_interface, 'w') as output_fp:
      output_fp.write(program.frt_interface)

  if all_steps or args.run_hls is not None:
    program.run_hls(**_get_device_info(parser, args),
                    other_configs=args.other_hls_configs)

  if all_steps or args.generate_task_rtl is not None:
    program.generate_task_rtl(
      args.additional_fifo_pipelining,
      _get_device_info(parser, args)['part_num'],
    )

    if args.enable_synth_util:
      program.generate_post_synth_task_area(
        _get_device_info(parser, args)['part_num'],
        args.max_parallel_synth_jobs,
      )

  if all_steps or args.run_floorplanning is not None:
    if args.floorplan_output is not None:
      kwargs = {}
      floorplan_params = (
        'min_area_limit',
        'max_area_limit',
        'min_slr_width_limit',
        'max_slr_width_limit',
        'max_search_time',
        'floorplan_strategy',
        'floorplan_opt_priority',
        'enable_hbm_binding_adjustment',
      )
      for param in floorplan_params:
        if hasattr(args, param):
          kwargs[param] = getattr(args, param)

      program.run_floorplanning(
        _get_device_info(parser, args)['part_num'],
        args.connectivity,
        args.floorplan_pre_assignments,
        read_only_args=args.read_only_args,
        write_only_args=args.write_only_args,
        **kwargs,
      )

  if all_steps or args.generate_top_rtl is not None:
    program.generate_top_rtl(
        args.floorplan_output,
        args.register_level or 0,
        args.additional_fifo_pipelining,
        _get_device_info(parser, args)['part_num'],
    )

  if all_steps or args.pack_xo is not None:
    try:
      with open(args.output_file, 'wb') as packed_obj:
        program.pack_rtl(packed_obj)
        _logger.info('generate the v++ xo file at %s', args.output_file)
    except:
      _logger.error('Fail to create the v++ xo file at %s. Check if you have write'
                    'permission', args.output_file)

    # trim the '.xo' from the end
    if args.output_file.endswith('.xo'):
      vitis_script = args.output_file[:-3]
      script_name = f'{vitis_script}_generate_bitstream.sh'
    else:
      script_name = f'{args.output_file}_generate_bitstream.sh'

    vpp_script = get_vitis_script(args)

    try:
      with open(script_name, 'w') as script:
        script.write(vpp_script)
        _logger.info('generate the v++ script at %s', script_name)

    except:
      _logger.error('Fail to create the v++ script at %s. Check if you have write'
                    'permission', script_name)


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
