"""Defining the type dispatcher to handle specific design types."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import os

from tapa.graphir.types import AnyModuleDefinition, BlackBox
from tapa.verilog.graphir_exporter.verilog import export_verilog_design


def export_design_file(root_path: str, module: AnyModuleDefinition) -> None:
    """Export a design module to the destination folder as specified type.

    Args:
        root_path (str): The project folder to export into.
        module (AnyModuleDefinition): The module to be exported.

    Examples:
        >>> from rapidstream.graphir.types import VerilogModuleDefinition
        >>> module = VerilogModuleDefinition(
        ...     name="empty",
        ...     hierarchical_name=("empty_orig_name",),
        ...     parameters=[],
        ...     ports=[],
        ...     verilog="module empty; endmodule;",
        ...     submodules_module_names=(),
        ... )
        >>> import os, tempfile
        >>> with tempfile.TemporaryDirectory() as temp_dir:
        ...     export_design_file(temp_dir, module)
        ...     print(open(os.path.join(temp_dir, "empty.v")).read())
        `timescale 1 ns / 1 ps
        module empty; endmodule;
    """
    # exporting a vivado or v++ design file
    filename = f"{module.name}.v"
    with open(os.path.join(root_path, filename), "w", encoding="utf-8") as design_file:
        design_file.write(export_verilog_design(module))


def export_blackbox_file(root_path: str, blackbox: BlackBox) -> None:
    """Export a blackbox file to the destination folder.

    Args:
        root_path (str): The project folder to export into.
        blackbox (BlackBox): The blackbox to be exported.

    Examples:
        >>> blackbox = BlackBox.from_binary("test/test.bin", b"test")
        >>> import os, tempfile
        >>> with tempfile.TemporaryDirectory() as temp_dir:
        ...     export_blackbox_file(temp_dir, blackbox)
        ...     print(open(os.path.join(temp_dir, "test/test.bin")).read())
        test
    """
    # Create the folder if it does not exist
    file_path = os.path.join(root_path, blackbox.path)
    os.makedirs(os.path.dirname(file_path), exist_ok=True)

    # Export the binary blackbox file
    with open(file_path, "wb") as file:
        file.write(blackbox.get_binary())
