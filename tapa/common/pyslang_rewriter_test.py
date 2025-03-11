"""Unit tests for tapa.common.unique_attrs."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import pytest
from pyslang import PortDeclarationSyntax, SyntaxTree

from tapa.common.pyslang_rewriter import PyslangRewriter


@pytest.fixture
def syntax_tree() -> SyntaxTree:
    return SyntaxTree.fromText("input wire foo;")


@pytest.fixture
def root(syntax_tree: SyntaxTree) -> PortDeclarationSyntax:
    assert isinstance(syntax_tree.root, PortDeclarationSyntax)
    return syntax_tree.root


@pytest.fixture
def rewriter(syntax_tree: SyntaxTree) -> PyslangRewriter:
    return PyslangRewriter(syntax_tree)


def test_commit_noop_succeeds(
    rewriter: PyslangRewriter, syntax_tree: SyntaxTree
) -> None:
    assert rewriter.commit() is syntax_tree


def test_add_before_root_start_succeeds(
    rewriter: PyslangRewriter, root: PortDeclarationSyntax
) -> None:
    rewriter.add_before(root.sourceRange.start, "output reg bar; ")
    assert str(rewriter.commit().root) == "output reg bar; input wire foo;"


def test_add_before_root_end_succeeds(
    rewriter: PyslangRewriter, root: PortDeclarationSyntax
) -> None:
    rewriter.add_before(root.sourceRange.end, " output reg bar;")
    assert str(rewriter.commit().root) == "input wire foo; output reg bar;"


def test_add_before_token_start_succeeds(
    rewriter: PyslangRewriter, root: PortDeclarationSyntax
) -> None:
    rewriter.add_before(root.semi.range.start, "/* comment */")
    assert str(rewriter.commit().root) == "input wire foo/* comment */;"


def test_add_before_node_end_succeeds(
    rewriter: PyslangRewriter, root: PortDeclarationSyntax
) -> None:
    rewriter.add_before(root.header.sourceRange.end, "/* comment */")
    assert str(rewriter.commit().root) == "input wire /* comment */foo;"


def test_remove_root_succeeds(
    rewriter: PyslangRewriter, root: PortDeclarationSyntax
) -> None:
    rewriter.remove(root.sourceRange)
    assert not str(rewriter.commit().root)


def test_remove_token_succeeds(
    rewriter: PyslangRewriter, root: PortDeclarationSyntax
) -> None:
    rewriter.remove(root.header[0].range)
    assert str(rewriter.commit().root) == " wire foo;"
