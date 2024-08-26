"""Synthesize the TAPA program into RTL code."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import NoReturn

import click

from tapa.backend.xilinx import parse_device_info
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
    help="Target FPGA part number.  Must be specified if "
    "`--platform` is not provided.",
)
@click.option(
    "--platform",
    "-p",
    type=str,
    help="Target Vitis platform.  Must be specified if "
    "`--part-num` is not provided.",
)
@click.option("--clock-period", type=float, help="Target clock period in nanoseconds.")
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
    "--print-fifo-ops / --no-print-fifo-ops",
    type=bool,
    default=False,
    help="Print all FIFO operations in cosim.",
)
def synth(  # noqa: PLR0913,PLR0917
    part_num: str | None,
    platform: str | None,
    clock_period: float | None,
    skip_hls_based_on_mtime: bool,
    other_hls_configs: str,
    print_fifo_ops: bool,
) -> None:
    """Synthesize the TAPA program into RTL code."""
    program = load_tapa_program()
    settings = load_persistent_context("settings")

    # Automatically infer the information of the given device
    device = get_device_info(part_num, platform, clock_period)
    part_num = device["part_num"]
    clock_period = float(device["clock_period"])

    # Save the context for downstream flows
    settings["part_num"] = part_num
    settings["platform"] = platform
    settings["clock_period"] = clock_period

    # Generate RTL code
    program.run_hls(
        clock_period,
        part_num,
        skip_hls_based_on_mtime,
        other_hls_configs,
    )
    program.generate_task_rtl(print_fifo_ops)

    settings["synthed"] = True
    store_persistent_context("settings")

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
