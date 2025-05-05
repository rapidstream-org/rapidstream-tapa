"""Base class of mutable objects that is shared among RapidStream types."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


from pydantic import BaseModel, RootModel

import rapidstream.graphir.types.commons.nuitka_workaround  # pylint: disable=unused-import  # noqa


class MutableModel(BaseModel):
    """The base model of RapidStream graph IR types."""


class MutableRootModel[X](RootModel[X]):
    """The base model of RapidStream graph IR types."""
