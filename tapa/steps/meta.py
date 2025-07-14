"""Meta command for TAPA compilation flow."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


import click

from tapa.steps.analyze import analyze
from tapa.steps.common import forward_applicable
from tapa.steps.floorplan import run_autobridge
from tapa.steps.pack import pack
from tapa.steps.synth import synth


@click.command("compile")
@click.pass_context
def compile_entry(ctx: click.Context, **kwargs: dict) -> None:
    """Compile a TAPA program to a hardware design."""
    forward_applicable(ctx, analyze, kwargs)
    forward_applicable(ctx, synth, kwargs)
    forward_applicable(ctx, pack, kwargs)


compile_entry.params.extend(analyze.params)
compile_entry.params.extend(synth.params)
compile_entry.params.extend(pack.params)


@click.command("generate-floorplan")
@click.pass_context
def generate_floorplan_entry(ctx: click.Context, **kwargs: dict) -> None:
    """Generate floorplan solution for TAPA program."""
    kwargs["flatten_hierarchy"] = True
    kwargs["enable_synth_util"] = True
    kwargs["gen_ab_graph"] = True
    forward_applicable(ctx, analyze, kwargs)
    forward_applicable(ctx, synth, kwargs)
    forward_applicable(ctx, run_autobridge, kwargs)


generate_floorplan_entry.params.extend(analyze.params)
generate_floorplan_entry.params.extend(synth.params)
generate_floorplan_entry.params.extend(run_autobridge.params)


@click.command("post-floorplan-compile")
@click.pass_context
def post_floorplan_compile_entry(ctx: click.Context, **kwargs: dict) -> None:
    """Compile a TAPA program to a hardware design with post-floorplanning."""
    kwargs["flatten_hierarchy"] = True
    kwargs["gen_graphir"] = True
    forward_applicable(ctx, analyze, kwargs)
    forward_applicable(ctx, floorplan, kwargs)
    forward_applicable(ctx, synth, kwargs)
    forward_applicable(ctx, add_pipeline, kwargs)
    forward_applicable(ctx, pack, kwargs)


post_floorplan_compile_entry.params.extend(analyze.params)
post_floorplan_compile_entry.params.extend(floorplan.params)
post_floorplan_compile_entry.params.extend(synth.params)
post_floorplan_compile_entry.params.extend(add_pipeline.params)
post_floorplan_compile_entry.params.extend(pack.params)
