"""Data types of interface definition."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from rapidstream.graphir.types.interfaces.any import AnyInterface
from rapidstream.graphir.types.interfaces.ap_ctrl import ApCtrlInterface
from rapidstream.graphir.types.interfaces.base import BaseInterface
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

__all__ = [
    "AnyInterface",
    "ApCtrlInterface",
    "AuxInterface",
    "BaseInterface",
    "ClockInterface",
    "FalsePathInterface",
    "FalsePathResetInterface",
    "FeedForwardInterface",
    "FeedForwardResetInterface",
    "HandShakeInterface",
    "NonPipelineInterface",
    "TAPAPeekInterface",
    "UnknownInterface",
]
