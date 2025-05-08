"""Data structure to represent a module grouping multiple submodules."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from collections.abc import Generator
from typing import Literal, Self

from tapa.graphir.types.commons.name import NamedModel
from tapa.graphir.types.expressions import Expression
from tapa.graphir.types.modules.definitions.base import BaseModuleDefinition
from tapa.graphir.types.modules.instantiation import ModuleInstantiation
from tapa.graphir.types.modules.supports import ModuleNet


class GroupedModuleDefinition(BaseModuleDefinition):
    """Definition of a computation module grouping multiple submodules.

    A grouped module definition contains zero or more submodules, which are
    potentially connected to each other via wire definitions in the grouped
    module, to port definitions of the grouped module, or to constants.
    The definition of the top-level module of a project is a grouped module.

    Attributes:
        submodules (tuple[ModuleInstantiation, ...]): A tuple of instantiation
            of submodules, including their names and connections.
        wires (tuple[ModuleNet, ...]): The definition of wires inside the
            module group, which may be connected to submodules.

    Examples:
        >>> import json
        >>> print(
        ...     json.dumps(
        ...         GroupedModuleDefinition(
        ...             name="empty_module",
        ...             hierarchical_name=None,
        ...             parameters=[],
        ...             ports=[],
        ...             submodules=[],
        ...             wires=[],
        ...         ).model_dump()
        ...     )
        ... )
        ... # doctest: +NORMALIZE_WHITESPACE
        {"name": "empty_module", "hierarchical_name": null,
         "module_type": "grouped_module", "parameters": [], "ports": [],
         "metadata": null, "submodules": [], "wires": []}

        >>> GroupedModuleDefinition.model_validate_json(
        ...     '''{
        ...     "name": "nesting_module", "hierarchical_name": null,
        ...     "parameters": [],
        ...     "ports": [
        ...         {"name": "test_port", "hierarchical_name": null,
        ...          "type": "input wire",
        ...          "range": {"left": [{"type": "lit", "repr": "31"}],
        ...                   "right": [{"type": "lit", "repr": "0"}]}}],
        ...     "module_type": "grouped_module",
        ...     "submodules": [{
        ...         "name": "submodule", "hierarchical_name": null,
        ...         "module": "nested_module",
        ...         "connections": [
        ...             {"name": "p1", "hierarchical_name": null,
        ...              "expr": [{"type":"id", "repr":"t"}]},
        ...             {"name": "p2", "hierarchical_name": null,
        ...              "expr": [{"type":"lit","repr":"0"}]}],
        ...         "parameters": [{"name": "DEPTH", "hierarchical_name": null,
        ...                         "expr": [{"type": "lit", "repr": "16"}]}],
        ...         "floorplan_region": null,
        ...         "area": null
        ...     }],
        ...     "wires": [
        ...       {"name": "test_wire", "hierarchical_name": null, "range": null}]}'''
        ... )
        ... # doctest: +NORMALIZE_WHITESPACE
        GroupedModuleDefinition(name='nesting_module',
            hierarchical_name=HierarchicalName(root=None),
            module_type='grouped_module',
            parameters=(),
            ports=(ModulePort(name='test_port',
                hierarchical_name=HierarchicalName(root=None),
                type='input wire',
                range='31:0'),),
            metadata=None,
            submodules=(ModuleInstantiation(name='submodule',
                hierarchical_name=HierarchicalName(root=None),
                module='nested_module',
                connections=(ModuleConnection(name='p1',
                        hierarchical_name=HierarchicalName(root=None),
                        expr='t'),
                    ModuleConnection(name='p2',
                        hierarchical_name=HierarchicalName(root=None),
                        expr='0')),
                parameters=(ModuleConnection(name='DEPTH',
                    hierarchical_name=HierarchicalName(root=None),
                    expr='16'),),
                floorplan_region=None,
                area=None, pragmas=()),),
            wires=(ModuleNet(name='test_wire',
                hierarchical_name=HierarchicalName(root=None),
                range=None),))
    """

    module_type: Literal["grouped_module"] = "grouped_module"

    submodules: tuple[ModuleInstantiation, ...]
    wires: tuple[ModuleNet, ...]

    def get_all_named(self) -> Generator[NamedModel]:
        """Yields all the named objects in the namespace."""
        yield from self.ports
        yield from self.parameters
        yield from self.submodules
        yield from self.wires

    @classmethod
    def sanitze_fields(cls, **kwargs: object) -> dict[str, object]:
        """Sort the tuple arguments by name and return the arguments."""
        cls.sort_tuple_field(kwargs, "submodules")
        cls.sort_tuple_field(kwargs, "wires")
        return super().sanitze_fields(**kwargs)

    def get_submodules(self) -> tuple[ModuleInstantiation, ...]:
        """Return the submodules of the module.

        Returns:
            tuple[ModuleInstantiation, ...]: The submodules.
        """
        return self.submodules

    def get_submodules_module_names(self) -> tuple[str, ...]:
        """Return the set of submodule module names."""
        return tuple(inst.module for inst in self.submodules)

    def get_submodule_by_inst_name(self, inst_name: str) -> ModuleInstantiation:
        """Return the submodule with the given instance name."""
        insts = [inst for inst in self.submodules if inst.name == inst_name]
        assert len(insts) == 1
        return insts[0]

    def get_used_identifiers(self) -> set[str]:
        """Get the identifiers used in the module."""
        used_idents: set[str] = set()
        for inst in self.submodules:
            for conn in inst.connections:
                used_idents |= conn.expr.get_used_identifiers()
        return used_idents

    def is_leaf_module(self) -> bool:  # noqa: PLR6301
        """Return True if the module is a leaf module.

        Returns:
            bool: Return True if the module is a leaf module.
        """
        return False

    def is_internal_module(self) -> bool:  # noqa: PLR6301
        """It is not an internal module."""
        return False

    def has_submodule(self, inst_name: str) -> bool:
        """Return True if the module has a submodule with the given name."""
        return any(inst.name == inst_name for inst in self.submodules)

    def rewritten(self, idmap: dict[str, Expression]) -> Self:
        """Rewrite the whole module definition with the given idmap.

        Args:
            idmap (dict[str, Expression]): The idmap to have identifiers rewritten.

        Returns:
            The modified module definition.
        """
        return (
            super()
            .rewritten(idmap)
            .updated(
                submodules=tuple(
                    submodule.rewritten(idmap) for submodule in self.submodules
                ),
                wires=tuple(wire.rewritten(idmap) for wire in self.wires),
            )
        )

    def assign_inst_to_region(
        self, inst_to_region: dict[str, str]
    ) -> "GroupedModuleDefinition":
        """Mark the floorplan_region field of each submodule."""
        updated_submodules = [
            inst.updated(floorplan_region=inst_to_region.get(inst.name))
            for inst in self.submodules
        ]

        return self.updated(submodules=updated_submodules)

    def get_wire(self, wire_name: str) -> ModuleNet:
        """Return the wire with the given name."""
        wires = [wire for wire in self.wires if wire.name == wire_name]
        if len(wires) == 0:
            msg = f"Wire {wire_name} not found in the module."
            raise KeyError(msg)
        if len(wires) > 1:
            msg = f"Multiple wires with the same name {wire_name}."
            raise KeyError(msg)
        return wires[0]
