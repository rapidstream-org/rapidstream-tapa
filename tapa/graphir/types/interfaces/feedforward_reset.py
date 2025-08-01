"""Data structure to represent a reset interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any, Literal

from tapa.graphir.types.interfaces.feedforward import FeedForwardInterface


class FeedForwardResetInterface(FeedForwardInterface):
    """Interface of reset signal.

    Feedforward reset interface for hls kernel
    """

    type: Literal["ff_reset"] = "ff_reset"  # type: ignore[reportIncompatibleVariableOverride]

    def __init__(self, **kwargs: Any) -> None:  # noqa: ANN401
        """Initialize the reset interface."""
        if "rst_port" not in kwargs:
            kwargs["rst_port"] = None
        super().__init__(**kwargs)
        assert len(self.ports) == 2  # noqa: PLR2004

    def __repr__(self) -> str:
        """Represent the interface as a string."""
        return f"ff_rst{self.ports}"

    def is_clk(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False

    def is_reset(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return True

    def get_rst_port(self) -> str:
        """Get the reset port."""
        reset_ports = set(self.ports) - {self.clk_port}
        assert len(reset_ports) == 1
        return reset_ports.pop()
