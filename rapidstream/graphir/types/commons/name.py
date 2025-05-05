"""Classes for namespaces and the named objects in it."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from abc import abstractmethod
from collections.abc import Generator

from rapidstream.graphir.types.commons.model import Model
from rapidstream.utilities.name import suggest_name

_logger = logging.getLogger(__name__)


class NamedModel(Model):
    """A class of objects with a name.

    Attributes:
        name (str): The name of the object.
    """

    name: str


class NamespaceModel(NamedModel):
    """An object that has named objects under its namespace.

    Methods:
        lookup (NamedModel): Look up an object in the namespace.
    """

    def lookup(self, name: str) -> NamedModel:
        """Look up a named object in the namespace.

        Args:
            name (str): The name of the object to lookup.

        Returns:
            NamedModel: The named object with the given name.
        """
        for n in self.get_all_named():
            if n.name == name:
                return n
        msg = f"Object {name} not found in namespace {self.name}."
        raise KeyError(msg)

    def get_all_names(self) -> set[str]:
        """Return all the names in the namespace.

        Returns:
            set[str]: A set of names in the namespace.
        """
        return {n.name for n in self.get_all_named()}

    @abstractmethod
    def get_all_named(self) -> Generator[NamedModel]:
        """Yield all the named objects in the namespace.

        Yields:
            NamedModel: Nnamed objects in the namespace.
        """

    def suggest_name(self, name: str, blocklist: set[str] | None = None) -> str:
        """Return a name that is not used in the namespace.

        Args:
            name (str): The suggested name.
            blocklist (set[str] | None): A set of names that are not available.

        Returns:
            str: A name that is not used in the namespace.
        """
        return suggest_name(name, (blocklist or set()) | set(self.get_all_names()))
