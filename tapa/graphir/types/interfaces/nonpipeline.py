"""Data structure to represent a clock interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Literal

from tapa.graphir.types.interfaces.base import BaseInterface


class NonPipelineInterface(BaseInterface):
    """Interface with ports that must not be pipelined."""

    clk_port: None = None  # type: ignore[reportIncompatibleVariableOverride]
    rst_port: None = None  # type: ignore[reportIncompatibleVariableOverride]
    role: BaseInterface.InterfaceRole
    type: Literal["non_pipeline"] = "non_pipeline"  # type: ignore[reportIncompatibleVariableOverride]

    def __init__(self, **kwargs: object) -> None:
        """Preprocessing the input ports."""
        # Check that ports excludes clk_port and rst_port.
        assert "clk_port" not in kwargs or kwargs["clk_port"] is None
        assert "rst_port" not in kwargs or kwargs["rst_port"] is None

        kwargs["clk_port"] = None
        kwargs["rst_port"] = None

        # default role is TBD
        kwargs["role"] = kwargs.get("role", BaseInterface.InterfaceRole.TBD)

        if not kwargs["ports"]:
            msg = "Interface must have at least one port."
            raise RuntimeError(msg)

        super().__init__(**kwargs)

    def __repr__(self) -> str:
        """Represent the interface as a string."""
        return f"np{self.ports}"

    def is_clk(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False

    def is_reset(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False

    def is_pipelinable(self) -> bool:  # noqa: PLR6301
        """Return if the interface is pipelinable."""
        return False


class UnknownInterface(NonPipelineInterface):
    """Auto-added non-pipeline interface.

    We should distinguish between user-defined / report inferred non-pps from the auto-
    inferred ones on uncovered ports.
    """

    type: Literal["unknown"] = "unknown"  # type: ignore[reportIncompatibleVariableOverride]

    def __init__(self, **kwargs: object) -> None:
        """Prevent pylint false positive warning of missing args."""
        # default role is TBD
        kwargs["role"] = kwargs.get("role", BaseInterface.InterfaceRole.TBD)

        super().__init__(**kwargs)


class TAPAPeekInterface(NonPipelineInterface):
    """TAPA peek interface."""

    type: Literal["tapa_peek"] = "tapa_peek"  # type: ignore[reportIncompatibleVariableOverride]

    def __init__(self, **kwargs: object) -> None:
        """Prevent pylint false positive warning of missing args."""
        # default role is TBD
        kwargs["role"] = kwargs.get("role", BaseInterface.InterfaceRole.TBD)

        super().__init__(**kwargs)
