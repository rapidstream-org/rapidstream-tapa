"""Data structure to represent a false path interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any, Literal

from tapa.graphir.types.interfaces.base import BaseInterface


class FalsePathInterface(BaseInterface):
    """Interface where a port is connected to a false path.

    No need to pipeline the connection regardless of distance.
    """

    type: Literal["false_path"] = "false_path"  # type: ignore[reportIncompatibleVariableOverride]

    def __init__(self, **kwargs: Any) -> None:  # noqa: ANN401
        """Preprocessing the input ports."""
        if "clk_port" not in kwargs:
            kwargs["clk_port"] = None
        if "rst_port" not in kwargs:
            kwargs["rst_port"] = None

        # FalsePath interface must not have clock or reset
        assert not kwargs["clk_port"]
        assert not kwargs["rst_port"]

        super().__init__(**kwargs)

    def __repr__(self) -> str:
        """Represent the interface as a string."""
        return f"false{self.ports}"

    def get_data_ports(self) -> tuple[str, ...]:
        """All ports are data ports in feedforward interfaces."""
        return self.ports

    def is_clk(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False

    def is_reset(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False

    def is_pipelinable(self) -> bool:  # noqa: PLR6301
        """Return if the interface is pipelinable."""
        return False
