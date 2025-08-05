"""Parser/vendor-agnostic RTL width type."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import functools
from typing import NamedTuple, Union

import pyslang
from pyverilog.vparser import ast


class Width(NamedTuple):
    msb: str
    lsb: str

    @classmethod
    def create(cls, width: int | pyslang.DataTypeSyntax | None) -> Union["Width", None]:
        return _create(width)

    @classmethod
    def ast_width(cls, width: int | pyslang.DataTypeSyntax | None) -> ast.Width | None:
        return get_ast_width(cls.create(width))


@functools.singledispatch
def _create(width: object) -> Width | None:
    raise TypeError(type(width))


@_create.register
def _(width: int) -> "Width":
    assert width > 0
    return Width(msb=str(width - 1), lsb="0")


@_create.register
def _(node: pyslang.DataTypeSyntax) -> Width | None:
    assert isinstance(node, pyslang.IntegerTypeSyntax | pyslang.ImplicitTypeSyntax)
    if node.dimensions:
        range_select = node.dimensions[0][1][0]
        assert isinstance(range_select, pyslang.RangeSelectSyntax)
        return Width(
            msb=str(range_select.left),
            lsb=str(range_select.right),
        )
    return None


@_create.register
def _(_: None) -> None:
    return None


@functools.singledispatch
def get_ast_width(width: Width | None) -> ast.Width | None:
    raise TypeError(type(width))


@get_ast_width.register
def _(width: Width) -> ast.Width:
    return ast.Width(ast.Constant(width.msb), ast.Constant(width.lsb))


@get_ast_width.register
def _(_: None) -> None:
    return None
