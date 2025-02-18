__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import enum
from typing import Optional, TextIO

__all__ = (
    "HierarchicalUtilization",
    "parse_hierarchical_utilization_report",
)


class HierarchicalUtilization:
    """Semantic-agnostic hierarchical utilization."""

    device: str
    parent: Optional["HierarchicalUtilization"]
    children: list["HierarchicalUtilization"]
    instance: str
    schema: dict[str, int]
    items: tuple[str, ...]

    def __init__(
        self,
        device: str,
        instance: str,
        schema: dict[str, int],
        items: tuple[str, ...],
        parent: Optional["HierarchicalUtilization"] = None,
    ) -> None:
        if len(schema) != len(items):
            msg = "mismatching schema and items"
            raise TypeError(msg)
        self.device = device
        self.parent = parent
        self.children = []
        if parent is not None:
            parent.children.append(self)
        self.instance = instance
        self.schema = schema
        self.items = items

    def __getitem__(self, key: str) -> str:
        return self.items[self.schema[key]]

    def __str__(self) -> str:
        parent = None
        if self.parent is not None:
            parent = self.parent.instance
        return "\n".join(
            (
                "",
                f"instance: {self.instance}",
                f"parent: {parent}",
                *(f"{key}: {value}" for key, value in zip(self.schema, self.items)),
            )
        )


def parse_hierarchical_utilization_report(rpt_file: TextIO) -> HierarchicalUtilization:
    """Parse hierarchical utilization report.

    This is a compromise where Vivado won't export structured report from scripts.
    """

    class ParseState(enum.Enum):
        PROLOG = 0
        HEADER = 1
        BODY = 2
        EPILOG = 3

    parse_state = ParseState.PROLOG
    stack: list[HierarchicalUtilization] = []
    device = ""

    for unstripped_line in rpt_file:
        line = unstripped_line.strip()
        items = line.split()
        if len(items) == 4 and items[:3] == ["|", "Device", ":"]:  # noqa: PLR2004
            device = items[3]
            continue
        if set(line) == {"+", "-"}:
            if parse_state == ParseState.PROLOG:
                parse_state = ParseState.HEADER
            elif parse_state == ParseState.HEADER:
                parse_state = ParseState.BODY
            elif parse_state == ParseState.BODY:
                parse_state = ParseState.EPILOG
            else:
                msg = "unexpected table separator line"
                raise ValueError(msg)
            continue

        if parse_state == ParseState.HEADER:
            instance, items = get_items(line)
            assert instance.lstrip() == "Instance"
            schema = {x.lstrip(): i for i, x in enumerate(items)}

        elif parse_state == ParseState.BODY:
            instance, items = get_items(line)
            while (len(instance) - len(instance.lstrip(" "))) // 2 < len(stack):
                stack.pop()
            instance = instance.lstrip()
            parent = stack[-1] if stack else None
            stack.append(
                HierarchicalUtilization(device, instance, schema, items, parent)
            )

    return stack[0]


def get_items(line: str) -> tuple[str, tuple[str, ...]]:
    """Split a table line into items.

    Args:
        line (str): A line in a report table.

    Returns:
        tuple[str, ...]: Fields split by '|' with blank on the right stripped.
    """
    items = line.strip().strip("|").split("|")
    return items[0].rstrip(), tuple(x.strip() for x in items[1:])
