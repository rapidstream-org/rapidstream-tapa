"""Data structure to represent the parameters of a submodule."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.graphir.types.commons import HierarchicalNamedModel
from tapa.graphir.types.expressions import Expression, Range


class ModuleParameter(HierarchicalNamedModel):
    """A parameter defined in a module, or passed into a submodule.

    Attributes:
        name (str): Name of the submodule's parameter to be defined.
        hierarchical_name (HierarchicalName): Hierarchical name of the parameter in the
            original design.  This is useful when a parameter has been renamed (=
            original name), has been flattened into the parent module
            (= f"{chile module name}/{parameter name}"), or does not represent any
            hierarchical parameter in the original design (= None).
        expr (Expression): The expression of the parameter.

    Examples:
        >>> import json
        >>> from tapa.graphir.types import Expression
        >>> print(
        ...     json.dumps(
        ...         ModuleParameter(
        ...             name="DEPTH",
        ...             hierarchical_name=None,
        ...             expr=Expression.new_id("D"),
        ...             range=Range(
        ...                 left=Expression.new_lit("63"), right=Expression.new_lit("0")
        ...             ),
        ...         ).model_dump()
        ...     )
        ... )
        ... # doctest: +NORMALIZE_WHITESPACE
        {"name": "DEPTH", "hierarchical_name": null,
         "expr": [{"type": "id", "repr": "D"}],
         "range": {"left": [{"type": "lit", "repr": "63"}],
                   "right": [{"type": "lit", "repr": "0"}]}}
    """

    expr: Expression
    range: Range | None = None

    def rewritten(self, idmap: dict[str, Expression]) -> "ModuleParameter":
        """Rewrite the expression of the parameter."""
        return self.updated(expr=self.expr.rewrite(idmap))
