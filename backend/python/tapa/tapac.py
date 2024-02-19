#!/usr/bin/python3
import argparse
import io
import logging
import shlex
import sys
from typing import Dict, List, Optional, Tuple

import click
import haoda.backend.xilinx
from absl import flags

import tapa.core
import tapa.steps.analyze
import tapa.steps.dse
import tapa.steps.link
import tapa.steps.optimize
import tapa.steps.pack
import tapa.steps.synth
import tapa.tapa
import tapa.util
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
      help=('Enable the floorplanning step. This option could be skipped if '
            'the --floorplan-output option is given.'),
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
      help=('Allow the top arguments to be binded to different physical ports '
            'based on the floorplan results. Overwrite the binding from the '
            '--connectivity option'),
  )
  group.add_argument(
      '--floorplan-output',
      type=argparse.FileType('w'),
      dest='floorplan_output',
      metavar='file',
      help=('Specify the name of the output tcl file that encodes the '
            'floorplan results. If provided this option, floorplan will be '
            'enabled automatically.'),
  )
  group.add_argument(
      '--constraint',
      type=argparse.FileType('w'),
      dest='floorplan_output',
      metavar='file',
      help=('[deprecated] Specify the name of the output tcl file that encodes '
            'the floorplan results.'),
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
      help=('The floorplanner will try to find solution with the resource '
            'usage of each slot betweeen min-area-limit and max-area-limit'),
  )
  group.add_argument(
      '--max-area-limit',
      type=float,
      dest='max_area_limit',
      default=0.85,
      metavar='0-1',
      help=('The floorplanner will try to find solution with the resource '
            'usage of each slot betweeen min-area-limit and max-area-limit'),
  )
  group.add_argument(
      '--min-slr-width-limit',
      type=int,
      dest='min_slr_width_limit',
      default=10000,
      help=('The floorplanner will try to find solution with the number of SLR '
            'crossing wires of each die boundary betweeen min-slr-width-limit '
            'and max-slr-width-limit'),
  )
  group.add_argument(
      '--max-slr-width-limit',
      type=int,
      dest='max_slr_width_limit',
      default=15000,
      help=('The floorplanner will try to find solution with the number of SLR '
            'crossing wires of each die boundary betweeen min-slr-width-limit '
            'and max-slr-width-limit'),
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
      help=('Limit the number of parallel synthesize jobs if enable_synth_util '
            'is set'),
  )
  group.add_argument(
      '--floorplan-pre-assignments',
      type=argparse.FileType('r'),
      dest='floorplan_pre_assignments',
      help=('Providing a json file of type Dict[str, List[str]] '
            'storing the manual assignments to be used in floorplanning. '
            'The key is the region name, the value is a list of modules.'
            'Replace the outdated --directive option.'),
  )
  group.add_argument(
      '--read-only-args',
      action='append',
      dest='read_only_args',
      default=[],
      type=str,
      help=('Optionally specify which mmap/async_mmap arguments of the top '
            'function are read-only. Regular expression supported.'),
  )
  group.add_argument(
      '--write-only-args',
      action='append',
      dest='write_only_args',
      default=[],
      type=str,
      help=('Optionally specify which mmap/async_mmap arguments of the top '
            'function are write-only. Regular expression supported. '),
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
      help=('Pipelining a FIFO whose source and destination are in the same '
            'region'),
  )
  strategies.add_argument(
      '--floorplan-strategy',
      dest='floorplan_strategy',
      type=str,
      default='HALF_SLR_LEVEL_FLOORPLANNING',
      choices=[
          'QUICK_FLOORPLANNING',
          'SLR_LEVEL_FLOORPLANNING',
          'HALF_SLR_LEVEL_FLOORPLANNING',
      ],
      help=(
          'Override the automatic choosed floorplanning method. '
          'QUICK_FLOORPLANNING: use iterative bi-partitioning, which has the '
          'best scalability. Typically used for designs with hundreds of '
          'tasks. SLR_LEVEL_FLOORPLANNING: only partition the device into SLR '
          'level slots. Do not perform half-SLR-level floorplanning. '
          'HALF_SLR_LEVEL_FLOORPLANNING: partition the device into half-SLR '
          'level slots. '),
  )
  strategies.add_argument(
      '--floorplan-opt-priority',
      dest='floorplan_opt_priority',
      type=str,
      default='AREA_PRIORITIZED',
      choices=['AREA_PRIORITIZED', 'SLR_CROSSING_PRIORITIZED'],
      help=('AREA_PRIORITIZED: give priority to the area usage ratio of each '
            'slot. SLR_CROSSING_PRIORITIZED: give priority to the number of '
            'SLR crossing wires.'),
  )
  return parser


def parse_steps(args, parser) -> Tuple[bool, str]:
  """ extract which steps of tapac to execute
  """
  manual_step_args = (
      'run_tapacc',
      'run_hls',
      'generate_task_rtl',
      'run_floorplanning',
      'generate_top_rtl',
      'pack_xo',
  )

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
    parser.error(
        'Floorplan is enabled but floorplan output file is not provided')

  # if --floorplan-output is set, enable floorplan anyway for backward compatibility
  if not args.enable_floorplan and args.floorplan_output:
    args.enable_floorplan = True
    _logger.warning('The floorplan option is automatically enabled because a '
                    'floorplan output file is provided')

  # if floorplanning is enabled
  if ((args.enable_floorplan and args.floorplan_output) or
      args.run_floorplan_dse):
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
      parser.error(
          '--work-dir must be set to enable automatic HBM binding adjustment.')
    if args.floorplan_strategy == 'QUICK_FLOORPLANNING':
      parser.error('--enable-hbm-binding-adjustment not supported '
                   'yet for QUICK_FLOORPLANNING')

    _logger.warning(
        'HBM port binding adjustment is enabled. The final binding may be '
        'different from %s', args.connectivity.name)

  return all_steps, last_step


def main(argv: Optional[List[str]] = None):
  argv = sys.argv if argv is None else [sys.argv[0]] + argv
  argv = flags.FLAGS(argv, known_only=True)[1:]

  parser = create_parser()
  args = parser.parse_args(argv)

  tapa.util.setup_logging(args.verbose, args.quiet, args.work_dir)

  argv = []
  if args.run_floorplan_dse:
    argv.append('dse-floorplan')
    for param in tapa.steps.dse.dse_floorplan.params:
      argv.extend(_to_argv(args, param))
  else:
    for param in tapa.tapa.entry_point.params:
      argv.extend(_to_argv(args, param))

    all_steps, _ = parse_steps(args, parser)

    if all_steps or args.run_tapacc is not None:
      argv.append('analyze')
      for param in tapa.steps.analyze.analyze.params:
        argv.extend(_to_argv(args, param))

    if (all_steps or args.run_hls is not None or
        args.generate_task_rtl is not None):
      argv.append('synth')
      for param in tapa.steps.synth.synth.params:
        argv.extend(_to_argv(args, param))

    if ((all_steps or args.run_floorplanning is not None) and
        args.floorplan_output is not None):
      argv.append('optimize-floorplan')
      for param in tapa.steps.optimize.optimize_floorplan.params:
        argv.extend(_to_argv(args, param))

    if all_steps or args.generate_top_rtl is not None:
      argv.append('link')
      for param in tapa.steps.link.link.params:
        argv.extend(_to_argv(args, param))

    if all_steps or args.pack_xo is not None:
      argv.append('pack')
      for param in tapa.steps.pack.pack.params:
        argv.extend(_to_argv(args, param))

  _logger.info('running translated command: `tapa %s`', shlex.join(argv))
  tapa.tapa.entry_point(argv)  # pylint: disable=no-value-for-parameter


def _to_argv(args: argparse.Namespace, param: click.Parameter) -> List[str]:
  """Translate a click `param` to a list of arguments."""
  if param.name in ('input', 'output'):
    param_name = f'{param.name}_file'
  else:
    param_name = param.name

  value = getattr(args, param_name, None)
  if param_name == 'recursion_limit':
    value = flags.FLAGS.recursionlimit

  if value is None or value == param.default:
    return []
  if isinstance(value, list):
    return [f'{param.opts[0]}={v}' for v in value]
  if isinstance(value, bool):
    return [param.opts[0] if value else param.secondary_opts[0]]
  if isinstance(value, io.TextIOWrapper):
    value = value.name
  return [f'{param.opts[0]}={value}']


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
