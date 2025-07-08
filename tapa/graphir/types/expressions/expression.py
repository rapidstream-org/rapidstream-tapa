"""Data structure to represent an expression."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import re
from typing import Any

from tapa.graphir.third_party.systemverilog.const import eval_verilog_const_no_exception
from tapa.graphir.types.commons import Model, RootModel, StringEnum, TupleLikeRootMixin


class Token(Model):
    """A token of the expression.

    Attributes:
        type (Token.Type): The type of the expression token.
        repr (str): The string representation of the expression token.
    """

    class Type(StringEnum):
        """Type of a token of the expression.

        ID is an identifier in the expression.  And LIT is a literal token
        extracted from the expression.

        Examples:
            >>> Token.Type("id")
            'id'
        """

        ID = "id"
        LIT = "lit"

    type: "Token.Type"
    repr: str

    def __init__(self, **kwargs: Any) -> None:  # noqa: ANN401
        """Preprocessing the input ports."""
        rep = kwargs["repr"]

        # an id can only contain letters, numbers, and underscore
        if kwargs["type"] == Token.Type.ID:
            assert re.fullmatch(r"[^0-9][a-zA-Z_0-9]*", rep), (
                f"Invalid identifier: {rep}"
            )

        super().__init__(**kwargs)

    @classmethod
    def new_id(cls, ident: str) -> "Token":
        """Get an expression part pointing to an identifier."""
        return Token(type=Token.Type.ID, repr=ident)

    @classmethod
    def new_lit(cls, lit: str) -> "Token":
        """Get an expression part pointing to an identifier."""
        return Token(type=Token.Type.LIT, repr=lit)

    def is_literal(self) -> bool:
        """Return true if the token is a literal."""
        return self.type == Token.Type.LIT

    def is_id(self) -> bool:
        """Return true if the token is an identifier."""
        return self.type == Token.Type.ID

    def __repr__(self) -> str:
        """Return the string representation of the token."""
        return self.repr

    def __str__(self) -> str:
        """Return the string representation of the token."""
        return self.repr


class Expression(  # type: ignore [misc]
    # Mix-in the methods from root
    TupleLikeRootMixin[Token],
    RootModel[tuple[Token, ...]],
):
    """An expression, with all identifiers listed.

    An object of this class is an expression that may be target-dependent.
    An expression is described as a list of expression parts, which may be
    a literal part, or an identifier.

    Attributes:
        root (list[Token]): Fragments of the expression defined
            as a list of tuples, whose first element is the type of the
            fragment part, and the second element is the literal expression
            of the part.

    Examples:
        >>> import json
        >>> e = Expression((Token.new_id("a"), Token.new_lit("+"), Token.new_lit("1")))
        >>> print(json.dumps(e.model_dump()))
        ... # doctest: +NORMALIZE_WHITESPACE
        [{"type": "id", "repr": "a"},
         {"type": "lit", "repr": "+"},
         {"type": "lit", "repr": "1"}]

        Printing out the expression as a literal string:
        >>> str(e)
        'a + 1'
    """

    root: tuple[Token, ...]

    # Explicitly allow supplying one argument to make mypy happy.
    def __init__(
        self,
        value: tuple[Token, ...],
        **kwargs: Any,  # noqa: ANN401
    ) -> None:
        """Initialize the IR node."""
        # convert long expressions to constants if possible
        # e.g., ( 1 + ( 2 * 3 )) will be converted to 7
        # NOTE: only convert if {} are not in the expression. For example, {NUM{1}}
        # cannot be convert to {NUM 1}
        if (
            len(value) > 1
            and all(part.type == Token.Type.LIT for part in value)
            and ("{" not in str(value) and "}" not in str(value))
        ):
            expr_str = " ".join([part.repr for part in value])
            val = eval_verilog_const_no_exception(expr_str)
            if val:
                value = (Token.new_lit(val),)

        super().__init__(value, **kwargs)

    def __repr__(self) -> str:
        """Use the string representation as expression's repr."""
        return repr(str(self)) if self else "None"

    def __str__(self) -> str:
        """Return the literal string of the expression.

        Returns:
            str: The literal string of the expression.

        Examples:
            >>> e = Expression(
            ...     (Token.new_id("a"), Token.new_lit("+"), Token.new_lit("1"))
            ... )
            >>> str(e)
            'a + 1'
        """
        return " ".join([part.repr for part in self]) if self else ""

    def __hash__(self) -> int:  # pylint: disable=useless-super-delegation
        """Hash the expression and make static analysis happy."""
        return super().__hash__()

    def get_identifier(self) -> str:
        """Return the identifier the expression directly connects to.

        Returns:
            str: The identifier directly connects to.

        Examples:
            >>> e = Expression.new_id("a")
            >>> e.get_identifier()
            'a'
        """
        if not self.is_identifier():
            msg = "The expression is not an identifier."
            raise TypeError(msg)
        return self[0].repr

    def get_only_identifier(self) -> str | None:
        """Return the identifier the expression directly connects to without exception.

        Returns:
            str: The identifier directly connects to.

        Examples:
            >>> e = Expression.new_id("a")
            >>> e.get_identifier()
            'a'
        """
        if not self.is_identifier():
            return None
        return self[0].repr

    def get_used_identifiers(self) -> set[str]:
        """Return all identifiers used in the expression.

        Returns:
            set[str]: The set of identifiers used in the expression.

        Examples:
            >>> e = Expression(
            ...     (Token.new_id("a"), Token.new_lit("+"), Token.new_lit("1"))
            ... )
            >>> e.get_used_identifiers()
            {'a'}
        """
        return {part.repr for part in self if part.type == Token.Type.ID}

    def get_used_literals(self) -> set[str]:
        """Return all literals used in the expression.

        Returns:
            set[str]: The set of literals used in the expression.

        Examples:
            >>> e = Expression(
            ...     (Token.new_id("a"), Token.new_lit("+"), Token.new_lit("1"))
            ... )
            >>> e.get_used_literals() == {"+", "1"}
            True
        """
        return {part.repr for part in self if part.type == Token.Type.LIT}

    def is_empty(self) -> bool:
        """Return true if the expression is empty.

        Returns:
            bool: True if the expression is empty.

        Examples:
            >>> e = Expression.new_empty()
            >>> e.is_empty()
            True
            >>> e = Expression.new_id("a")
            >>> e.is_empty()
            False
        """
        return len(self) == 0

    def is_identifier(self) -> bool:
        """Return true if the expression directly connects to an identifier.

        Returns:
            bool: True if the expression directly connects to an identifier.

        Examples:
            >>> e = Expression(
            ...     (Token.new_id("a"), Token.new_lit("+"), Token.new_lit("1"))
            ... )
            >>> e.is_identifier()
            False
            >>> e = Expression.new_id("a")
            >>> e.is_identifier()
            True
        """
        return len(self) == 1 and self[0].type == Token.Type.ID

    @classmethod
    def new_empty(cls) -> "Expression":
        """Get an empty expression.

        Returns:
            Expression: The empty expression.

        Example:
            >>> e = Expression.new_empty()
            >>> print(e.model_dump_json())
            []
        """
        return Expression(())

    @classmethod
    def new_id(cls, ident: str) -> "Expression":
        """Get an expression pointing to an identifier.

        Args:
            ident (str): The identifier.

        Returns:
            Expression: The expression to the identifier.

        Example:
            >>> e = Expression.new_id("i")
            >>> print(e.model_dump_json())
            [{"type":"id","repr":"i"}]
        """
        return Expression((Token.new_id(ident),))

    @classmethod
    def new_lit(cls, lit: str) -> "Expression":
        """Get an expression with a literal expression.

        Args:
            lit (str): The literal expression.

        Returns:
            Expression: The expression of the literal expression.

        Example:
            >>> e = Expression.new_lit("1")
            >>> print(e.model_dump_json())
            [{"type":"lit","repr":"1"}]
        """
        return Expression((Token.new_lit(lit),))

    @classmethod
    def new_string_lit(cls, lit: str) -> "Expression":
        """Get an expression with a literal expression.

        Args:
            lit (str): The literal expression.

        Returns:
            Expression: The expression of the literal expression.

        Example:
            >>> e = Expression.new_lit("1")
            >>> print(e.model_dump_json())
            [{"type":"lit","repr":"1"}]
        """
        return Expression((Token.new_lit(f'"{lit}"'),))

    def rewrite(self, idmap: dict[str, "Expression"]) -> "Expression":
        """Rewrite the expression with the given map.

        Args:
            idmap (dict[str, Expression]): The map from identifiers to expressions.

        Returns:
            Expression: The expression with identifiers replaced.

        Examples:
            >>> e = Expression(
            ...     (Token.new_id("a"), Token.new_lit("+"), Token.new_lit("1"))
            ... )
            >>> str(e.rewrite({"a": e}))
            '( a + 1 ) + 1'
        """
        new_expr: list[Token] = []

        for part in self:
            if part.type == Token.Type.ID:
                # Rewrite identifiers to match the new namespace
                rewritten = idmap.get(part.repr, Expression.new_id(part.repr))
                if len(rewritten) > 1:
                    new_expr.append(Token.new_lit("("))
                new_expr += list(rewritten)
                if len(rewritten) > 1:
                    new_expr.append(Token.new_lit(")"))
            else:
                new_expr.append(part)

        return Expression(tuple(new_expr))

    @classmethod
    def from_str_to_tokens(cls, s: str) -> "Expression":
        """Convert a string to a list of graphir tokens."""
        tokens = []
        for elem in re.sub(r"([\[\]\(\)\{\}])", r" \1 ", s).split():
            if (
                elem.isdigit()
                or elem
                in {
                    "(",
                    ")",
                    "[",
                    "]",
                    "{",
                    "}",
                    "~",
                    "-",
                    "+",
                    "*",
                    "/",
                    "%",
                    "**",
                    "==",
                    "!=",
                    ">",
                    "<",
                    ">=",
                    "<=",
                    "&&",
                    "||",
                    "&",
                    "|",
                    "^",
                    "~^",
                    "<<",
                    ">>",
                    ">>>",
                }
                or any(
                    item in elem
                    for item in ("'d", "'b", "'h", "'o", "'D", "'B", "'H", "'O")
                )
            ):
                # numeric literal or operator
                tokens.append(Token.new_lit(elem))
            else:
                # identifier
                tokens.append(Token.new_id(elem))
        return Expression(tuple(tokens))

    def is_all_literals(self) -> bool:
        """Check if all tokens in the expression are literals.

        Returns:
            bool: True if all tokens are literals, False otherwise.

        Examples:
            >>> e = Expression((Token.new_lit("1"), Token.new_lit("2")))
            >>> e.is_all_literals()
            True
            >>> e = Expression((Token.new_id("a"), Token.new_lit("1")))
            >>> e.is_all_literals()
            False
        """
        return all(token.is_literal() for token in self)
