"""Parser/vendor-agnostic RTL pragma type."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import NamedTuple


class Pragma(NamedTuple):
    name: str
    value: str | None = None

    def __str__(self) -> str:
        if self.value is None:
            return f"(* {self.name} *)"
        return f'(* {self.name} = "{self.value}" *)'
