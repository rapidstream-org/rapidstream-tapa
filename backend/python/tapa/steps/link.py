import logging
import os
from typing import Optional

import click

import tapa.core
import tapa.steps.synth
from tapa.steps.common import forward_applicable

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.pass_context
@click.option(
    '--floorplan-output',
    type=click.Path(dir_okay=False, writable=True),
    help='Specify the output tcl file that encodes the floorplan results. '
    'Defaults to [work-dir]/constraints.tcl',
)
@click.option(
    '--register-level',
    type=int,
    default=0,
    help='Use a specific register level of top-level scalar signals '
    'instead of inferring from the floorplanning directive.',
)
def link(ctx, floorplan_output: Optional[str], register_level: int):

  program = tapa.steps.common.load_tapa_program()
  settings = tapa.steps.common.load_persistent_context('settings')

  if not tapa.steps.common.is_pipelined('synth'):
    _logger.warn('The `link` command must be chained after the `synth` '
                 'command in a single execution.')
    if settings['synthed']:
      _logger.warn('Executing `synth` using saved options.')
      forward_applicable(ctx, tapa.steps.synth.synth, settings)
    else:
      raise click.BadArgumentUsage('Please run `synth` first.')

  floorplan_output_file = None
  if floorplan_output is not None:
    if not tapa.steps.common.is_pipelined('optimize-floorplan'):
      raise click.BadArgumentUsage(
          'To write floorplaning results, the `link` command must '
          'be chained after the `optimize-floorplan` command.')

  work_dir = tapa.steps.common.get_work_dir()
  if tapa.steps.common.is_pipelined('optimize-floorplan'):
    if floorplan_output is None:
      floorplan_output = os.path.join(work_dir, 'constraints.tcl')
    settings['floorplan_output'] = floorplan_output
    floorplan_output_file = open(floorplan_output, 'w')

  program.generate_top_rtl(
      floorplan_output_file,
      register_level,
      settings['additional_fifo_pipelining'],
      settings['part_num'],
  )

  settings['linked'] = True
  tapa.steps.common.store_persistent_context('settings')

  tapa.steps.common.is_pipelined('link', True)
