import logging
from typing import Dict

import click

import tapa.core
from tapa.steps.common import forward_applicable

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.pass_context
@click.option(
    '--connectivity',
    type=click.File(mode='r'),
    help='Input ``connectivity.ini`` specification for mmaps. '
    'This is the same file passed to ``v++``.',
)
@click.option(
    '--enable-synth-util / --disable-synth-util',
    type=bool,
    default=False,
    help='Enable post-synthesis resource utilization report '
    'for floorplanning.',
)
@click.option(
    '--enable-hbm-binding-adjustment / --disable-hbm-binding-adjustment',
    type=bool,
    default=False,
    help='Allow the top arguments to be binded to different physical ports '
    'based on the floorplan results. Overwrite the binding from the '
    '--connectivity option',
)
@click.option(
    '--max-parallel-synth-jobs',
    type=int,
    default=8,
    help=
    'Limit the number of parallel synthesize jobs if enable_synth_util is set',
)
@click.option(
    '--floorplan-pre-assignments',
    type=click.File(mode='r'),
    help='Providing a json file of type Dict[str, List[str]] '
    'storing the manual assignments to be used in floorplanning. '
    'The key is the region name, the value is a list of modules.'
    'Replace the outdated --directive option.',
)
@click.option(
    '--read-only-args',
    type=str,
    multiple=True,
    default=[],
    help='Optionally specify which mmap/async_mmap arguments of the top function '
    'are read-only. Regular expression supported.',
)
@click.option(
    '--write-only-args',
    type=str,
    multiple=True,
    default=[],
    help='Optionally specify which mmap/async_mmap arguments of the top function '
    'are write-only. Regular expression supported.',
)
@click.option(
    '--min-area-limit',
    type=float,
    default=0.65,
    metavar='0-1',
    help='The floorplanner will try to find solution with the resource usage '
    'of each slot betweeen min-area-limit and max-area-limit.',
)
@click.option(
    '--max-area-limit',
    type=float,
    default=0.85,
    metavar='0-1',
    help='The floorplanner will try to find solution with the resource usage '
    'of each slot betweeen min-area-limit and max-area-limit.',
)
@click.option(
    '--min-slr-width-limit',
    type=int,
    default=10000,
    help='The floorplanner will try to find solution with the number of SLR '
    'crossing wires of each die boundary betweeen min-slr-width-limit and '
    'max-slr-width-limit.',
)
@click.option(
    '--max-slr-width-limit',
    type=int,
    default=15000,
    help='The floorplanner will try to find solution with the number of SLR '
    'crossing wires of each die boundary betweeen min-slr-width-limit and '
    'max-slr-width-limit.',
)
@click.option(
    '--max-search-time',
    type=int,
    default=600,
    help='The max runtime (in seconds) of each ILP solving process',
)
@click.option(
    '--floorplan-strategy',
    type=click.Choice([
        'QUICK_FLOORPLANNING', 'SLR_LEVEL_FLOORPLANNING',
        'HALF_SLR_LEVEL_FLOORPLANNING'
    ]),
    default='HALF_SLR_LEVEL_FLOORPLANNING',
    help='Override the automatic choosed floorplanning method. '
    'QUICK_FLOORPLANNING: use iterative bi-partitioning, which has the '
    'best scalability. Typically used for designs with hundreds of tasks. '
    'SLR_LEVEL_FLOORPLANNING: only partition the device into SLR level '
    'slots. Do not perform half-SLR-level floorplanning. '
    'HALF_SLR_LEVEL_FLOORPLANNING: partition the device into half-SLR level '
    'slots.',
)
@click.option(
    '--floorplan-opt-priority',
    type=click.Choice(['AREA_PRIORITIZED', 'SLR_CROSSING_PRIORITIZED']),
    default='AREA_PRIORITIZED',
    help='AREA_PRIORITIZED: give priority to the area usage ratio of each '
    'slot. SLR_CROSSING_PRIORITIZED: give priority to the number of SLR '
    'crossing wires.',
)
def optimize_floorplan(ctx, **kwargs: Dict):

  if kwargs['connectivity'] is None:
    raise click.BadArgumentUsage('Missing option: --connectivity')

  program = tapa.steps.common.load_tapa_program()
  settings = tapa.steps.common.load_persistent_context('settings')

  if not tapa.steps.common.is_pipelined('synth'):
    _logger.warn('The `optimize_floorplan` command must be chained after '
                 'the `synth` command in a single execution.')
    if 'synthed' in settings:
      _logger.warn('Executing `synth` using saved options.')
      forward_applicable(ctx, tapa.steps.synth.synth, settings)
    else:
      raise click.BadArgumentUsage('Please run `synth` first.')

  kwargs['work_dir'] = tapa.steps.common.get_work_dir()
  kwargs['part_num'] = settings['part_num']

  if kwargs['enable_hbm_binding_adjustment']:
    if not kwargs['part_num'].startswith('xcu280'):
      raise click.BadArgumentUsage(
          '--enable-hbm-binding-adjustment only works with U280')
    _logger.warning(
        'HBM port binding adjustment is enabled. The final binding may be '
        'different from %s', kwargs['connectivity'].name)

  if kwargs['enable_synth_util']:
    program.generate_post_synth_task_area(kwargs['part_num'],
                                          kwargs['max_parallel_synth_jobs'])

  program.run_floorplanning(**kwargs)
  settings['connectivity'] = kwargs['connectivity'].name
  settings['enable_hbm_binding_adjustment'] = \
      kwargs['enable_hbm_binding_adjustment']

  tapa.steps.common.store_persistent_context('settings')

  tapa.steps.common.is_pipelined('optimize-floorplan', True)
