"""Meta command for TAPA compilation flow."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


import logging
from pathlib import Path

import click

from tapa.steps.add_pipeline import add_pipeline
from tapa.steps.analyze import analyze
from tapa.steps.common import forward_applicable, load_tapa_program
from tapa.steps.floorplan import AUTOBRIDGE_WORK_DIR, floorplan, run_autobridge
from tapa.steps.pack import pack
from tapa.steps.synth import synth

_logger = logging.getLogger().getChild(__name__)


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
def compile_entry(ctx: click.Context, run_add_pipeline: bool, **kwargs: bool) -> None:
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
def generate_floorplan_entry(ctx: click.Context, **kwargs: bool) -> None:
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


@click.command("compile-with-floorplan-dse")
@click.pass_context
def compile_with_floorplan_dse(ctx: click.Context, **kwargs: bool | Path) -> None:
    """Compile a TAPA program with floorplan design space exploration."""
    assert kwargs["output"] is None, (
        "When using compile-with-floorplan-dse, output should not be specified. "
    )

    kwargs["flatten_hierarchy"] = True
    kwargs["enable_synth_util"] = True
    kwargs["gen_ab_graph"] = True

    # Run generate-floorplan
    forward_applicable(ctx, generate_floorplan_entry, kwargs)

    # Run the rest of the compilation flow
    # Get floorplan solutions
    program = load_tapa_program()
    autobridge_work_dir = Path(program.work_dir) / AUTOBRIDGE_WORK_DIR
    floorplan_files = autobridge_work_dir.glob("solution_*/floorplan.json")

    kwargs["gen_graphir"] = True
    kwargs["enable_synth_util"] = False
    kwargs["run_add_pipeline"] = True
    succeeded = []
    for floorplan_file in floorplan_files:
        _logger.info("Using floorplan file: %s", floorplan_file)
        clean_obj = {
            "work-dir": str(Path(ctx.obj["work-dir"]) / floorplan_file.parent.name)
        }
        kwargs["floorplan_path"] = floorplan_file
        with click.Context(
            compile_entry, info_name=compile_entry.name, obj=clean_obj
        ) as new_ctx:
            try:
                forward_applicable(new_ctx, compile_entry, kwargs)
                succeeded.append(floorplan_file)
            except Exception:
                _logger.exception(
                    "Error during compilation with floorplan %s",
                    floorplan_file,
                )
                continue

    _logger.info("Found %d successful compilations with floorplan.", len(succeeded))
    for floorplan_file in succeeded:
        _logger.info("Successful compilation with floorplan: %s", floorplan_file)


compile_with_floorplan_dse.params.extend(analyze.params)
compile_with_floorplan_dse.params.extend(floorplan.params)
compile_with_floorplan_dse.params.extend(synth.params)
compile_with_floorplan_dse.params.extend(run_autobridge.params)
compile_with_floorplan_dse.params.extend(add_pipeline.params)
compile_with_floorplan_dse.params.extend(pack.params)
