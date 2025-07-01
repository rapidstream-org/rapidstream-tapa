"""Parser/vendor-agnostic RTL logic (`Assign`/`Always`) type."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import NamedTuple


class Assign(NamedTuple):
    lhs: str
    rhs: str

    def __str__(self) -> str:
        return f"assign {self.lhs} = {self.rhs};"
