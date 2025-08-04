"""Add pipeline to TAPA program."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import shutil
import subprocess
from pathlib import Path

import click

from tapa.steps.add_top_passthrough import ADD_PASSTHROUGH_PATH
from tapa.steps.common import load_tapa_program

_ADD_PIPELINE_WORK_DIR = "add_pipeline"
_EXPORTER_DIR = "exporter"

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.option(
    "--device-config",
    type=Path,
    help="Path to the device configuration file.",
)
@click.option(
    "--pipeline-config",
    type=click.Path(exists=True, dir_okay=False, readable=True, resolve_path=True),
    help="File recording the pipeline configuration",
    required=False,
)
def add_pipeline(device_config: Path | None, pipeline_config: Path | None) -> None:
    """Run external add pipeline tool."""
    if not device_config:
        _logger.error("Device configuration file is required for add pipeline.")
        msg = "Missing --device-config option."
        raise click.UsageError(msg)

    program = load_tapa_program()
    input_graphir_path = Path(program.work_dir) / ADD_PASSTHROUGH_PATH
    if not input_graphir_path.exists():
        msg = f"GraphIR file not found: {input_graphir_path}"
        raise FileNotFoundError(msg)
    output_graphir_path = Path(program.work_dir) / "graphir_add_pipeline.json"
    sol_dir = Path(program.work_dir) / _ADD_PIPELINE_WORK_DIR
    sol_dir.mkdir(parents=True, exist_ok=True)

    add_pipeline_cmd = [
        "rapidstream-optimizer",
        "-i",
        input_graphir_path,
        "-o",
        str(output_graphir_path),
        "add-pipeline",
        "--sol-dir",
        str(sol_dir),
        "--device-config",
        str(device_config),
        "--no-flatten-pipeline",
    ]

    if pipeline_config:
        add_pipeline_cmd.extend(["--pipeline-config", str(pipeline_config)])

    subprocess.run(add_pipeline_cmd, check=True)

    export_dir = Path(program.work_dir) / _EXPORTER_DIR
    export_dir.mkdir(parents=True, exist_ok=True)
    exporter_cmd = [
        "rapidstream-exporter",
        "-i",
        str(output_graphir_path),
        "-f",
        str(export_dir),
    ]

    subprocess.run(exporter_cmd, check=True)

    # Copy exported files to the program RTL directory
    for file in export_dir.iterdir():
        if file.is_file():
            shutil.copy2(file, Path(program.rtl_dir) / file.name)
