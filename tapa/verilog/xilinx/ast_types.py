__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from pyverilog.vparser.ast import (
    Always,
    Assign,
    Initial,
    Inout,
    Input,
    Output,
    Pragma,
    Reg,
    Wire,
)

__all__ = [
    "Directive",
    "IOPort",
    "Logic",
    "Signal",
]

Directive = tuple[int, str]
IOPort = Input | Output | Inout
Signal = Reg | Wire | Pragma
Logic = Assign | Always | Initial
