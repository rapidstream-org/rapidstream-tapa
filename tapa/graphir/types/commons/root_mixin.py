"""Mixin class that has a root field that is list-like or dict-like."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from collections.abc import ItemsView, Iterator, KeysView, ValuesView
from typing import Any


class CommonRootMixin:
    """A mixin class to pass common functions to root."""

    root: list[Any] | tuple[Any, ...] | dict[Any, Any]

    def __bool__(self) -> bool:
        """Passthrough bool conversion to the root element."""
        return bool(self.root)

    def __len__(self) -> int:
        """Passthrough length to the root element."""
        return len(self.root)

    def __repr__(self) -> str:
        """Passthrough repr conversion to the root element."""
        return repr(self.root)

    def __str__(self) -> str:
        """Passthrough string conversion to the root element."""
        return str(self.root)


class DictLikeRootMixin[Key, Value](CommonRootMixin):
    """A mixin class to mock a dict from the root attribute."""

    root: dict[Key, Value]

    def __init__(
        self,
        root: dict[Key, Value] | None = None,
        **kwargs: Any,  # noqa: ANN401
    ) -> None:
        """Allow one value to be specified for the root element."""
        if root is not None:
            super().__init__(root=root)  # type: ignore [call-arg]
        else:
            super().__init__(**kwargs)

    def __contains__(self, key: Key) -> bool:
        """Mock a membership test from the root element."""
        return key in self.root

    def __getitem__(self, key: Key) -> Value:
        """Mock an indexable object from the root element."""
        return self.root[key]

    def __setitem__(self, key: Key, value: Value) -> None:
        """Mock an indexable object from the root element."""
        self.root[key] = value

    def __iter__(self) -> Iterator[Key]:
        """Mock an iterable object from the root element."""
        return self.root.__iter__()

    def items(self) -> ItemsView[Key, Value]:
        """Mock a dict from the root element."""
        return self.root.items()

    def keys(self) -> KeysView[Key]:
        """Mock a dict from the root element."""
        return self.root.keys()

    def values(self) -> ValuesView[Value]:
        """Mock a dict from the root element."""
        return self.root.values()


class TupleLikeRootMixin[Value](CommonRootMixin):
    """A mixin class to mock a tuple from the root attribute."""

    root: list[Value] | tuple[Value, ...]

    def __init__(
        self,
        root: list[Value] | tuple[Value, ...] | None = None,
        **kwargs: Any,  # noqa: ANN401
    ) -> None:
        """Allow one value to be specified for the root element."""
        if root is not None:
            super().__init__(root=root)  # type: ignore [call-arg]
        else:
            super().__init__(**kwargs)

    def __contains__(self, value: Value) -> bool:
        """Mock a membership test from the root element."""
        return value in self.root

    def __getitem__(self, idx: int) -> Value:
        """Mock an indexable object from the root element."""
        return self.root[idx]

    def __iter__(self) -> Iterator[Value]:
        """Mock an iterable object from the root element."""
        return self.root.__iter__()


class ListLikeRootMixin[Value](TupleLikeRootMixin[Value]):
    """A mixin class to mock a list from the root attribute."""

    root: list[Value]

    def __setitem__(self, key: int, value: Value) -> None:
        """Mock an indexable object from the root element."""
        self.root[key] = value
