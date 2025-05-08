"""Workaround Nuitka to support Pydantic."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

# Nuitka does not follow to the following modules, causing Pydantic to fail to start.
# pylint: disable=unused-import
