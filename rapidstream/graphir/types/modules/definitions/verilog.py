"""Data structure to represent a verilog module definition."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import re
from collections.abc import Generator
from typing import Literal

from rapidstream.graphir.types.commons.name import NamedModel
from rapidstream.graphir.types.modules.definitions.base import BaseModuleDefinition


class VerilogModuleDefinition(BaseModuleDefinition):
    """A definition of a computation module written in Verilog.

    Attributes:
        verilog (str): The verilog source code of the module.

    Examples:
        >>> import json
        >>> print(
        ...     json.dumps(
        ...         VerilogModuleDefinition(
        ...             name="empty_module",
        ...             hierarchical_name=None,
        ...             parameters=[],
        ...             ports=[],
        ...             verilog="",
        ...             submodules_module_names=(),
        ...         ).model_dump()
        ...     )
        ... )
        ... # doctest: +NORMALIZE_WHITESPACE
        {"name": "empty_module", "hierarchical_name": null,
        "module_type": "verilog_module", "parameters": [], "ports": [],
        "metadata": null, "verilog": "", "submodules_module_names": []}

        >>> VerilogModuleDefinition.model_validate_json(
        ...     '''{
        ...     "name": "nested_module",
        ...     "hierarchical_name": null,
        ...     "parameters": [
        ...         {"name": "a", "hierarchical_name": null, "expr": []},
        ...         {"name": "b", "hierarchical_name": null,
        ...          "expr": [{"type": "lit", "repr": "1"}]}
        ...     ],
        ...     "ports": [
        ...         {"name": "a", "hierarchical_name": null,
        ...          "type": "input wire", "range": null},
        ...         {"name": "b", "hierarchical_name": null,
        ...          "type": "output wire", "range": null},
        ...         {"name": "c", "hierarchical_name": null,
        ...          "type": "input wire", "range": null}
        ...     ],
        ...     "verilog": "",
        ...     "submodules_module_names": []
        ... }'''
        ... )
        ... # doctest: +NORMALIZE_WHITESPACE
        VerilogModuleDefinition(name='nested_module',
            hierarchical_name=HierarchicalName(root=None),
            module_type='verilog_module',
            parameters=(ModuleParameter(name='a',
                    hierarchical_name=HierarchicalName(root=None),
                    expr=None, range=None),
                ModuleParameter(name='b',
                    hierarchical_name=HierarchicalName(root=None),
                    expr='1', range=None)),
            ports=(ModulePort(name='a',
                    hierarchical_name=HierarchicalName(root=None),
                    type='input wire', range=None),
                ModulePort(name='b',
                    hierarchical_name=HierarchicalName(root=None),
                    type='output wire', range=None),
                ModulePort(name='c',
                    hierarchical_name=HierarchicalName(root=None),
                    type='input wire', range=None)),
            metadata=None,
            verilog='', submodules_module_names=())
    """

    module_type: Literal["verilog_module"] = "verilog_module"

    verilog: str
    submodules_module_names: tuple[str, ...]

    def get_all_named(self) -> Generator[NamedModel]:
        """Yields all the named objects in the namespace."""
        yield from self.ports
        yield from self.parameters

    @classmethod
    def sanitze_fields(cls, **kwargs: object) -> dict[str, object]:
        """Sort the tuple arguments by name and return the arguments."""
        cls.sort_tuple_field(kwargs, "submodules_module_names")
        return super().sanitze_fields(**kwargs)

    def is_leaf_module(self) -> bool:
        """Return True if the module is a leaf module.

        Returns:
            bool: Return True if the module is a leaf module.
        """
        return True

    def get_submodules_module_names(self) -> tuple[str, ...]:
        """Return the set of submodule module names."""
        return self.submodules_module_names

    def is_internal_module(self) -> bool:
        """Return True if the module is an internal module."""
        return False

    def module_name_updated(self, new_name: str) -> "VerilogModuleDefinition":
        """Update the module name and the verilog."""
        non_word = r"(?<!\w)"  # Lookbehind for non-word character (beginning of word)
        word_boundary = r"(?!\w)"  # Lookahead for non-word character (end of word)

        # Pattern to match the module name with non-word boundaries
        name_pattern = rf"{non_word}{self.name}{word_boundary}"

        # Find all matches
        matches = re.findall(name_pattern, self.verilog)

        # Ensure there is exactly one match
        if len(matches) != 1:
            msg = (
                f"Expected exactly one match for keyword '{self.name}' in Verilog code, "
                f"but found {len(matches)} matches."
            )
            raise NotImplementedError(msg)

        # Replace the module name with the new name
        return self.updated(
            name=new_name,
            verilog=re.sub(name_pattern, f"{new_name}", self.verilog),
        )
