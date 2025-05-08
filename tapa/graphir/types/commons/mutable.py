"""Base class of mutable objects that is shared among TAPA graph IR types."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


from pydantic import BaseModel, RootModel


class MutableModel(BaseModel):
    """The base model of tapa graph IR types."""


class MutableRootModel[X](RootModel[X]):
    """The base model of graph IR types."""
