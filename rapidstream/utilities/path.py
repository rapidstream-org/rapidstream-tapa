"""Get the paths of RapidStream."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import inspect
import pathlib


def get_relative_path_and_func() -> str:
    """Get the relative path and line number of the caller."""
    # Get the caller's frame
    frame = inspect.currentframe()
    if frame is None:
        msg = "Cannot get the caller's frame."
        raise RuntimeError(msg)

    caller_frame = frame.f_back
    if caller_frame is None:
        msg = "Cannot get the caller's frame."
        raise RuntimeError(msg)

    # Get the caller's filename
    abs_path = caller_frame.f_code.co_filename
    file_name = pathlib.Path(abs_path).name

    # Get the current function name
    function_name = caller_frame.f_code.co_name

    return f"{file_name}:{function_name}"
