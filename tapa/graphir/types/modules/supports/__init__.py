"""Supporting data types of modules."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.graphir.types.modules.supports.connection import ModuleConnection
from tapa.graphir.types.modules.supports.net import ModuleNet
from tapa.graphir.types.modules.supports.parameter import ModuleParameter
from tapa.graphir.types.modules.supports.port import ModulePort

__all__ = [
    "ModuleConnection",
    "ModuleNet",
    "ModuleParameter",
    "ModulePort",
]
