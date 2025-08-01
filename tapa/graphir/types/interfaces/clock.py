"""Data structure to represent a clock interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any, Literal

from tapa.graphir.types.interfaces.falsepath import FalsePathInterface


class ClockInterface(FalsePathInterface):
    """Interface of clock signal.

    No need to pipeline the connection regardless of distance since clock is falsepath.
    """

    type: Literal["clock"] = "clock"  # type: ignore[reportIncompatibleVariableOverride]

    def __init__(self, **kwargs: Any) -> None:  # noqa: ANN401
        """Preprocessing the input ports."""
        super().__init__(**kwargs)
        assert len(self.ports) == 1

    def __repr__(self) -> str:
        """Represent the interface as a string."""
        return f"clk({self.ports[0]}, role={self.role})"

    def is_clk(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return True

    def is_reset(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False
