# ruff: noqa: INP001

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""
import pytest


def pytest_addoption(parser: pytest.Parser) -> None:
    """Add custom bazel args to pytest."""
    parser.addoption(
        "--test",
        action="store",
        default="default_value",
        help="A custom argument for the test",
    )
