"""Data structure to represent a range."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.graphir.types.commons import Model
from tapa.graphir.types.expressions.expression import Expression


class Range(Model):
    """A range, with all identifiers listed.

    An object of this class represents a range that may be target-dependent.
    A range is two expressions, expressing the left-range and the right-range.

    Attributes:
        left (Expression): The left part of the range.
        right (Expression): The right part of the range.

    Examples:
        >>> import json
        >>> from tapa.graphir.types import Expression, Token
        >>> r = Range(
        ...     left=Expression(
        ...         (Token.new_id("a"), Token.new_lit("+"), Token.new_lit("1"))
        ...     ),
        ...     right=Expression.new_lit("0"),
        ... )
        >>> print(json.dumps(r.model_dump()))
        ... # doctest: +NORMALIZE_WHITESPACE
        {"left": [{"type": "id", "repr": "a"},
                  {"type": "lit", "repr": "+"},
                  {"type": "lit", "repr": "1"}],
         "right": [{"type": "lit", "repr": "0"}]}

        Printing out the range as a literal string:
        >>> str(r)
        '(a + 1):0'
    """

    left: Expression
    right: Expression

    def __repr__(self) -> str:
        """Use the string representation as its repr."""
        return repr(str(self))

    def __str__(self) -> str:
        """Return the literal string of the range.

        Returns:
            str: The literal string of the range.
        """
        left_str = f"({self.left!s})" if len(self.left) > 1 else f"{self.left!s}"
        right_str = f"({self.right!s})" if len(self.right) > 1 else f"{self.right!s}"

        return f"{left_str}:{right_str}"

    def rewrite(self, idmap: dict[str, "Expression"]) -> "Range":
        """Rewrite the range's expression with the given idmap."""
        return Range(
            left=self.left.rewrite(idmap),
            right=self.right.rewrite(idmap),
        )
