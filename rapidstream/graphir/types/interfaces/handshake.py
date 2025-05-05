"""Data structure to represent a clock interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any, Literal, Self

from rapidstream.graphir.types.interfaces.base import BaseInterface
from rapidstream.utilities.path import get_relative_path_and_func


class HandShakeInterface(BaseInterface):
    """Interface with handshake mechanisms.

    The interface include a valid port and a ready port. When both valid and ready are
    logic 1, data will be transferred from the source side to the sink side.
    """

    ready_port: str
    valid_port: str

    type: Literal["handshake"] = "handshake"

    def __init__(self, **kwargs: Any) -> None:
        """Preprocessing the input ports."""
        curr_ports = set(kwargs["ports"])

        # Make sure that ports include ready_port and valid_port.
        for special_port in ("ready_port", "valid_port"):
            if port_name := kwargs[special_port]:
                curr_ports.add(port_name)

        kwargs["ports"] = curr_ports

        for name in ("ready_port", "valid_port", "clk_port"):
            if not kwargs.get(name):
                msg = f"{name} is required for handshake interface"
                raise ValueError(msg)

        super().__init__(**kwargs)

    def __repr__(self) -> str:
        """Represent the interface as a string."""
        return (
            f"hs(clk={self.clk_port}, rst={self.rst_port},"
            f" ready={self.ready_port}, valid={self.valid_port}, "
            f"data={self.get_data_ports()}, role={self.role})"
        )

    def get_data_ports(self) -> tuple[str, ...]:
        """Extract only the data ports of the interface."""
        return tuple(
            p_name
            for p_name in self.ports
            if p_name
            not in {self.ready_port, self.valid_port, self.clk_port, self.rst_port}
        )

    def cleanup_ports(
        self, remaining_ports: set[str], **kawgs: str | None
    ) -> "HandShakeInterface | None":
        """Remove ports that are not in the remaining_ports."""
        if self.ready_port not in remaining_ports:
            return None

        if self.valid_port not in remaining_ports:
            return None

        return super().cleanup_ports(remaining_ports, **kawgs)

    def get_mapped_iface(self, port_mapping: dict[str | None, str]) -> Self:
        """Return a new interface with port names mapped according to port_mapping."""
        if set(self.__dict__.keys()) != {
            "clk_port",
            "rst_port",
            "ports",
            "role",
            "type",
            "ready_port",
            "valid_port",
            "origin_info",
        }:
            msg = f"get_mapped_iface is not implemented for {self.__class__.__name__}."
            raise NotImplementedError(msg)

        return self.updated(
            ports=tuple(port_mapping[p] for p in self.ports),
            clk_port=port_mapping.get(self.clk_port, self.clk_port),
            rst_port=port_mapping.get(self.rst_port, self.rst_port),
            ready_port=port_mapping[self.ready_port],
            valid_port=port_mapping[self.valid_port],
            role=BaseInterface.InterfaceRole.TBD,
            origin_info=get_relative_path_and_func(),
        )

    def is_clk(self) -> bool:
        """Return if the interface is a clock or reset interface."""
        return False

    def is_reset(self) -> bool:
        """Return if the interface is a clock or reset interface."""
        return False

    def is_pipelinable(self) -> bool:
        """Return if the interface is pipelinable."""
        return True
