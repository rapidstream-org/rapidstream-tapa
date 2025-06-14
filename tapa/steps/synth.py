"""Synthesize the TAPA program into RTL code."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import json
import os
from pathlib import Path
from typing import NoReturn

import click

from tapa.abgraph.gen_abgraph import get_top_level_ab_graph
from tapa.backend.xilinx import parse_device_info
from tapa.common.target import Target
from tapa.graphir_conversion.gen_rs_graphir import get_project_from_floorplanned_program
from tapa.steps.common import (
    is_pipelined,
    load_persistent_context,
    load_tapa_program,
    store_persistent_context,
)


@click.command()
@click.option(
    "--part-num",
    type=str,
    help="Target FPGA part number.  Must be specified if `--platform` is not provided.",
)
@click.option(
    "--platform",
    "-p",
    type=str,
    help="Target Vitis platform.  Must be specified if `--part-num` is not provided.",
)
@click.option("--clock-period", type=float, help="Target clock period in nanoseconds.")
@click.option(
    "--jobs",
    "-j",
    type=int,
    help="Number of parallel jobs for HLS (or RTL synthesis).",
)
@click.option(
    "--keep-hls-work-dir / --remove-hls-work-dir",
    type=bool,
    default=False,
    help="Keep HLS working directory in the TAPA work directory.",
)
@click.option(
    "--skip-hls-based-on-mtime / --no-skip-hls-based-on-mtime",
    type=bool,
    default=False,
    help=(
        "Skip HLS if an output tarball exists "
        "and is newer than the source C++ file. "
        "This can lead to incorrect results; use at your own risk."
    ),
)
@click.option(
    "--other-hls-configs",
    type=str,
    default="",
    help="Additional compile options for Vitis HLS, "
    'e.g., --other-hls-configs "config_compile -unsafe_math_optimizations"',
)
@click.option(
    "--enable-synth-util / --disable-synth-util",
    type=bool,
    default=False,
    help="Enable post-synthesis resource utilization report.",
)
@click.option(
    "--print-fifo-ops / --no-print-fifo-ops",
    type=bool,
    default=False,
    help="Print all FIFO operations in cosim.",
)
@click.option(
    "--override-report-schema-version",
    type=str,
    default="",
    help="If non-empty, overrides the schema version in generated reports.",
)
@click.option(
    "--nonpipeline-fifos",
    type=click.Path(dir_okay=False, writable=True),
    default=None,
    help=(
        "Specifies the stream FIFOs to not add pipeline to. "
        "A `grouping_constrains.json` file will be generated for rapidstream."
    ),
)
@click.option(
    "--gen-ab-graph / --no-gen-ab-graph",
    type=bool,
    default=False,
    help="Specifies whether to generate the AutoBridge graph for partitioning.",
)
@click.option(
    "--gen-graphir",
    is_flag=True,
    help="Generate GraphIR for the TAPA program.",
)
def synth(  # noqa: PLR0913,PLR0917
    part_num: str | None,
    platform: str | None,
    clock_period: float | None,
    jobs: int | None,
    keep_hls_work_dir: bool,
    skip_hls_based_on_mtime: bool,
    other_hls_configs: str,
    enable_synth_util: bool,
    print_fifo_ops: bool,
    override_report_schema_version: str,
    nonpipeline_fifos: Path | None,
    gen_ab_graph: bool,
    gen_graphir: bool,
) -> None:
    """Synthesize the TAPA program into RTL code."""
    program = load_tapa_program()
    settings = load_persistent_context("settings")
    target = Target(settings.get("target"))

    # Automatically infer the information of the given device
    device = get_device_info(part_num, platform, clock_period)
    part_num = device["part_num"]
    clock_period = float(device["clock_period"])

    # Save the context for downstream flows
    settings["part_num"] = part_num
    settings["platform"] = platform
    settings["clock_period"] = clock_period

    # Generate RTL code
    if target == Target.XILINX_AIE:
        assert platform is not None, "Platform must be specified for AIE flow."
        program.run_aie(
            clock_period,
            skip_hls_based_on_mtime,
            keep_hls_work_dir,
            platform,
        )
    elif target in {Target.XILINX_VITIS, Target.XILINX_HLS}:
        program.run_hls(
            clock_period,
            part_num,
            skip_hls_based_on_mtime,
            other_hls_configs,
            jobs,
            keep_hls_work_dir,
        )
        program.generate_task_rtl(print_fifo_ops)
        if enable_synth_util:
            program.generate_post_synth_util(part_num, jobs)
        program.generate_top_rtl(print_fifo_ops, override_report_schema_version)

        if nonpipeline_fifos:
            with open(nonpipeline_fifos, encoding="utf-8") as fifo_file:
                fifos = json.load(fifo_file)
            with open(
                os.path.join(program.work_dir, "grouping_constraints.json"),
                "w",
                encoding="utf-8",
            ) as json_file:
                result = program.get_grouping_constraints(fifos)
                json.dump(result, json_file)

        if gen_ab_graph:
            graph = get_top_level_ab_graph(program)
            with open(
                os.path.join(program.work_dir, "ab_graph.json"),
                "w",
                encoding="utf-8",
            ) as json_file:
                json_file.write(graph.model_dump_json())

        if gen_graphir:
            project = get_project_from_floorplanned_program(program)
            with open(
                os.path.join(program.work_dir, "graphir.json"),
                "w",
                encoding="utf-8",
            ) as json_file:
                json_file.write(project.model_dump_json())
                os.path.abspath(program.work_dir)

        settings["synthed"] = True
        store_persistent_context("settings")
        store_persistent_context("templates_info", program.get_rtl_templates_info())

        is_pipelined("synth", True)


def get_device_info(
    part_num: str | None,
    platform: str | None,
    clock_period: float | None,
) -> dict[str, str]:
    class ShimArgs:
        part_num: str | None
        platform: str | None
        clock_period: float | None
        dest: str
        option_strings: list[str]

    args = ShimArgs()
    args.part_num = part_num
    args.platform = platform
    args.clock_period = clock_period

    class ShimParser:
        _actions: list[ShimArgs]

        @staticmethod
        def error(message: str) -> NoReturn:
            raise click.BadArgumentUsage(message)

        @staticmethod
        def make_option_pair(dest: str, option: str) -> ShimArgs:
            arg = ShimArgs()
            arg.dest = dest
            arg.option_strings = [option]
            return arg

    parser = ShimParser()
    parser._actions = [  # noqa: SLF001
        ShimParser.make_option_pair("part_num", "--part-num"),
        ShimParser.make_option_pair("platform", "--platform"),
        ShimParser.make_option_pair("clock_period", "--clock-period"),
    ]

    return parse_device_info(
        parser,
        args,
        platform_name="platform",
        part_num_name="part_num",
        clock_period_name="clock_period",
    )
