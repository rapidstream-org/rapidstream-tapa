"""Data types of module definitions."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from rapidstream.graphir.types.modules.definitions.any import AnyModuleDefinition
from rapidstream.graphir.types.modules.definitions.aux import (
    AuxModuleDefinition,
    AuxSplitModuleDefinition,
)
from rapidstream.graphir.types.modules.definitions.base import BaseModuleDefinition
from rapidstream.graphir.types.modules.definitions.grouped import (
    GroupedModuleDefinition,
)
from rapidstream.graphir.types.modules.definitions.internal import (
    InternalGroupedModuleDefinition,
    InternalModuleDefinition,
    InternalVerilogModuleDefinition,
)
from rapidstream.graphir.types.modules.definitions.pass_through import (
    PassThroughModuleDefinition,
)
from rapidstream.graphir.types.modules.definitions.stub import StubModuleDefinition
from rapidstream.graphir.types.modules.definitions.verilog import (
    VerilogModuleDefinition,
)

__all__ = [
    "AnyModuleDefinition",
    "AuxModuleDefinition",
    "AuxSplitModuleDefinition",
    "BaseModuleDefinition",
    "GroupedModuleDefinition",
    "InternalGroupedModuleDefinition",
    "InternalModuleDefinition",
    "InternalVerilogModuleDefinition",
    "PassThroughModuleDefinition",
    "StubModuleDefinition",
    "VerilogModuleDefinition",
]
