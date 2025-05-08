"""Common base data types for TAPA graph IR."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.graphir.types.commons.enum import StringEnum
from tapa.graphir.types.commons.hierarchical_name import (
    HierarchicalName,
    HierarchicalNamedModel,
    HierarchicalNamespaceModel,
)
from tapa.graphir.types.commons.model import Model, RootModel
from tapa.graphir.types.commons.mutable import MutableModel, MutableRootModel
from tapa.graphir.types.commons.name import NamedModel, NamespaceModel
from tapa.graphir.types.commons.root_mixin import (
    CommonRootMixin,
    DictLikeRootMixin,
    ListLikeRootMixin,
    TupleLikeRootMixin,
)

__all__ = [
    "CommonRootMixin",
    "DictLikeRootMixin",
    "HierarchicalName",
    "HierarchicalNamedModel",
    "HierarchicalNamespaceModel",
    "ListLikeRootMixin",
    "Model",
    "MutableModel",
    "MutableRootModel",
    "NamedModel",
    "NamespaceModel",
    "RootModel",
    "StringEnum",
    "TupleLikeRootMixin",
]
