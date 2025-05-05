"""Data structure to represent a definition of a port of a module."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from rapidstream.graphir.types.commons import HierarchicalNamedModel, StringEnum
from rapidstream.graphir.types.expressions import Range
from rapidstream.graphir.types.expressions.expression import Expression, Token


class ModulePort(HierarchicalNamedModel):
    """A port of a module definition.

    Attributes:
        name (str): Name of the port as defined in the implementation.
        hierarchical_name (HierarchicalName): Hierarchical name of the port in the
            original design.  This is useful when a port has been renamed (= original
            name), has been flattened into the parent module (= f"{chile module name}/
            {port name}"), or does not represent any hierarchical port in the original
            design (= None).
        type (ModulePort.Type): Type of the port.  INPUT is going into the
            module, while OUTPUT is going out from the module, and OUTPUT_REG
            is a registered output.
        range (str): Range of the port.

    Examples:
        >>> import json
        >>> from rapidstream.graphir.types import Expression
        >>> print(
        ...     json.dumps(
        ...         ModulePort(
        ...             name="a",
        ...             hierarchical_name=None,
        ...             type=ModulePort.Type.INPUT,
        ...             range=Range(
        ...                 left=Expression.new_lit("31"), right=Expression.new_lit("0")
        ...             ),
        ...         ).model_dump()
        ...     )
        ... )
        ... # doctest: +NORMALIZE_WHITESPACE
        {"name": "a", "hierarchical_name": null, "type": "input wire",
         "range": {"left": [{"type": "lit", "repr": "31"}],
                   "right": [{"type": "lit", "repr": "0"}]}}

        >>> port = ModulePort(
        ...     name="b",
        ...     hierarchical_name=None,
        ...     type=ModulePort.Type.INPUT,
        ...     range=Range(
        ...         left=Expression.new_lit("31"), right=Expression.new_lit("0")
        ...     ),
        ... )
        >>> port.get_width_expr()
        '32'

        >>> port = ModulePort(
        ...     name="c",
        ...     hierarchical_name=None,
        ...     type=ModulePort.Type.INPUT,
        ...     range=None,
        ... )
        >>> port.get_width_expr()
        '1'

        >>> port = ModulePort(
        ...     name="d",
        ...     hierarchical_name=None,
        ...     type=ModulePort.Type.INPUT,
        ...     range=Range(left=Expression.new_id("X"), right=Expression.new_id("Y")),
        ... )
        >>> port.get_width_expr()
        '( ( X ) - ( Y ) + 1 )'
    """

    class Type(StringEnum):
        """Type of a port.

        Examples:
            >>> ModulePort.Type("input wire")
            'input wire'

            >>> ModulePort.Type("output wire")
            'output wire'

            >>> ModulePort.Type("output reg")
            'output reg'
        """

        INOUT = "inout wire"
        INPUT = "input wire"
        OUTPUT = "output wire"
        OUTPUT_REG = "output reg"

        def is_input(self) -> bool:
            """If self is an input port.

            Examples:
                >>> ModulePort.Type.INPUT.is_input()
                True

                >>> ModulePort.Type.OUTPUT.is_input()
                False
            """
            return self in {ModulePort.Type.INPUT, ModulePort.Type.INOUT}

        def is_output(self) -> bool:
            """If self is an output port.

            Examples:
                >>> ModulePort.Type.INPUT.is_output()
                False

                >>> ModulePort.Type.OUTPUT.is_output()
                True

                >>> ModulePort.Type.INOUT.is_output()
                True
            """
            return self in {
                ModulePort.Type.OUTPUT,
                ModulePort.Type.OUTPUT_REG,
                ModulePort.Type.INOUT,
            }

    type: "ModulePort.Type"
    range: Range | None

    def is_output_port(self) -> bool:
        """If self is an output port."""
        return self.type.is_output()

    def is_input_port(self) -> bool:
        """If self is an input port."""
        return self.type.is_input()

    def get_width_expr(self) -> Expression:
        """Get the expression for the width of the port."""
        if not self.range:
            return Expression((Token.new_lit("1"),))

        return Expression(
            (
                Token.new_lit("("),
                Token.new_lit("("),
                *self.range.left.root,
                Token.new_lit(")"),
                Token.new_lit("-"),
                Token.new_lit("("),
                *self.range.right.root,
                Token.new_lit(")"),
                Token.new_lit("+"),
                Token.new_lit("1"),
                Token.new_lit(")"),
            )
        )

    def rewritten(self, idmap: dict[str, Expression]) -> "ModulePort":
        """Rewrite the expression of the port."""
        if not self.range:
            return self
        return self.updated(range=self.range.rewrite(idmap))

    def has_same_direction_as(self, other: "ModulePort") -> bool:
        """Return if self has the same direction as other."""
        return (self.is_input_port() and other.is_input_port()) or (
            self.is_output_port() and other.is_output_port()
        )

    def remove_reg(self) -> "ModulePort":
        """Remove the register from the port.

        This is useful when creating wrapper for a module. The output reg port should be
        changed to output port in the wrapper definition.
        """
        if self.type == ModulePort.Type.OUTPUT_REG:
            return self.updated(type=ModulePort.Type.OUTPUT)
        return self
