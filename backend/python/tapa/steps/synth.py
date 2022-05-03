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
              metavar='period',
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
<<<<<<< HEAD
=======


def get_device_info(part_num: Optional[str], platform: Optional[str],
                    clock_period: Optional[float]) -> Dict[str, str]:

  class ShimArgs:
    pass

  args = ShimArgs()
  args.part_num = part_num
  args.platform = platform
  args.clock_period = clock_period

  class ShimParser:

    def error(self, message: str):
      raise click.BadArgumentUsage(message)

    @classmethod
    def make_option_pair(cls, dest: str, option: str):
      arg = ShimArgs()
      arg.dest = dest
      arg.option_strings = [option]
      return arg

  parser = ShimParser()
  parser._actions = [
      ShimParser.make_option_pair('part_num', '--part-num'),
      ShimParser.make_option_pair('platform', '--platform'),
      ShimParser.make_option_pair('clock_period', '--clock-period'),
  ]

  return haoda.backend.xilinx.parse_device_info(
      parser,
      args,
      platform_name='platform',
      part_num_name='part_num',
      clock_period_name='clock_period',
  )
>>>>>>> aa6265d (fixup! feat(tapa): new cli tapa synth)
