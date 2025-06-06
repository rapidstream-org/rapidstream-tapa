"""Defining the driver to use exporters."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import json
import logging
import os
import pathlib
import shutil
import sys
from glob import glob
from pathlib import Path
from typing import Any

import click

from tapa.graphir.types import Project
from tapa.verilog.graphir_exporter.dispatcher import (
    export_blackbox_file,
    export_design_file,
)

_logger = logging.getLogger(__name__)

DEFAULT_SCHEMA = "xilinx.com:schema:json_instance:1.0"


@click.command()
@click.option(
    "-f",
    "--destination",
    help="The project folder to export design into.",
    type=click.Path(file_okay=False, resolve_path=True),
    required=True,
)
@click.option(
    "-i",
    "--input-file",
    help="The input IR file",
    required=False,
    default="",
)
@click.option(
    "--binary",
    hidden=True,
    envvar="RAPIDSTREAM_EXPORTER_BINARY",
    type=click.Path(executable=True, resolve_path=True),
    required=False,
)
@click.option(
    "-v",
    "--verbose",
    count=True,
)
def main(
    destination: str,
    input_file: str,
) -> None:
    """Read a project graph IR from stdin and export the design.

    Args:
        destination (str): The project folder to export into.
        input_file (str): The input IR file.
    """
    os.makedirs(destination, exist_ok=True)

    _logger.info("Loading the project graph IR from %s", input_file)
    if input_file:
        with open(input_file, encoding="utf-8") as file:
            project = Project.model_validate_json(file.read())
    else:
        project = Project.model_validate_json(sys.stdin.read())

    export_design(
        project,
        destination,
    )


def export_design(
    project: Project,
    destination: str,
    create_stub_for_xci: bool = False,
) -> None:
    """Export the project."""
    _logger.info("Exporting project to %s.", destination)
    os.makedirs(destination, exist_ok=True)

    for module in project.modules.module_definitions:
        export_design_file(destination, module)

    for blackbox in project.blackboxes:
        export_blackbox_file(destination, blackbox)

    create_xci_sub_folder(destination)

    # generate a stub module for IPs
    # for example, elaboration and syntax check may need those stub files.
    if create_stub_for_xci:
        create_stub_files(destination)


def create_xci_sub_folder(destination: str) -> None:
    """Create a separate directory for each .xci according to Vivado requirement."""
    for source_xci_path in glob(f"{destination}/**/*.xci", recursive=True):
        xci_folder_name = pathlib.Path(source_xci_path).stem
        xci_file_basename = pathlib.Path(source_xci_path).name

        target_folder = f"{destination}/{xci_folder_name}"
        target_path = f"{target_folder}/{xci_file_basename}"

        os.makedirs(target_folder, exist_ok=True)

        # Skip if the file is already at the correct path
        if os.path.exists(target_path):
            if os.path.samefile(source_xci_path, target_path):
                continue

            # Remove and replace the file if already existing.
            _logger.warning("File %s exists, replacing with a new file.", target_path)
            os.remove(target_path)

        shutil.move(source_xci_path, target_path)


def create_stub_files(destination: str) -> None:
    """Create stub files for xci."""
    for xci_path in glob(f"{destination}/**/*.xci", recursive=True):
        xci_name = pathlib.Path(xci_path).stem
        xci_to_stub(xci_path, f"{destination}/{xci_name}.v")

    # create stub file for xilinx primitives
    with open(f"{destination}/LUT6.v", "w", encoding="utf-8") as file:
        file.write(
            """
            module LUT6 #(
                parameter INIT = 64'h0000000000000000
            )(
                input  I0,
                input  I1,
                input  I2,
                input  I3,
                input  I4,
                input  I5,
                output O
            );
            endmodule
        """
        )

    with open(f"{destination}/FDRE.v", "w", encoding="utf-8") as file:
        file.write(
            """
            module FDRE #(
                parameter INIT = 1'b0
            )(
                input C,
                input CE,
                input D,
                input R,
                output Q
            );
            endmodule
        """
        )

    with open(f"{destination}/BUFGCE.v", "w", encoding="utf-8") as file:
        file.write(
            """
            module BUFGCE (
                input  I,
                input  CE,
                output O
            );
            endmodule
        """
        )


def xci_to_stub(xci_path: str, file_path: str | None = None) -> list[str]:
    """Generate a stub file for the XCI."""
    module_name, ports = get_module_name_and_ports(xci_path)

    stub = []
    stub += [f"module {module_name} ("]

    for port_name, _port_info in ports.items():
        assert len(_port_info) == 1
        port_info = _port_info[0]

        port = "  "

        # direction is "in", "out" or "inout"
        port += {
            "in": "input ",
            "out": "output ",
            "inout": "inout ",
        }[port_info["direction"]]

        # width if available
        if "size_left" in port_info:
            assert "size_right" in port_info
            port += "[" + port_info["size_left"] + ": " + port_info["size_right"] + "] "

        # port name
        port += f"{port_name},"

        stub.append(port)

    # the last port does not have comma
    stub[-1] = stub[-1].strip(",")

    stub += [");"]
    stub += ["endmodule"]

    if file_path:
        with open(file_path, "w", encoding="utf-8") as fp:
            fp.write("\n".join(stub))

    return stub


def get_module_name_and_ports(xci_path: str) -> tuple[str, dict[str, Any]]:
    """Get the module name and ports from the XCI."""
    xci = load_xci(Path(xci_path))
    assert xci["schema"] == DEFAULT_SCHEMA
    ip_inst = xci["ip_inst"]
    module_name = ip_inst["xci_name"]

    # some IPs do not have the boundary section. Create a dummy section.
    if "boundary" not in ip_inst:
        _logger.warning("IP %s does not have interface or port info.", xci_path)
        ip_inst["boundary"] = {"ports": {}, "interfaces": {}}

    ports = ip_inst["boundary"]["ports"]

    return module_name, ports


def load_xci(xci_path: Path) -> Any:  # noqa: ANN401
    """Load the xci file as a dict.

    The XCI file may split a string into multiple lines, which is not a valid JSON. We
    need to first remove all the newlines in the string.
    """
    return json.loads(xci_path.read_text(encoding="utf-8").replace("\n", ""))


if __name__ == "__main__":
    main()  # pragma: no cover  # pylint: disable=no-value-for-parameter
