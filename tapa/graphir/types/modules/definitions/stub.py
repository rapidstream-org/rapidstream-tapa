"""Data structure to represent a stub module definition."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from collections.abc import Generator
from typing import Literal

from tapa.graphir.types.commons.name import NamedModel
from tapa.graphir.types.modules.definitions.base import BaseModuleDefinition


class StubModuleDefinition(BaseModuleDefinition):
    """A definition of a stub module with only interface information.

    Examples:
        >>> import json
        >>> print(
        ...     json.dumps(
        ...         StubModuleDefinition(
        ...             name="empty_module",
        ...             hierarchical_name=None,
        ...             parameters=[],
        ...             ports=[],
        ...         ).model_dump()
        ...     )
        ... )
        ... # doctest: +NORMALIZE_WHITESPACE
        {"name": "empty_module", "hierarchical_name": null,
         "module_type": "stub_module", "parameters": [], "ports": [], "metadata": null}
    """

    module_type: Literal["stub_module"] = "stub_module"  # type: ignore[reportIncompatibleVariableOverride]

    def get_all_named(self) -> Generator[NamedModel]:
        """Yields all the named objects in the namespace."""
        yield from self.ports
        yield from self.parameters

    def get_submodules_module_names(self) -> tuple[str, ...]:  # noqa: PLR6301
        """No submodules in a stub module."""
        return ()

    def is_leaf_module(self) -> bool:  # noqa: PLR6301
        """Stub module is always a leaf module."""
        return True

    def is_internal_module(self) -> bool:  # noqa: PLR6301
        """Stub module is not an internal module."""
        return False
