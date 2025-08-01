"""Data structure to describe the interfaces of the modules in a project."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from typing import Any

from tapa.graphir.types.commons import DictLikeRootMixin, MutableRootModel
from tapa.graphir.types.interfaces import AnyInterface


class Interfaces(  # type: ignore [misc]
    # Mix-in the methods from root
    DictLikeRootMixin[str, list[AnyInterface]],
    MutableRootModel[dict[str, list[AnyInterface]]],
):
    """A dictionary to describe the interfaces of the modules.

    Attributes:
        root (dict[str, list[AnyInterface]]): The interfaces of
            the modules, whose name serves as the key of the dictionary.

    Example:
        >>> print(Interfaces().model_dump_json())
        {}
    """

    root: dict[str, list[AnyInterface]] = {}  # noqa: RUF012

    # Explicitly allow supplying one argument to make mypy happy.
    def __init__(
        self,
        value: dict[str, list[AnyInterface]] | None = None,
        **kwargs: Any,  # noqa: ANN401
    ) -> None:
        """Initialize the IR node."""
        super().__init__(value, **kwargs)

    def get(self, module_name: str) -> list[AnyInterface]:
        """Return the interfaces of the given module.

        Args:
            module_name (str): The name of the module.

        Returns:
            list[AnyInterface]: The interfaces of the module.
        """
        return self.get(module_name, [])  # type: ignore[reportCallIssue]

    def remove(self, module_names: set[str]) -> None:
        """Remove the interfaces of the given modules."""
        self.root = {
            module_name: ifaces
            for module_name, ifaces in self.root.items()
            if module_name not in module_names
        }
