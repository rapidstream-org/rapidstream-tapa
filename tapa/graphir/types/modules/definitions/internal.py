"""Data structure to represent an internal module definition."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Annotated, Literal

from pydantic import Field

from tapa.graphir.types.modules.definitions.grouped import GroupedModuleDefinition
from tapa.graphir.types.modules.definitions.verilog import VerilogModuleDefinition


class InternalVerilogModuleDefinition(VerilogModuleDefinition):
    """A definition of an internal verilog module supporting tapa graphir.

    Examples:
        >>> import json
        >>> module = InternalVerilogModuleDefinition(
        ...     parameters=[],
        ...     name="empty_internal_module",
        ...     hierarchical_name=None,
        ...     ports=[],
        ...     verilog="",
        ...     submodules_module_names=(),
        ... )
        >>> print(json.dumps(module.model_dump()))
        ... # doctest: +NORMALIZE_WHITESPACE
        {"name": "empty_internal_module", "hierarchical_name": null,
        "module_type": "internal_verilog_module", "parameters": [], "ports": [],
        "metadata": null, "verilog": "", "submodules_module_names": []}

        >>> module.is_internal_module()
        True
    """

    module_type: Literal["internal_verilog_module"] = "internal_verilog_module"  # type: ignore[reportIncompatibleVariableOverride]

    def is_internal_module(self) -> bool:  # noqa: PLR6301
        """It is an internal module."""
        return True


class InternalGroupedModuleDefinition(GroupedModuleDefinition):
    """A definition of an internal grouped module supporting tapa graphir.

    Examples:
        >>> import json
        >>> module = InternalGroupedModuleDefinition(
        ...     name="empty_internal_module",
        ...     hierarchical_name=None,
        ...     parameters=(),
        ...     ports=(),
        ...     submodules=(),
        ...     wires=(),
        ... )
        >>> print(json.dumps(module.model_dump()))
        ... # doctest: +NORMALIZE_WHITESPACE
        {"name": "empty_internal_module", "hierarchical_name": null,
        "module_type": "internal_grouped_module", "parameters": [], "ports": [],
        "metadata": null, "submodules": [], "wires": []}

        >>> module.is_internal_module()
        True
    """

    module_type: Literal["internal_grouped_module"] = "internal_grouped_module"  # type: ignore[reportIncompatibleVariableOverride]

    def is_internal_module(self) -> bool:  # noqa: PLR6301
        """It is an internal module."""
        return True


InternalModuleDefinition = Annotated[
    InternalVerilogModuleDefinition | InternalGroupedModuleDefinition,
    Field(discriminator="module_type"),
]
