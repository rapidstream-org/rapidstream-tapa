"""Base class of a enum that use its values as its string representation."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from enum import StrEnum


class StringEnum(StrEnum):
    """Enum with string representation."""

    def __repr__(self) -> str:
        """Use the string representation as enum's repr."""
        return repr(str(self))

    def __str__(self) -> str:
        """Use the enum value as enum's string representation."""
        return str(self.value)
