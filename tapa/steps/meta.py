"""Meta command for TAPA compilation flow."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


import click

from tapa.steps.add_pipeline import add_pipeline
from tapa.steps.analyze import analyze
from tapa.steps.common import forward_applicable
from tapa.steps.floorplan import floorplan, run_autobridge
from tapa.steps.pack import pack
from tapa.steps.synth import synth


@click.command("compile")
@click.option(
    "--run-add-pipeline",
    is_flag=True,
    help=(
        "Run the add pipeline step in the compilation flow. When specified, "
        "flatten_hierarchy and gen_graphir options are specified automatically."
    ),
)
@click.pass_context
def compile_entry(ctx: click.Context, run_add_pipeline: bool, **kwargs: dict) -> None:
    """Compile a TAPA program to a hardware design."""
    if not run_add_pipeline:
        forward_applicable(ctx, analyze, kwargs)
        forward_applicable(ctx, synth, kwargs)
        forward_applicable(ctx, pack, kwargs)
    else:
        assert "floorplan_path" in kwargs, (
            "When --run-add-pipeline is specified, floorplan_path must be provided."
        )
        kwargs["flatten_hierarchy"] = True
        kwargs["gen_graphir"] = True
        forward_applicable(ctx, analyze, kwargs)
        forward_applicable(ctx, floorplan, kwargs)
        forward_applicable(ctx, synth, kwargs)
        forward_applicable(ctx, add_pipeline, kwargs)
        forward_applicable(ctx, pack, kwargs)


compile_entry.params.extend(analyze.params)
compile_entry.params.extend(floorplan.params)
compile_entry.params.extend(synth.params)
compile_entry.params.extend(add_pipeline.params)
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
