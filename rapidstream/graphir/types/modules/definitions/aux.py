"""Data structure to represent an aux module definition."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Literal

from rapidstream.graphir.types.modules.definitions.verilog import (
    VerilogModuleDefinition,
)


class AuxModuleDefinition(VerilogModuleDefinition):
    """A definition of an aux module, which is the remaining logic of a grouped module.

    When a module containing other modules is imported, the submodules are imported as
    the grouped module's submodules.  However, this module still has some other logic
    the is not part of any submodule.  This logic is represented by an aux module.

    Examples:
        >>> import json
        >>> print(
        ...     json.dumps(
        ...         AuxModuleDefinition(
        ...             parameters=[],
        ...             name="empty_aux_module",
        ...             hierarchical_name=None,
        ...             ports=[],
        ...             verilog="",
        ...             submodules_module_names=(),
        ...         ).model_dump()
        ...     )
        ... )
        ... # doctest: +NORMALIZE_WHITESPACE
        {"name": "empty_aux_module", "hierarchical_name": null,
         "module_type": "aux_module", "parameters": [],
         "ports": [], "metadata": null, "verilog": "", "submodules_module_names": []}
    """

    module_type: Literal["aux_module"] = "aux_module"  # type: ignore

    def is_internal_module(self) -> bool:
        """It is not an internal module."""
        return False


class AuxSplitModuleDefinition(VerilogModuleDefinition):
    """A definition of an aux split module.

    An aux module would be split into multiple aux split modules. Each aux split module
    contains a set of ports that are connected inside the original module.
    """

    module_type: Literal["aux_split_module"] = "aux_split_module"  # type: ignore
