"""Data structure to represent the connections to a submodule."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from rapidstream.graphir.types.commons import HierarchicalNamedModel
from rapidstream.graphir.types.expressions import Expression


class ModuleConnection(HierarchicalNamedModel):
    """A connection to a submodule's port or parameter.

    An object of this class defines a connection into the submodule's ports parameters.

    Attributes:
        name (str): Name of the submodule's port or parameter to be connected.
        hierarchical_name (HierarchicalName): Hierarchical name of the submodule's port
            or parameter in the original design.  This is useful when a submodule's port
            or parameter has been renamed (= original name), has been flattened into
            the parent module (= f"{chile module name}/{conn name}"), does not represent
            a wire in the original design (= wire name), or does not represent any
            hierarchical port or parameter in the original design (= None).
        expr (Expression): The expression to be connected to the port or parameter.

    Examples:
        >>> from rapidstream.graphir.types import Expression
        >>> ModuleConnection(
        ...     name="port", hierarchical_name=None, expr=Expression.new_id("i")
        ... ).model_dump_json()
        '{"name":"port","hierarchical_name":null,"expr":[{"type":"id","repr":"i"}]}'

        >>> ModuleConnection(
        ...     name="p", hierarchical_name=None, expr=Expression.new_empty()
        ... ).model_dump_json()
        '{"name":"p","hierarchical_name":null,"expr":[]}'
    """

    expr: Expression

    def rewritten(self, idmap: dict[str, Expression]) -> "ModuleConnection":
        """Rewrite the expression of the connection."""
        return self.updated(expr=self.expr.rewrite(idmap))
