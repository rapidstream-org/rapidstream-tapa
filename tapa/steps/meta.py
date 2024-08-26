"""Meta command for TAPA compilation flow."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


import click

from tapa.steps.analyze import analyze
from tapa.steps.common import forward_applicable
from tapa.steps.link import link
from tapa.steps.pack import pack
from tapa.steps.synth import synth


@click.command("compile")
@click.pass_context
def compile_entry(ctx: click.Context, **kwargs: dict) -> None:
    """Compile a TAPA program to a hardware design."""
    forward_applicable(ctx, analyze, kwargs)
    forward_applicable(ctx, synth, kwargs)
    forward_applicable(ctx, link, kwargs)
    forward_applicable(ctx, pack, kwargs)


compile_entry.params.extend(analyze.params)
compile_entry.params.extend(synth.params)
compile_entry.params.extend(link.params)
compile_entry.params.extend(pack.params)
