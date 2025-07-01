__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from pyverilog.vparser.ast import (
    Inout,
    Input,
    Output,
)

__all__ = [
    "Directive",
    "IOPort",
]

Directive = tuple[int, str]
IOPort = Input | Output | Inout
