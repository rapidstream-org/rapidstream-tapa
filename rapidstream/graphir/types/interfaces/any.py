"""A union type to represent any type of an interface definition."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Annotated

from pydantic import Field

from rapidstream.graphir.types.interfaces.ap_ctrl import ApCtrlInterface
from rapidstream.graphir.types.interfaces.clock import ClockInterface
from rapidstream.graphir.types.interfaces.falsepath import FalsePathInterface
from rapidstream.graphir.types.interfaces.falsepath_aux import AuxInterface
from rapidstream.graphir.types.interfaces.falsepath_reset import FalsePathResetInterface
from rapidstream.graphir.types.interfaces.feedforward import FeedForwardInterface
from rapidstream.graphir.types.interfaces.feedforward_reset import (
    FeedForwardResetInterface,
)
from rapidstream.graphir.types.interfaces.handshake import HandShakeInterface
from rapidstream.graphir.types.interfaces.nonpipeline import (
    NonPipelineInterface,
    TAPAPeekInterface,
    UnknownInterface,
)

AnyInterface = Annotated[
    FeedForwardInterface
    | HandShakeInterface
    | NonPipelineInterface
    | FalsePathInterface
    | ApCtrlInterface
    | ClockInterface
    | FalsePathResetInterface
    | FeedForwardResetInterface
    | UnknownInterface
    | TAPAPeekInterface
    | AuxInterface,
    Field(discriminator="type"),
]
