__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import NamedTuple


class AXI:
    def __init__(self, name: str, data_width: int, addr_width: int) -> None:
        self.name = name
        self.data_width = data_width
        self.addr_width = addr_width


class Port(NamedTuple):
    """Port parsed from kernel.xml."""

    name: str
    mode: str  # 'read_only' | 'write_only'
    data_width: int

    @property
    def is_istream(self) -> bool:
        """Returns whether this port is a tapa::istream."""
        return self.mode == "read_only"

    @property
    def is_ostream(self) -> bool:
        """Returns whether this port is a tapa::ostream."""
        return self.mode == "write_only"


class Arg(NamedTuple):
    """Arg parsed from kernel.xml."""

    name: str
    address_qualifier: int  # 0: scalar, 1: mmap, 4: stream
    id: int
    port: Port
    is_streams: bool = False

    @property
    def is_scalar(self) -> bool:
        """Returns whether this arg is a scalar."""
        return self.address_qualifier == 0

    @property
    def is_mmap(self) -> bool:
        """Returns whether this arg is a mmap."""
        return self.address_qualifier == 1

    @property
    def is_stream(self) -> bool:
        """Returns whether this arg is a stream."""
        # Adding a constant for this magic value is tautology.
        # ruff: noqa: PLR2004
        return self.address_qualifier == 4


MAX_AXI_BRAM_ADDR_WIDTH = 32
