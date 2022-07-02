import logging
from typing import Optional

import click

import tapa.bitstream
import tapa.steps.common

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.pass_context
@click.option('--output',
              '-o',
              type=click.Path(dir_okay=False, writable=True),
              default='work.xo',
              help='Output packed .xo Xilinx object file.')
@click.option('--bitstream-script',
              '-s',
              type=click.Path(dir_okay=False, writable=True),
              help='Output packed .xo Xilinx object file.')
def pack(ctx, output: str, bitstream_script: Optional[str]):

  program = tapa.steps.common.load_tapa_program()
  settings = tapa.steps.common.load_persistent_context('settings')

  if not settings.get('linked', False):
    raise click.BadArgumentUsage('You must run `link` before you can `pack`.')

  with open(output, 'wb') as packed_obj:
    program.pack_rtl(packed_obj)
    _logger.info('generate the v++ xo file at %s', output)

  if bitstream_script is not None:

    class Object:
      pass

    args = Object()
    args.top = program.top
    args.output_file = output
    floorplan_output = settings.get('floorplan-output', None)
    if floorplan_output is not None:
      args.floorplan_output = Object()
      args.floorplan_output.name = floorplan_output
    else:
      args.floorplan_output = None
    connectivity = settings.get('connectivity', None)
    if connectivity is not None:
      args.connectivity = Object()
      args.connectivity.name = connectivity
    else:
      args.connectivity = None
    args.clock_period = settings.get('clock-period', None)
    args.platform = settings.get('platform', None)
    args.enable_hbm_binding_adjustment = \
      settings.get('enable-hbm-binding-adjustment', False)

    with open(bitstream_script, 'w') as script:
      script.write(tapa.bitstream.get_vitis_script(args))
      _logger.info('generate the v++ script at %s', bitstream_script)

  tapa.steps.common.is_pipelined('pack', True)
