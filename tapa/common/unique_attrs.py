"""Utility to create a set of unique attributes."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


class UniqueAttrs(dict[str, object]):
    """A set of unique attributes.

    Intended usage is to visit a syntax tree and retrieve expected attributes.

    Getting an attribute that does not exist will raise `KeyError`.

    Setting an attribute that is already set (and not None) will raise `ValueError`.
    """

    def __init__(self, **kwargs: object) -> None:
        self.update(**kwargs)

    def __getattr__(self, name: str) -> object:
        return self[name]

    def __setattr__(self, name: str, value: object) -> None:
        if (old_value := self.get(name)) is None:
            self[name] = value
            return
        msg = f"attr '{name}' already exists: {old_value}"
        raise ValueError(msg)
