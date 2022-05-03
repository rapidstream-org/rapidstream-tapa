import logging
from typing import Dict

import click

from . import common

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.pass_context
@click.option('--part-num',
              type=str,
              help='Target FPGA part number.  Must be specified if '
              '`--platform` is not provided.')
@click.option('--platform',
              '-p',
              type=str,
              help='Target Vitis platform.  Must be specified if '
              '`--part-num` is not provided.')
@click.option('--clock-period',
              type=float,
              default=3.,
              help='Target clock period in nanoseconds.')
@click.option(
    '--additional-fifo-pipelining / --no-additional-fifo-pipelining',
    type=bool,
    default=False,
    help='Pipelining a FIFO whose source and destination are in the same region.'
)
def synth(ctx, part_num: str, clock_period: float,
          additional_fifo_pipelining: bool):

  program = common.load_program(ctx.obj)

  program.run_hls(clock_period, part_num)
  program.generate_task_rtl(additional_fifo_pipelining, part_num)
