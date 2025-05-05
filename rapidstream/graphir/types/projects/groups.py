"""Data structure to describe which instances are to be grouped together."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any

from rapidstream.graphir.types.commons import (
    ListLikeRootMixin,
    MutableModel,
    MutableRootModel,
)


class Group(MutableModel):
    """A group of instances.

    Attributes:
        name (str): The name of the group.
        instances (list[str]): The instances in the group.
    """

    name: str
    instances: list[str]


class Groups(  # type: ignore [misc]
    # Mix-in the methods from root
    ListLikeRootMixin[Group],
    MutableRootModel[list[Group]],
):
    """A scheme to group multiple instances into a grouped instance.

    Attributes:
        root (list[Group]): The groups of instances.

    Example:
        >>> g = Group(name="g1", instances=["i1"])
        >>> print(Groups([g]).model_dump_json())
        [{"name":"g1","instances":["i1"]}]
    """

    root: list[Group]

    # Explicitly allow supplying one argument to make mypy happy.
    def __init__(self, value: list[Group] | None = None, **kwargs: Any) -> None:
        """Initialize the IR node."""
        super().__init__(value, **kwargs)
