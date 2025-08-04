"""Add top passthrough to TAPA program graphir."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import subprocess
from pathlib import Path

import click

from tapa.graphir.types import Project
from tapa.steps.common import load_tapa_program

ADD_PASSTHROUGH_PATH = "graphir_add_passthrough.json"

_logger = logging.getLogger().getChild(__name__)


@click.command()
def add_top_passthrough() -> None:
    """Add top passthrough to TAPA program graphir."""
    program = load_tapa_program()
    input_graphir_path = Path(program.work_dir) / "graphir.json"
    if not input_graphir_path.exists():
        msg = f"GraphIR file not found: {input_graphir_path}"
        raise FileNotFoundError(msg)
    output_graphir_path = Path(program.work_dir) / ADD_PASSTHROUGH_PATH

    cmd = [
        "rapidstream-optimizer",
        "-i",
        input_graphir_path,
        "-o",
        str(output_graphir_path),
        "flatten",
    ]

    with open(input_graphir_path, encoding="utf-8") as f:
        tapa_project: Project = Project.model_validate_json(f.read())

    for submodule in tapa_project.get_top_module().submodules:
        cmd.extend(
            [
                "--stop-at-modules",
                submodule.module,
            ]
        )

    subprocess.run(cmd, check=True)

    _logger.info("Top passthrough added to GraphIR: %s", output_graphir_path)
