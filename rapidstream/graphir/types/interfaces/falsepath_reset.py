"""Data structure to represent a reset interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any, Literal

from rapidstream.graphir.types.interfaces.falsepath import FalsePathInterface


class FalsePathResetInterface(FalsePathInterface):
    """Interface of reset signal.

    No need to pipeline the connection regardless of distance since reset is falsepath.
    """

    type: Literal["fp_reset"] = "fp_reset"  # type: ignore

    def __init__(self, **kwargs: Any) -> None:
        """Initialize the reset interface."""
        super().__init__(**kwargs)
        assert len(self.ports) == 1

    def __repr__(self) -> str:
        """Represent the interface as a string."""
        return f"fp_rst({self.ports[0]})"

    def is_clk(self) -> bool:
        """Return if the interface is a clock or reset interface."""
        return False

    def is_reset(self) -> bool:
        """Return if the interface is a clock or reset interface."""
        return True

    def get_rst_port(self) -> str:
        """Get the reset port."""
        return self.ports[0]
