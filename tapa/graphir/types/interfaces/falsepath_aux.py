"""Data structure to represent a reset interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any, Literal

from tapa.graphir.types.interfaces.falsepath import FalsePathInterface


class AuxInterface(FalsePathInterface):
    """Interface of aux module signal.

    No need to pipeline the connection regardless of distance since reset is falsepath.
    """

    type: Literal["aux"] = "aux"

    def __init__(self, **kwargs: Any) -> None:  # noqa: ANN401
        """Initialize the reset interface."""
        super().__init__(**kwargs)
        assert len(self.ports) == 1

    def __repr__(self) -> str:
        """Represent the interface as a string."""
        return f"aux({self.ports[0]})"

    def is_clk(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False

    def is_reset(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False
