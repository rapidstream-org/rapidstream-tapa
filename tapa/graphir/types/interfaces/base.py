"""Data structure to represent a module interface."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any, Self

from tapa.graphir.types.commons import Model, StringEnum
from tapa.graphir.utilities.path import get_relative_path_and_func


class BaseInterface(Model):
    """A definition of an interface.

    An interface is a group of related ports.

    Attributes:
        ports (list[ModulePort]): An ordered list of port definitions of
            this interface.  In module instantiations, connections to the ports
            shall be provided.
        type (Literal): e.g., handshake
        role (InterfaceRole): For handshake interfaces, source interface
        initiates a data transaction and the sink interface accepts a
        data transaction. For feedforward interfaces, the source interface
        produces data and the sink interfaces takes in data.
    """

    class InterfaceRole(StringEnum):
        """Differentiate sink or source interfaces."""

        SINK = "sink"
        SOURCE = "source"
        TBD = "to_be_determined"

    clk_port: str | None
    rst_port: str | None
    ports: tuple[str, ...]
    role: InterfaceRole
    origin_info: str

    def __init__(self, **kwargs: Any) -> None:  # noqa: ANN401
        """Preprocessing the input ports."""
        # make sure that ports include clk_port and rst_port.
        curr_ports = list(kwargs["ports"])
        curr_ports.extend(
            port_name
            for special_port in ("clk_port", "rst_port")
            if special_port in kwargs and (port_name := kwargs[special_port])
        )

        # sort the ports
        kwargs["ports"] = tuple(sorted(set(curr_ports)))

        # default role is TBD
        kwargs["role"] = kwargs.get("role", BaseInterface.InterfaceRole.TBD)

        if not kwargs["ports"]:
            msg = "Interface must have at least one port."
            raise RuntimeError(msg)

        super().__init__(**kwargs)

    def __eq__(self, other: object) -> bool:
        """Check if two interfaces are equal.

        Different origin info does not affect the equality.
        """
        if type(self) is not type(other):
            return False
        assert isinstance(other, BaseInterface)
        return (
            self.ports == other.ports
            and self.clk_port == other.clk_port
            and self.rst_port == other.rst_port
            and self.role == other.role
        )

    def __hash__(self) -> int:
        """Return the hash value of the interface.

        Different origin info does not affect the equality.
        """
        return hash((self.ports, self.clk_port, self.rst_port, self.role))

    def get_key(self) -> int:
        """Return the hash value of the interface."""
        return hash(self)  # pylint: disable=unnecessary-dunder-call

    @classmethod
    def sanitze_fields(cls, **kwargs: object) -> dict[str, object]:
        """Sort the ports by name and return the arguments."""
        cls.sort_tuple_field(kwargs, "ports")
        return Model.sanitze_fields(**kwargs)

    def get_data_ports(self) -> tuple[str, ...]:
        """Return all ports except clk, rst, valid, ready.

        For base iface, this is equial to get_ports_except_clk_rst(). For other types
        such as HandShake iface and FalsePath iface, need to reload this method.
        """
        return self.get_ports_except_clk_rst()

    def get_ports_except_clk_rst(self) -> tuple[str, ...]:
        """Return all ports except clk and rst ports."""
        return tuple(
            p_name
            for p_name in self.ports
            if p_name not in {self.clk_port, self.rst_port}
        )

    def is_sink(self) -> bool:
        """Return if the interface is a sink."""
        return self.role == BaseInterface.InterfaceRole.SINK

    def is_source(self) -> bool:
        """Return if the interface is a source."""
        return self.role == BaseInterface.InterfaceRole.SOURCE

    def is_tbd(self) -> bool:
        """Return if the interface role is to be determined."""
        return self.role == BaseInterface.InterfaceRole.TBD

    def cleanup_ports(
        self, remaining_ports: set[str], **kawgs: str | None
    ) -> Self | None:
        """Remove ports that are not in the remaining_ports."""
        ports = tuple(set(self.ports) & remaining_ports)
        clk_port = None if self.clk_port not in self.ports else self.clk_port
        rst_port = None if self.rst_port not in self.ports else self.rst_port

        # If there is no port left, return None.
        if not (set(ports) - {clk_port, rst_port}):
            return None

        return self.updated(ports=ports, clk_port=clk_port, rst_port=rst_port, **kawgs)

    def get_mapped_iface(self, port_mapping: dict[str | None, str]) -> Self:
        """Return a new interface with port names mapped according to port_mapping."""
        if set(self.__dict__.keys()) != {
            "clk_port",
            "rst_port",
            "ports",
            "role",
            "type",
            "origin_info",
        }:
            msg = f"get_mapped_iface is not implemented for {self.__class__.__name__}."
            raise NotImplementedError(msg)

        return self.updated(
            ports=tuple(port_mapping[p] for p in self.ports),
            clk_port=port_mapping.get(self.clk_port, self.clk_port),
            rst_port=port_mapping.get(self.rst_port, self.rst_port),
            role=BaseInterface.InterfaceRole.TBD,
            origin_info=get_relative_path_and_func(),
        )

    def is_clk(self) -> bool:
        """Return if the interface is a clock or reset interface."""
        msg = f"is_clk_or_reset is not implemented for {self.__class__.__name__}."
        raise NotImplementedError(msg)

    def is_reset(self) -> bool:
        """Return if the interface is a clock or reset interface."""
        msg = f"is_clk_or_reset is not implemented for {self.__class__.__name__}."
        raise NotImplementedError(msg)

    def is_clk_or_reset(self) -> bool:
        """Return if the interface is a clock or reset interface."""
        return self.is_clk() or self.is_reset()

    def is_pipelinable(self) -> bool:
        """Return if the interface is pipelinable."""
        msg = f"is_pipelinable is not implemented for {self.__class__.__name__}."
        raise NotImplementedError(msg)
