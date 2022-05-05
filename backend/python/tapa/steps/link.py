import os
from typing import Optional

import click

import tapa.core


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

  work_dir: str
  work_dir = ctx.obj['work-dir']

  program: tapa.core.Program
  program = ctx.obj.get('program', None)
  if program is None:
    raise click.BadArgumentUsage('Link commands must be chained after synth.')

  floorplan_output_file = None
  if ctx.obj.get('floorplanned', False):
    if floorplan_output is None:
      floorplan_output = os.path.join(work_dir, 'constraints.tcl')
    ctx.obj['floorplan-output'] = floorplan_output
    floorplan_output_file = open(floorplan_output, 'w')

  program.generate_top_rtl(
      floorplan_output_file,
      register_level,
      ctx.obj['additional-fifo-pipelining'],
      ctx.obj['part-num'],
  )
