"""Data structure to represent a verilog module definition."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Literal

from rapidstream.graphir.types.modules.definitions.verilog import (
    VerilogModuleDefinition,
)


class PassThroughModuleDefinition(VerilogModuleDefinition):
    """A definition of a pass through module.

    A pass-through module is a verilog module that has no internal logic. Instances of
    this module will have zero area.
    """

    module_type: Literal["pass_through_module"] = "pass_through_module"  # type: ignore
