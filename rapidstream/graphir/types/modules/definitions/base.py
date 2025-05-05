"""Data structure to represent a base module definition."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from abc import abstractmethod
from typing import Self

from rapidstream.graphir.types.commons import HierarchicalNamespaceModel
from rapidstream.graphir.types.expressions import Expression
from rapidstream.graphir.types.modules.instantiation import ModuleInstantiation
from rapidstream.graphir.types.modules.supports import ModuleParameter, ModulePort


class BaseModuleDefinition(HierarchicalNamespaceModel):
    """A definition of a computation module.

    A module definition may be instantiated multiple times, or may be the root
    of a project as the top-level module.  It could be a Verilog module which
    contains no other modules, or a grouped module that consists of multiple
    submodules.  The top-level module is usually a grouped module.

    A submodel of the module definition must has a `module_type` field to
    identify its own type.

    Attributes:
        name (str): Name of the module.  The name is unique across the project.
        hierarchical_name (HierarchicalName): Hierarchical name of the module in the
            original design.  This is useful when a module has been renamed (= original
            name), or does not represent any hierarchical module in the original design
            (= None).
        parameters (tuple[ModuleParameter, ...]): An ordered tuple of parameter
            of the module and their default values.  In module instantiations,
            parameter overriding could be provided.
        ports (tuple[ModulePort, ...]): An ordered tuple of port
            definitions of this module.  In module instantiations, connections
            to the ports shall be provided.
        metadata (dict[str, str] | None): Metadata of the module.
    """

    ##############################
    #           Fields           #
    ##############################

    module_type: str = "base_module"

    parameters: tuple[ModuleParameter, ...]
    ports: tuple[ModulePort, ...]

    metadata: dict[str, str] | None = None

    @classmethod
    def sanitze_fields(cls, **kwargs: object) -> dict[str, object]:
        """Sort the ports by name and return the arguments.

        Parameters must not be sorted to prevent use before definition.
        """
        cls.sort_tuple_field(kwargs, "ports")
        return super().sanitze_fields(**kwargs)

    ###############################
    #           Methods           #
    ###############################

    def get_port(self, name: str) -> ModulePort:
        """Return the port of the given port.

        Args:
            name (str): The name of the port.

        Returns:
            ModuleModulePort: The port with the given name

        Raises:
            KeyError: If the port is not found or if the name is not a port.

        Examples:
            >>> from rapidstream.graphir.types import StubModuleDefinition
            >>> m = StubModuleDefinition.model_validate_json(
            ...     '''{
            ...     "name": "module",
            ...     "hierarchical_name": null,
            ...     "ports": [
            ...         {"name": "a", "hierarchical_name": null,
            ...          "type": "input wire", "range": null}
            ...     ], "parameters": []
            ... }'''
            ... )
            >>> m.get_port("a").type
            'input wire'

            >>> m.get_port("b")
            Traceback (most recent call last):
            KeyError: 'Object b not found in namespace module.'
        """
        port = self.lookup(name)
        if not isinstance(port, ModulePort):
            msg = f"{name} is not a port in {self.name}"
            raise KeyError(msg)
        return port

    def get_submodules(self) -> tuple[ModuleInstantiation, ...]:
        """Return the submodules of the module.

        A module with submodules shall override this method.

        Returns:
            tuple[ModuleInstantiation, ...]: The submodules.
        """
        return ()

    def get_submodules_of(self, module_name: str) -> tuple[ModuleInstantiation, ...]:
        """Return the submodules of the given module name.

        Args:
            module_name (str): The module name of the submodule.

        Returns:
            tuple[ModuleInstantiation, ...]: The submodules.
        """
        return tuple(m for m in self.get_submodules() if m.module == module_name)

    def rewritten(self, idmap: dict[str, Expression]) -> Self:
        """Rewrite the whole module definition with the given idmap.

        Args:
            idmap (dict[str, Expression]): The idmap to have identifiers rewritten.

        Returns:
            The modified module definition.
        """
        return self.updated(
            parameters=tuple(param.rewritten(idmap) for param in self.parameters),
            ports=tuple(port.rewritten(idmap) for port in self.ports),
        )

    def metadata_updated(self, new_metadata: dict[str, str]) -> Self:
        """Add and override metadata of the module."""
        if self.metadata is None:
            return self.updated(metadata=new_metadata)
        return self.updated(metadata={**self.metadata, **new_metadata})

    ########################################
    #           Abstract Methods           #
    ########################################

    @abstractmethod
    def get_submodules_module_names(self) -> tuple[str, ...]:
        """Return the module names of the submodules.

        Returns:
            set[str]: The module names of the submodules.
        """

    @abstractmethod
    def is_leaf_module(self) -> bool:
        """Return True if the module is a leaf module.

        Returns:
            bool: Return True if the module is a leaf module.
        """

    @abstractmethod
    def is_internal_module(self) -> bool:
        """Return True if the module is an internal module.

        Returns:
            bool: Return True if the module is a internal module.
        """
