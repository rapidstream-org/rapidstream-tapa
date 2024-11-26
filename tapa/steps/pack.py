"""Pack the generated RTL into a Xilinx object file."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import os

import click

from tapa.steps.common import is_pipelined, load_persistent_context, load_tapa_program

_logger = logging.getLogger().getChild(__name__)

VITIS_COMMAND_BASIC = [
    "v++ ${DEBUG} \\",
    "  --link \\",
    '  --output "${OUTPUT_DIR}/${TOP}_${PLATFORM}.xclbin" \\',
    "  --kernel ${TOP} \\",
    "  --platform ${PLATFORM} \\",
    "  --target ${TARGET} \\",
    "  --report_level 2 \\",
    '  --temp_dir "${OUTPUT_DIR}/${TOP}_${PLATFORM}.temp" \\',
    "  --optimize 3 \\",
    "  --connectivity.nk ${TOP}:1:${TOP} \\",
    "  --save-temps \\",
    '  "${XO}" \\',
    "  --vivado.synth.jobs ${MAX_SYNTH_JOBS} \\",
    "  --vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.IS_ENABLED=1 \\",
    "  --vivado.prop=run.impl_1.STEPS.OPT_DESIGN.ARGS.DIRECTIVE=$STRATEGY \\",
    "  --vivado.prop=run.impl_1.STEPS.PLACE_DESIGN.ARGS.DIRECTIVE=$PLACEMENT_STRATEGY \\",  # noqa: E501
    "  --vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE=$STRATEGY \\",
    "  --vivado.prop=run.impl_1.STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE=$STRATEGY \\",
]
FLOORPLAN_OPTION = [
    "  --vivado.prop=run.impl_1.STEPS.OPT_DESIGN.TCL.PRE=$CONSTRAINT \\",
]
CONFIG_OPTION = ['  --config "${CONFIG_FILE}" \\']
CLOCK_OPTION = ["  --kernel_frequency ${TARGET_FREQUENCY} \\"]

CHECK_CONSTRAINT = """
# check that the floorplan tcl exists
if [ ! -f "$CONSTRAINT" ]; then
    echo "no constraint file found"
    exit
fi
"""

NEWLINE = [""]


@click.command()
@click.option(
    "--output",
    "-o",
    type=click.Path(dir_okay=False, writable=True),
    default="work.xo",
    help="Output packed .xo Xilinx object file.",
)
@click.option(
    "--bitstream-script",
    "-s",
    type=click.Path(dir_okay=False, writable=True),
    help="Script file to generate the bitstream.",
)
def pack(output: str, bitstream_script: str | None) -> None:
    """Pack the generated RTL into a Xilinx object file."""
    program = load_tapa_program()
    settings = load_persistent_context("settings")
    if settings.get("has_template", False):
        logging.info("skip packing for template program")

    if not settings.get("linked", False):
        msg = "You must run `link` before you can `pack`."
        raise click.BadArgumentUsage(msg)

    if not settings.get("vitis-mode", True):
        logging.warning(
            "you are not in Vitis mode, the generated RTL will not be packed."
        )
        return

    program.pack_rtl(output)

    if bitstream_script is not None:
        with open(bitstream_script, "w", encoding="utf-8") as script:
            script.write(
                get_vitis_script(
                    program.top,
                    output,
                    settings.get("platform", None),
                    settings.get("clock-period", None),
                    settings.get("connectivity", None),
                )
            )
            _logger.info("generate the v++ script at %s", bitstream_script)

    is_pipelined("pack", True)


def get_vitis_script(
    top: str,
    output_file: str,
    platform: str,
    clock_period: str,
    connectivity: str | None,
) -> str:
    """Generate v++ commands to run implementation."""
    script = []
    script.append("#!/bin/bash")

    vitis_command = VITIS_COMMAND_BASIC

    script.extend(("TARGET=hw", "# TARGET=hw_emu", "# DEBUG=-g"))
    script += NEWLINE

    script.append(f"TOP={top}")
    script.append(f"XO='{os.path.abspath(output_file)}'")

    if connectivity:
        orig_config_path = os.path.abspath(connectivity)
        _logger.info(
            "use the original connectivity configuration at " "%s in the v++ script",
            orig_config_path,
        )
        script.append(f"CONFIG_FILE='{orig_config_path}'")
        vitis_command += CONFIG_OPTION
    else:
        _logger.warning(
            "No connectivity file is provided, skip this part in the v++ script.",
        )

    # if not specified in tapac, use platform default
    if clock_period:
        freq_mhz = round(1000 / float(clock_period))
        script.append(f"TARGET_FREQUENCY={freq_mhz}")
        vitis_command += CLOCK_OPTION
    else:
        script.append('>&2 echo "Using the default clock target of the platform."')

    # if platform not specified in tapac, need to manually add it
    if platform:
        script.append(f"PLATFORM={platform}")
    else:
        script.append('PLATFORM=""')
        warning_msg = (
            'Please edit this file and set a valid PLATFORM= on line "${LINENO}"'
        )
        script.append(f"if [ -z $PLATFORM ]; then echo {warning_msg}; exit; fi")
        script += NEWLINE

    script.append(r'OUTPUT_DIR="$(pwd)/vitis_run_${TARGET}"')
    script += NEWLINE

    script.append(r"MAX_SYNTH_JOBS=8")
    script.append(r'STRATEGY="Explore"')
    script.append(r'PLACEMENT_STRATEGY="EarlyBlockPlacement"')
    script += NEWLINE

    script += vitis_command
    script += NEWLINE

    return "\n".join(script)
