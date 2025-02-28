"""Unit tests for tapa.common.unique_attrs."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import pytest

from tapa.common.unique_attrs import UniqueAttrs


def test_init_succeeds() -> None:
    attrs = UniqueAttrs(foo="bar")
    assert attrs == {"foo": "bar"}


def test_getting_existing_attr_succeeds() -> None:
    attrs = UniqueAttrs(foo="bar")
    assert attrs.foo == "bar"


def test_getting_nonexisting_attr_fails() -> None:
    attrs = UniqueAttrs()
    with pytest.raises(KeyError, match="'foo'"):
        attrs.foo


def test_setting_new_attr_succeeds() -> None:
    attrs = UniqueAttrs()
    attrs.foo = "bar"
    assert attrs == {"foo": "bar"}


def test_setting_none_attr_succeeds() -> None:
    attrs = UniqueAttrs(foo=None)
    attrs.foo = "bar"
    assert attrs == {"foo": "bar"}


def test_setting_existing_attr_fails() -> None:
    attrs = UniqueAttrs(foo="bar")
    with pytest.raises(ValueError, match="attr 'foo' already exists: bar"):
        attrs.foo = "baz"
