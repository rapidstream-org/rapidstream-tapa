"""The main entry point for TAPA compiler."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import sys

import click

from tapa import __version__
from tapa.steps.analyze import analyze
from tapa.steps.common import switch_work_dir
from tapa.steps.link import link
from tapa.steps.meta import compile_entry
from tapa.steps.pack import pack
from tapa.steps.synth import synth
from tapa.util import setup_logging

_logger = logging.getLogger().getChild(__name__)


@click.group(chain=True)
@click.option(
    "--verbose",
    "-v",
    default=0,
    count=True,
    help="Increase logging verbosity.",
)
@click.option(
    "--quiet",
    "-q",
    default=0,
    count=True,
    help="Decrease logging verbosity.",
)
@click.option(
    "--work-dir",
    "-w",
    metavar="DIR",
    default="./work.out/",
    type=click.Path(file_okay=False),
    help="Specify working directory.",
)
@click.option(
    "--recursion-limit",
    default=3000,
    metavar="limit",
    help="Override Python recursion limit.",
)
@click.pass_context
def entry_point(
    ctx: click.Context,
    verbose: bool,
    quiet: bool,
    work_dir: str,
    recursion_limit: int,
) -> None:
    """The TAPA compiler."""
    setup_logging(verbose, quiet, work_dir)

    # Setup execution context
    ctx.ensure_object(dict)
    switch_work_dir(work_dir)

    # Print version information
    _logger.info("tapa version: %s", __version__)

    # RTL parsing may require a deep stack
    sys.setrecursionlimit(recursion_limit)
    _logger.info("Python recursion limit set to %d", recursion_limit)


entry_point.add_command(analyze)
entry_point.add_command(synth)
entry_point.add_command(link)
entry_point.add_command(pack)
entry_point.add_command(compile_entry)

if __name__ == "__main__":
    entry_point()
