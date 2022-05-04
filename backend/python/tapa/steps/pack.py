import logging
from typing import Optional

import click

import tapa.bitstream
from . import common

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

  program = common.load_program(ctx.obj)
  with open(output, 'wb') as packed_obj:
    program.pack_rtl(packed_obj)
    _logger.info('generate the v++ xo file at %s', output)

  if bitstream_script is not None:

    class Object:
      pass

    args = Object()
    args.top = program.top
    args.output_file = output
    floorplan_output = ctx.obj.get('floorplan-output', None)
    if floorplan_output is not None:
      args.floorplan_output = Object()
      args.floorplan_output.name = floorplan_output
    connectivity = ctx.obj.get('connectivity', None)
    if connectivity is not None:
      args.connectivity = Object()
      args.connectivity = connectivity
    args.clock_period = ctx.obj.get('clock-period', None)
    args.platform = ctx.obj.get('platform', None)
    args.enable_hbm_binding_adjustment = \
      ctx.obj.get('enable-hbm-binding-adjustment', False)

    with open(bitstream_script, 'w') as script:
      script.write(tapa.bitstream.get_vitis_script(args))
      _logger.info('generate the v++ script at %s', bitstream_script)