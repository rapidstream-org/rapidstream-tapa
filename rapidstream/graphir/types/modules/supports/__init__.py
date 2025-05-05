"""Supporting data types of modules."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from rapidstream.graphir.types.modules.supports.connection import ModuleConnection
from rapidstream.graphir.types.modules.supports.net import ModuleNet
from rapidstream.graphir.types.modules.supports.parameter import ModuleParameter
from rapidstream.graphir.types.modules.supports.port import ModulePort

__all__ = [
    "ModuleConnection",
    "ModuleNet",
    "ModuleParameter",
    "ModulePort",
]
