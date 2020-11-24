from typing import Tuple, Union

from tapa.verilog import ast

__all__ = [
    'Directive',
    'IOPort',
    'Signal',
    'Logic',
]

Directive = Tuple[int, str]
IOPort = Union[ast.Input, ast.Output, ast.Inout]
Signal = Union[ast.Reg, ast.Wire, ast.Pragma]
Logic = Union[ast.Assign, ast.Always, ast.Initial]
