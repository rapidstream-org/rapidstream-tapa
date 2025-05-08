"""Data types of interface definition."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.graphir.types.interfaces.any import AnyInterface
from tapa.graphir.types.interfaces.ap_ctrl import ApCtrlInterface
from tapa.graphir.types.interfaces.base import BaseInterface
from tapa.graphir.types.interfaces.clock import ClockInterface
from tapa.graphir.types.interfaces.falsepath import FalsePathInterface
from tapa.graphir.types.interfaces.falsepath_aux import AuxInterface
from tapa.graphir.types.interfaces.falsepath_reset import FalsePathResetInterface
from tapa.graphir.types.interfaces.feedforward import FeedForwardInterface
from tapa.graphir.types.interfaces.feedforward_reset import FeedForwardResetInterface
from tapa.graphir.types.interfaces.handshake import HandShakeInterface
from tapa.graphir.types.interfaces.nonpipeline import (
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
