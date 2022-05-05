import logging
from typing import Dict, Optional

import click
import haoda.backend.xilinx

import tapa.core
import tapa.steps.common

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
              help='Target clock period in nanoseconds.')
@click.option(
    '--additional-fifo-pipelining / --no-additional-fifo-pipelining',
    type=bool,
    default=False,
    help='Pipelining a FIFO whose source and destination are in the same region.'
)
def synth(ctx, part_num: Optional[str], platform: Optional[str],
          clock_period: Optional[float], additional_fifo_pipelining: bool):

  program = tapa.steps.common.load_tapa_program()
  settings = tapa.steps.common.load_persistent_context('settings')

  # Automatically infer the information of the given device
  device = get_device_info(part_num, platform, clock_period)
  part_num = device['part_num']
  clock_period = device['clock_period']

  # Save the context for downstream flows
  settings['part-num'] = part_num
  settings['platform'] = platform
  settings['clock-period'] = clock_period
  settings['additional-fifo-pipelining'] = additional_fifo_pipelining

  # Generate RTL code
  program.run_hls(clock_period, part_num)
  program.generate_task_rtl(additional_fifo_pipelining, part_num)

  settings['synthed'] = True
  tapa.steps.common.store_persistent_context('settings')
  
  tapa.steps.common.is_pipelined('synth', True)


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
