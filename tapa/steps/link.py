"""Link the generated RTL with the floorplan results."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging

import click

from tapa.steps.common import (
    forward_applicable,
    is_pipelined,
    load_persistent_context,
    load_tapa_program,
    store_persistent_context,
)
from tapa.steps.synth import synth

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.pass_context
@click.option(
    "--print-fifo-ops / --no-print-fifo-ops",
    type=bool,
    default=False,
    help="Print all FIFO operations in cosim.",
)
@click.option(
    "--flow-type",
    type=click.Choice(["hls", "aie"], case_sensitive=False),
    default="hls",
    help="Flow Option: 'hls' for FPGA Fabric steps, 'aie' for Versal AIE steps.",
)
def link(
    ctx: click.Context,
    print_fifo_ops: bool,
    flow_type: str,
) -> None:
    """Link the generated RTL."""
    program = load_tapa_program()
    settings = load_persistent_context("settings")

    if flow_type == "aie":
        return

    if not is_pipelined("synth"):
        _logger.warning(
            "The `link` command must be chained after the `synth` "
            "command in a single execution.",
        )
        if settings["synthed"]:
            _logger.warning("Executing `synth` using saved options.")
            forward_applicable(ctx, synth, settings)
        else:
            msg = "Please run `synth` first."
            raise click.BadArgumentUsage(msg)

    program.generate_top_rtl(print_fifo_ops)
    program.replace_custom_rtl()

    settings["linked"] = True
    store_persistent_context("settings")

    is_pipelined("link", True)
