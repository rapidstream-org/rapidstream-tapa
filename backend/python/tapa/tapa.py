#!/usr/bin/python3

import os
import logging
import sys

import click

import tapa.steps.analyze
import tapa.steps.synth
import tapa.steps.optimize
import tapa.steps.link
import tapa.steps.pack
import tapa.steps.meta
import tapa.steps.dse

import tapa.steps.common
import tapa.util

_logger = logging.getLogger().getChild(__name__)


@click.group(chain=True)
@click.option('--verbose',
              '-v',
              default=0,
              count=True,
              help='Increase logging verbosity.')
@click.option('--quiet',
              '-q',
              default=0,
              count=True,
              help='Decrease logging verbosity.')
@click.option('--work-dir',
              '-w',
              metavar='DIR',
              default='./work.out/',
              type=click.Path(file_okay=False),
              help='Specify working directory.')
@click.option('--recursion-limit',
              default=3000,
              metavar='limit',
              help='Override Python recursion limit.')
@click.pass_context
def entry_point(ctx, verbose, quiet, work_dir, recursion_limit):
  tapa.util.setup_logging(verbose, quiet, work_dir)

  # Setup execution context
  obj = ctx.ensure_object(dict)
  tapa.steps.common.switch_work_dir(work_dir)

  # Read versions from VERSION file and write log
  with open(os.path.join(os.path.dirname(tapa.__file__), 'VERSION')) as fp:
    version = fp.read().strip()
  _logger.info('tapa version: %s', version)

  # RTL parsing may require a deep stack
  sys.setrecursionlimit(recursion_limit)
  _logger.info('Python recursion limit set to %d', recursion_limit)


entry_point.add_command(tapa.steps.analyze.analyze)
entry_point.add_command(tapa.steps.synth.synth)
entry_point.add_command(tapa.steps.optimize.optimize_floorplan)
entry_point.add_command(tapa.steps.link.link)
entry_point.add_command(tapa.steps.pack.pack)
entry_point.add_command(tapa.steps.meta.compile)
entry_point.add_command(tapa.steps.dse.dse_floorplan)

if __name__ == '__main__':
  entry_point()
