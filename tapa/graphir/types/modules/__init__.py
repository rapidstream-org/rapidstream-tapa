"""Data types of modules."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.graphir.types.modules.definitions import (
    AnyModuleDefinition,
    AuxModuleDefinition,
    AuxSplitModuleDefinition,
    BaseModuleDefinition,
    GroupedModuleDefinition,
    InternalGroupedModuleDefinition,
    InternalModuleDefinition,
    InternalVerilogModuleDefinition,
    PassThroughModuleDefinition,
    StubModuleDefinition,
    VerilogModuleDefinition,
)
from tapa.graphir.types.modules.instantiation import ModuleInstantiation
from tapa.graphir.types.modules.supports import (
    ModuleConnection,
    ModuleNet,
    ModuleParameter,
    ModulePort,
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
    "ModuleConnection",
    "ModuleInstantiation",
    "ModuleNet",
    "ModuleParameter",
    "ModulePort",
    "PassThroughModuleDefinition",
    "StubModuleDefinition",
    "VerilogModuleDefinition",
]
