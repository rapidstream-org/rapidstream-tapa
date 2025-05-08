"""Data types of a graph intermediate representation."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.graphir.types.blackbox import BlackBox
from tapa.graphir.types.commons import (
    HierarchicalName,
    HierarchicalNamedModel,
    HierarchicalNamespaceModel,
    Model,
    MutableModel,
    NamespaceModel,
)
from tapa.graphir.types.expressions import Expression, Range, Token
from tapa.graphir.types.interfaces import (
    AnyInterface,
    ApCtrlInterface,
    BaseInterface,
    ClockInterface,
    FalsePathInterface,
    FalsePathResetInterface,
    FeedForwardInterface,
    FeedForwardResetInterface,
    HandShakeInterface,
    NonPipelineInterface,
    TAPAPeekInterface,
    UnknownInterface,
)
from tapa.graphir.types.modules import (
    AnyModuleDefinition,
    AuxModuleDefinition,
    AuxSplitModuleDefinition,
    BaseModuleDefinition,
    GroupedModuleDefinition,
    InternalGroupedModuleDefinition,
    InternalModuleDefinition,
    InternalVerilogModuleDefinition,
    ModuleConnection,
    ModuleInstantiation,
    ModuleNet,
    ModuleParameter,
    ModulePort,
    PassThroughModuleDefinition,
    StubModuleDefinition,
    VerilogModuleDefinition,
)
from tapa.graphir.types.projects import Group, Groups, Interfaces, Modules, Project

__all__ = [
    "AnyInterface",
    "AnyModuleDefinition",
    "ApCtrlInterface",
    "AuxModuleDefinition",
    "AuxSplitModuleDefinition",
    "BaseInterface",
    "BaseModuleDefinition",
    "BlackBox",
    "ClockInterface",
    "Expression",
    "FalsePathInterface",
    "FalsePathResetInterface",
    "FeedForwardInterface",
    "FeedForwardResetInterface",
    "Group",
    "GroupedModuleDefinition",
    "Groups",
    "HandShakeInterface",
    "HierarchicalName",
    "HierarchicalNamedModel",
    "HierarchicalNamespaceModel",
    "Interfaces",
    "InternalGroupedModuleDefinition",
    "InternalModuleDefinition",
    "InternalVerilogModuleDefinition",
    "Model",
    "ModuleConnection",
    "ModuleInstantiation",
    "ModuleNet",
    "ModuleParameter",
    "ModulePort",
    "Modules",
    "MutableModel",
    "NamespaceModel",
    "NonPipelineInterface",
    "PassThroughModuleDefinition",
    "Project",
    "Range",
    "StubModuleDefinition",
    "TAPAPeekInterface",
    "Token",
    "UnknownInterface",
    "VerilogModuleDefinition",
]
