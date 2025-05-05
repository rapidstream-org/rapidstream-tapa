"""Utilities to evaluate verilog constants."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import re

import pyslang as sl

VERILOG_CONST_PATTERN = r"\d+'s?[bodhBODH][0-9a-fA-F_]+"

_logger = logging.getLogger(__name__)


def is_verilog_const(ident: str) -> bool:
    """Return True if an identifier is a verilog constant value.

    Args:
        ident (str): The Verilog expression of the identifier.

    Returns:
        bool: True if the identifier is a verilog constant value.

    Examples:
        >>> is_verilog_const('''8'b1011''')
        True
        >>> is_verilog_const('''test''')
        False
    """
    try:
        eval_verilog_const(ident)
        return True
    except ValueError:
        return False


def get_int_from_verilog_const_val(value_str: str) -> int:
    """Convert a verilog constant value to int.

    Examples:
    >>> get_int_from_verilog_const_val("8'b1011")
    11

    >>> get_int_from_verilog_const_val("8'hA")
    10

    >>> get_int_from_verilog_const_val("1+1")
    2
    """
    session = sl.ScriptSession()
    value = session.eval(value_str)

    if value and isinstance(value, sl.ConstantValue):
        return int(value.convertToInt().value)

    msg = f"The argument is not an int: {value_str}"
    raise ValueError(msg)


def eval_verilog_const(const: str) -> str:
    r"""Convert a verilog constant value to int.

    Args:
        const (str): The Verilog expression of the constant.

    Raises:
        ValueError: The given argument is not a constant.

    Returns:
        str: The corresponding string representation of the constant.

    Examples:
        It evaluates constant integer values:

        >>> eval_verilog_const('''8'b1011''')
        "8'b1011"
        >>> eval_verilog_const('''8'hA''')
        "8'hA"
        >>> eval_verilog_const('''8'shA''')
        "8'shA"

        Raises an error for non-constant values:

        >>> eval_verilog_const('''test''')  # doctest: +IGNORE_EXCEPTION_DETAIL
        Traceback (most recent call last):
        ValueError: The argument is not a constant.

        And it evaluates constant string values:

        >>> eval_verilog_const('''"test"''')
        '"test"'
    """
    val = eval_verilog_const_no_exception(const)
    if val is not None:
        return val

    msg = f"The argument is not a constant: {const}"
    raise ValueError(msg)


def eval_verilog_const_no_exception(value_str: str) -> str | None:
    """Convert a verilog constant value to int.

    Examples:
    >>> eval_verilog_const_no_exception("{64'd01, 64'd97, 64'd01}")
    "{64'd01, 64'd97, 64'd01}"

    >>> eval_verilog_const_no_exception("128'hffff_ffff_ffff_ffff_ffff_ffff_ffff_ffff")
    "128'hffff_ffff_ffff_ffff_ffff_ffff_ffff_ffff"

    >>> eval_verilog_const_no_exception("12")
    '12'

    >>> eval_verilog_const_no_exception("a")


    >>> eval_verilog_const_no_exception("{64'd01}")
    "64'h1"

    >>> eval_verilog_const_no_exception("128'b111")
    "128'b111"

    >>> eval_verilog_const_no_exception("(1+2) * 3")
    '9'
    """
    session = sl.ScriptSession()
    value = session.eval(value_str)

    if value and isinstance(value, sl.ConstantValue):
        # Do nothing for large value or string
        # Vivado accepts large values in the format of [width]'[base][value] but not
        # as pure int. There is no need to convert it to int.
        if (
            value.bitstreamWidth() > 64
            or '"' in value_str
            or re.fullmatch(VERILOG_CONST_PATTERN, value_str)
        ):
            return value_str

        int_val = int(value.convertToInt().value)

        # do not try to simplify large value
        if int_val < -(2**31) or int_val >= 2**31:
            _logger.warning(
                "Failed to convert the overly large/small constant: %s", value_str
            )
            return value_str

        # simplify small value
        return str(value)

    # value is not constant
    return None
