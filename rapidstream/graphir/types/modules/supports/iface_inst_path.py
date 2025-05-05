"""Data structure to represent a definition of a wire in a module."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from rapidstream.graphir.types import AnyInterface
from rapidstream.graphir.types.commons import Model


class IfaceInstPath(Model):
    """Record an iface and its path."""

    inst_name: str
    iface: AnyInterface
    path: list[str]

    def __hash__(self) -> int:
        """Hash the instance name."""
        return hash(self.get_key())

    def get_key(self) -> tuple[str, int, tuple[str, ...]]:
        """Get the key of the iface."""
        return (self.inst_name, self.iface.get_key(), tuple(self.path))

    def __eq__(self, other: object) -> bool:
        """Check if the instance name is the same."""
        if not isinstance(other, IfaceInstPath):
            return False
        return self.get_key() == other.get_key()

    # enable sorting of IfaceInstPath objects
    def __lt__(self, other: "IfaceInstPath") -> bool:
        """Compare the instance name."""
        return self.get_key() < other.get_key()
