"""Data structure to represent the HLS ap control interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any, Literal, Self

from tapa.graphir.types.interfaces.base import BaseInterface
from tapa.graphir.utilities.path import get_relative_path_and_func


class ApCtrlInterface(BaseInterface):
    """Interface with ap_ctrl mechanisms.

    The ap_start signal must remain High until ap_ready goes High. Once ap_ready goes
    High:
    => If ap_start remains High the design will start the next transaction.
    => If ap_start is taken Low, the design will complete the current transaction and
    halt operation.
    """

    ap_start_port: str
    ap_ready_port: str | None
    ap_done_port: str | None
    ap_idle_port: str | None
    ap_continue_port: str | None

    type: Literal["ap_ctrl"] = "ap_ctrl"  # type: ignore[reportIncompatibleVariableOverride]

    def __init__(self, **kwargs: Any) -> None:  # noqa: ANN401
        """Preprocessing the input ports."""
        curr_ports = set(kwargs["ports"])

        # Make sure that ports include ap_start, and ap_ready.
        for special_port in (
            "ap_start_port",
            "ap_ready_port",
            "ap_done_port",
            "ap_idle_port",
            "ap_continue_port",
        ):
            if port_name := kwargs[special_port]:
                curr_ports.add(port_name)

        kwargs["ports"] = curr_ports

        super().__init__(**kwargs)

    def __repr__(self) -> str:
        """Represent the interface as a string."""
        return (
            f"ap_ctrl(clk={self.clk_port}, rst={self.rst_port},"
            f" ap_ready={self.ap_ready_port}, ap_start={self.ap_start_port},"
            f" ap_done={self.ap_done_port}, ap_idle={self.ap_idle_port},"
            f" ap_continue={self.ap_continue_port}, role={self.role})"
        )

    def get_data_ports(self) -> tuple[str, ...]:
        """Extract only the data ports of the interface."""
        return tuple(
            p_name
            for p_name in self.ports
            if p_name
            not in {
                self.clk_port,
                self.rst_port,
                self.ap_start_port,
                self.ap_done_port,
                self.ap_ready_port,
                self.ap_idle_port,
                self.ap_continue_port,
            }
        )

    def cleanup_ports(
        self, remaining_ports: set[str], **kawgs: str | None
    ) -> "ApCtrlInterface | None":
        """Remove ports that are not in the remaining_ports."""
        return super().cleanup_ports(remaining_ports, **kawgs)

    def get_mapped_iface(self, port_mapping: dict[str | None, str]) -> Self:
        """Return a new interface with port names mapped according to port_mapping."""
        if set(self.__dict__.keys()) != {
            "clk_port",
            "rst_port",
            "ports",
            "role",
            "type",
            "ap_start_port",
            "ap_ready_port",
            "ap_done_port",
            "ap_idle_port",
            "ap_continue_port",
            "origin_info",
        }:
            msg = f"get_mapped_iface is not implemented for {self.__class__.__name__}."
            raise NotImplementedError(msg)

        return self.updated(
            ports=tuple(port_mapping[p] for p in self.ports),
            clk_port=port_mapping.get(self.clk_port, self.clk_port),
            rst_port=port_mapping.get(self.rst_port, self.rst_port),
            ap_start_port=port_mapping[self.ap_start_port],
            ap_ready_port=(
                port_mapping[self.ap_ready_port] if self.ap_ready_port else None
            ),
            ap_done_port=port_mapping[self.ap_done_port] if self.ap_done_port else None,
            ap_idle_port=port_mapping[self.ap_idle_port] if self.ap_idle_port else None,
            ap_continue_port=(
                port_mapping[self.ap_continue_port] if self.ap_continue_port else None
            ),
            role=BaseInterface.InterfaceRole.TBD,
            origin_info=get_relative_path_and_func(),
        )

    def is_clk(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False

    def is_reset(self) -> bool:  # noqa: PLR6301
        """Return if the interface is a clock or reset interface."""
        return False

    def is_pipelinable(self) -> bool:  # noqa: PLR6301
        """Return if the interface is pipelinable."""
        return True
