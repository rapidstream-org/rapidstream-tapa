"""Base class of immutable objects that is shared among TAPA graph IR types."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""
from collections.abc import Iterable, Sized
from typing import Self, TypeVar

from pydantic import BaseModel, ConfigDict
from pydantic import RootModel as PydanticRootModel

X = TypeVar("X")


class ModelMixin:
    """The base model for immutable and hashable tapa graph IR types."""

    def updated(self, **update: object) -> Self:
        """Return a new object attached to the original namespace with fields updated.

        For example, you can `module.updated(name='new_name')` to update the
        name of an immutable module, which returns the updated module.

        Note that if the model is a named model, you shall not use the original
        object after the update as its namespace will be detached.

        Args:
            **update (object): The fields to update.

        Returns:
            _X: The updated immutable object.
        """
        return self._model_copy(deep=False, **update)

    def _model_copy(self, deep: bool, **update: object) -> Self:
        """Return a new immutable object with fields updated.  Either deep or shallow.

        Args:
            deep (bool): Whether to perform a deep copy.
            **update (object): The fields to update.

        Returns:
            _X: The updated immutable object.
        """
        for key in update:
            if key not in self.model_fields:
                msg = f"Unknown field {key} to update."
                raise ValueError(msg)

        update = self.sanitze_fields(**update)
        return self.model_copy(update=update, deep=deep)

    @staticmethod
    def get_name_of_object(inst: object | dict[str, object]) -> str:
        """Return the name of a named object or its dictionary representation."""
        if isinstance(inst, dict):
            return str(inst["name"])
        if hasattr(inst, "name"):
            return str(inst.name)
        if isinstance(inst, str):
            return inst
        msg = f"Cannot get name of {inst}."
        raise ValueError(msg)

    @staticmethod
    def sanitze_fields(**kwargs: object) -> dict[str, object]:
        """To be overridden by subclasses to sanitize the fields.

        Args:
            **kwargs (object): The fields to sanitize.

        Returns:
            dict[str, object]: The sanitized fields.
        """
        return kwargs

    @classmethod
    def sort_tuple_field(cls, kwargs: dict[str, object], field: str) -> None:
        """Sort the tuple `field` in `kwargs` by name and return the argument."""
        if field in kwargs and (data := kwargs[field]):
            assert isinstance(data, Iterable)
            assert isinstance(data, Sized)

            # The items in data should have unique names.
            if len({cls.get_name_of_object(inst) for inst in data}) != len(data):
                msg = f"Duplicate names in {field}: {data}."
                raise ValueError(msg)

            kwargs[field] = tuple(sorted(data, key=cls.get_name_of_object))

    model_config = ConfigDict(frozen=True)


class Model(BaseModel, ModelMixin):
    """The base model of immutable and hashable tapa graph IR types."""

    # This must be implemented by this class instead of in the mixin class.
    # Otherwise, super() solves the MRO incorrectly.
    def __init__(self, **kwargs: object) -> None:
        """Initialize the model with the fields sorted."""
        super().__init__(**(self.sanitze_fields(**kwargs)))


class RootModel(PydanticRootModel[X], ModelMixin):
    """The base model of immutable and hashable tapa graph IR types."""
