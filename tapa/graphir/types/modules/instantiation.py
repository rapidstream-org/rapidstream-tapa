"""Data structure to represent the module instantiation inside a definition."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from collections.abc import Generator
from typing import TYPE_CHECKING

from tapa.graphir.assets.floorplan.instance_area import InstanceArea
from tapa.graphir.types.commons import HierarchicalNamespaceModel
from tapa.graphir.types.commons.name import NamedModel
from tapa.graphir.types.expressions import Expression
from tapa.graphir.types.modules.supports import ModuleConnection
from tapa.graphir.types.modules.supports.port import ModulePort

if TYPE_CHECKING:
    from tapa.graphir.types.modules.definitions import AnyModuleDefinition
    from tapa.graphir.types.projects.modules import Modules

_logger = logging.getLogger(__name__)


class ModuleInstantiation(HierarchicalNamespaceModel):
    """An instantiation of a computation module as a submodule.

    An object of this class instantiate a submodule inside a module definition,
    and defining the connections into the module's ports.

    Attributes:
        name (str): Name of the instance.
        hierarchical_name (HierarchicalName): Hierarchical name of the instance in the
            original design.
                The full path of a module instance is the concatenation of the
            hierarchical names of the parent modules and the hierarchical name of the
            instance, e.g., `top/group/sub`.
                A module instance's full hierarchical path remains the same even if the
            instance is renamed or moved to its parent module.  In the `top/group/sub`
            example, if the group module `group` is flattened, the hierarchical name
            of the `sub` instance becomes `group/sub`, and the full path of the instance
            remains `top/group/sub`.
        module (str): Name of the module definition, which can be looked up
            in the project namespace.
        connections (tuple[ModuleConnection, ...]): A tuple of module
            connections.
        parameters (tuple[ModuleConnection, ...]): A tuple of module parameters.
        floorplan_region (str | None): The final region determined by AutoBridge.
        area (InstanceArea | None): The resource usage of the module instance.

    Examples:
        >>> import json
        >>> from tapa.graphir.types import Expression
        >>> print(
        ...     json.dumps(
        ...         ModuleInstantiation(
        ...             module="module",
        ...             name="sub",
        ...             hierarchical_name=None,
        ...             connections=[
        ...                 ModuleConnection(
        ...                     name="port1",
        ...                     hierarchical_name=None,
        ...                     expr=Expression.new_id("test"),
        ...                 ),
        ...                 ModuleConnection(
        ...                     name="port2",
        ...                     hierarchical_name=None,
        ...                     expr=Expression.new_lit("0"),
        ...                 ),
        ...             ],
        ...             parameters=[
        ...                 ModuleConnection(
        ...                     name="DEPTH",
        ...                     hierarchical_name=None,
        ...                     expr=Expression.new_lit("16"),
        ...                 )
        ...             ],
        ...             floorplan_region=None,
        ...             area=None,
        ...         ).model_dump()
        ...     )
        ... )
        ... # doctest: +NORMALIZE_WHITESPACE
        {"name": "sub", "hierarchical_name": null, "module": "module",
            "connections":
                [{"name": "port1", "hierarchical_name": null,
                  "expr": [{"type": "id", "repr": "test"}]},
                 {"name": "port2", "hierarchical_name": null,
                  "expr": [{"type": "lit", "repr": "0"}]}],
            "parameters":
                [{"name": "DEPTH", "hierarchical_name": null,
                  "expr": [{"type": "lit", "repr": "16"}]}],
            "floorplan_region": null, "area": null, "pragmas": []}
    """

    module: str
    connections: tuple[ModuleConnection, ...]
    parameters: tuple[ModuleConnection, ...]
    floorplan_region: str | None
    area: InstanceArea | None
    pragmas: tuple[tuple[str, str], ...] = ()

    def get_pragma_string(self) -> str:
        """Return the pragma string."""
        args = []
        for key, value in self.pragmas:
            if value:
                args.append(f"{key}={value}")
            else:
                args.append(key)

        if args:
            return "(* " + ", ".join(args) + " *)"

        return ""

    def get_all_named(self) -> Generator[NamedModel]:
        """Yields all the named objects in the namespace."""
        yield from self.connections
        yield from self.parameters

    @classmethod
    def sanitze_fields(cls, **kwargs: object) -> dict[str, object]:
        """Sort the tuple arguments by name and return the arguments."""
        cls.sort_tuple_field(kwargs, "connections")
        cls.sort_tuple_field(kwargs, "parameters")
        return super().sanitze_fields(**kwargs)

    def get_parameters_used_identifiers(self) -> set[str]:
        """Return the used identifiers in the parameters.

        Returns:
            set[str]: The used identifiers.

        Examples:
            >>> from tapa.graphir.types import Expression
            >>> (
            ...     ModuleInstantiation(
            ...         name="sub",
            ...         hierarchical_name=None,
            ...         module="module",
            ...         connections=[],
            ...         parameters=[
            ...             ModuleConnection(
            ...                 name="DEPTH",
            ...                 hierarchical_name=None,
            ...                 expr=Expression.new_id("i"),
            ...             )
            ...         ],
            ...         floorplan_region=None,
            ...         area=None,
            ...     ).get_parameters_used_identifiers()
            ... )
            {'i'}
        """
        return {
            val
            for param in self.parameters
            for val in param.expr.get_used_identifiers()
        }

    def get_connection(self, port_name: str) -> ModuleConnection | None:
        """Return the connection to the requested port.

        Args:
            port_name (str): The port name of the connection.

        Returns:
            ModuleConnection: The connection into the port.

        Examples:
            >>> from tapa.graphir.types import Expression
            >>> print(
            ...     ModuleInstantiation(
            ...         name="sub",
            ...         hierarchical_name=None,
            ...         module="module",
            ...         parameters=[],
            ...         connections=[
            ...             ModuleConnection(
            ...                 name="DEPTH",
            ...                 hierarchical_name=None,
            ...                 expr=Expression.new_id("i"),
            ...             )
            ...         ],
            ...         floorplan_region=None,
            ...         area=None,
            ...     )
            ...     .get_connection("DEPTH")
            ...     .model_dump_json()
            ... )
            {"name":"DEPTH","hierarchical_name":null,"expr":[{"type":"id","repr":"i"}]}
        """
        try:
            return next(conn for conn in self.connections if conn.name == port_name)
        except StopIteration:
            return None

    def get_connection_direction(
        self, modules: "Modules", port_name: str
    ) -> ModulePort.Type:
        """Return the direction of the connection to the requested port.

        Args:
            modules (Modules): The modules namespace.
            port_name (str): The port name of the connection.

        Returns:
            ModulePort.Type: The direction of the connection into the port.
        """
        if not (module := modules.get(self.module)):
            _logger.warning(
                "%s is a blackbox, so that its connections cannot be analyzed. "
                "It is assumed to be an inout port.",
                str(self.module),
            )
            return ModulePort.Type.INOUT

        return module.get_port(port_name).type

    def get_connections_used_identifiers(self) -> set[str]:
        """Return the used identifiers in the connections.

        Returns:
            set[str]: The used identifiers.

        Examples:
            >>> from tapa.graphir.types import Expression
            >>> (
            ...     ModuleInstantiation(
            ...         name="sub",
            ...         hierarchical_name=None,
            ...         module="module",
            ...         parameters=[],
            ...         connections=[
            ...             ModuleConnection(
            ...                 name="DEPTH",
            ...                 hierarchical_name=None,
            ...                 expr=Expression.new_id("i"),
            ...             )
            ...         ],
            ...         floorplan_region=None,
            ...         area=None,
            ...     ).get_connections_used_identifiers()
            ... )
            {'i'}
        """
        return {
            val for conn in self.connections for val in conn.expr.get_used_identifiers()
        }

    def get_expression_of_port(self, port_name: str) -> Expression:
        """Get the expression connected to a port."""
        if (connection := self.get_connection(port_name)) is not None:
            return connection.expr
        return Expression.new_empty()

    def is_port_connected(self, port_name: str) -> bool:
        """Return whether the port is connected."""
        try:
            return not self.get_expression_of_port(port_name).is_empty()
        except KeyError:
            return False

    def is_port_connect_to_constant(self, port_name: str) -> bool:
        """Check if the port is connected to a constant."""
        expr = self.get_expression_of_port(port_name)
        return all(token.type != token.Type.ID for token in expr)

    def resolve_parameters(  # noqa: C901
        self, module: "AnyModuleDefinition"
    ) -> dict[str, Expression]:
        """Resolve all parameters of the module instantiation."""
        # (1) Collect the parameters and their default of the submodule.
        default_exprs: dict[str, Expression] = {}
        for def_param in module.parameters:
            default_exprs[def_param.name] = def_param.expr

        # (2) Collect the resolved parameters of the submodule instance.
        resolved_exprs: dict[str, Expression] = {}
        for ovr_param in self.parameters:
            resolved_exprs[ovr_param.name] = ovr_param.expr

        # (2b) Iterate over the default parameters and resolve them.
        while default_exprs:
            new_resolved = False

            # Remove the resolved parameters from the default parameters.
            for name in resolved_exprs:
                if name in default_exprs:
                    default_exprs.pop(name)
                    new_resolved = True

            # For all remaining default parameters, if all used identifiers are
            # resolved, resolve the parameter.
            for name, expr in default_exprs.items():
                if all(idf in resolved_exprs for idf in expr.get_used_identifiers()):
                    resolved_exprs[name] = expr.rewrite(resolved_exprs)
                    new_resolved = True

            # If no new parameter is resolved, the parameters are not resolvable.
            if not new_resolved:
                # HACK: if the unresolvable parameter has function `clog2`, we resolve
                # it to $clog2. This is a workaround for the HLS tool >= 2024.2.
                workaround = False
                for name, expr in default_exprs.items():
                    _logger.error("Cannot resolve the parameter %s = %s", name, expr)
                    if any(token.repr == "clog2" for token in expr):
                        resolved_exprs["clog2"] = Expression.new_lit("$clog2")
                        _logger.error("Workaround: Resolve clog2 = $clog2")
                        workaround = True
                if workaround:
                    continue
                # If workaround is not applied, raise an error
                msg = f"Cannot resolve the parameters of {self.name} in {module.name}."
                raise ValueError(msg)

        return resolved_exprs

    def rewritten(self, idmap: dict[str, Expression]) -> "ModuleInstantiation":
        """Rewrite the expression of the module instantiation."""
        return self.updated(
            connections=tuple(conn.rewritten(idmap) for conn in self.connections),
            parameters=tuple(param.rewritten(idmap) for param in self.parameters),
        )
