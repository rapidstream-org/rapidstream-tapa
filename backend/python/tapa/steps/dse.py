import json
import logging
import os
import shutil
from copy import copy
from typing import Dict

import click

import tapa.steps.analyze as analyze
import tapa.steps.common as common
import tapa.steps.link as link
import tapa.steps.optimize as optimize
import tapa.steps.pack as pack
import tapa.steps.synth as synth
from tapa.steps.common import forward_applicable

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.pass_context
@click.option(
    '--floorplan-dse-step',
    type=int,
    default=500,
    help='The minimal gap of slr_crossing_width between two design points.')
def dse_floorplan(ctx: click.Context, min_area_limit: int,
                  min_slr_width_limit: int, max_slr_width_limit: int,
                  floorplan_dse_step: int, **kwargs: Dict):

  work_dir = common.get_work_dir()
  synth_dir = os.path.abspath(os.path.join(work_dir, 'csynth'))

  log_info_header('DSE preparation: synthesize each task into RTL')
  common.switch_work_dir(synth_dir)
  forward_applicable(ctx, analyze.analyze, kwargs)
  forward_applicable(ctx, synth.synth, kwargs)

  id = 1
  while min_slr_width_limit < max_slr_width_limit:
    log_info_header(f'Start generating design point {id} with '
                    f'--max-slr-width-limit {max_slr_width_limit}')

    # Setup the design point environment and output dir
    output_dir = os.path.join(work_dir, f'run-{id}')
    common.switch_work_dir(output_dir)

    run_dir = os.path.abspath(os.path.join(output_dir, 'run'))
    shutil.copytree(synth_dir, run_dir)
    common.switch_work_dir(run_dir)

    args = copy(kwargs)
    args['output'] = os.path.join(output_dir, f'design-point.xo')
    args['floorplan_output'] = \
      os.path.join(output_dir, f'constraints.tcl')
    args['bitstream_script'] = \
      os.path.join(output_dir, f'generate-bitstream.sh')
    args['max_slr_width_limit'] = max_slr_width_limit
    args['min_area_limit'] = min_area_limit
    del args['max_area_limit']

    # Generate the design point
    forward_applicable(ctx, optimize.optimize_floorplan, args)
    forward_applicable(ctx, link.link, args)
    forward_applicable(ctx, pack.pack, args)

    config_path = os.path.join(run_dir, 'autobridge',
                               'post-floorplan-config.json')
    with open(config_path, 'r') as config_json:
      config = json.load(config_json)
      if config['floorplan_status'] != 'SUCCEED':
        _logger.info(f'Failed to generate design point {id}')
        break

      actual_slr_width = config['actual_slr_width_usage']
      actual_area_usage = config['actual_area_usage']

    # in the next iteration we decrease the slr limit, so the area usage
    # could only be higher
    min_area_limit = max(min_area_limit, actual_area_usage)

    # the slr limit in the next iteration will be at least explore_step smaller
    max_slr_width_limit = min(max_slr_width_limit - floorplan_dse_step,
                              actual_slr_width)

    id += 1


dse_floorplan.params.extend(analyze.analyze.params)
dse_floorplan.params.extend(synth.synth.params)
dse_floorplan.params.extend(optimize.optimize_floorplan.params)
dse_floorplan.params.extend(link.link.params)
dse_floorplan.params.extend(pack.pack.params)


def log_info_header(msg: str):
  _logger.info('')
  _logger.info('-' * 80)
  _logger.info(msg)
  _logger.info('-' * 80)
  _logger.info('')