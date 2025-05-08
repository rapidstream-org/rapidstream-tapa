"""Data structure to represent a clock interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any, Literal

from tapa.graphir.types.interfaces.base import BaseInterface


class FeedForwardInterface(BaseInterface):
    """Interface where all ports could be directly pipelined.

    No handshake exists in a feed_forward. All ports must have the same direction and
    must be pipelined by the same level.
    """

    type: Literal["feed_forward"] = "feed_forward"

    def __init__(self, **kwargs: Any) -> None:  # noqa: ANN401
        """Preprocessing the input ports."""
        # Feedforward interface must have clock
        assert kwargs["clk_port"]

        super().__init__(**kwargs)

    def __repr__(self) -> str:
        """Represent the interface as a string."""
        return f"ff({self.ports}, role={self.role})"

    def is_clk(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False

    def is_reset(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False

    def is_pipelinable(self) -> bool:  # noqa: PLR6301
        """Return if the interface is pipelinable."""
        return True
