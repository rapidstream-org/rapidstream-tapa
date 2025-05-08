"""Data types of project information."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.graphir.types.projects.groups import Group, Groups
from tapa.graphir.types.projects.interfaces import Interfaces
from tapa.graphir.types.projects.modules import Modules
from tapa.graphir.types.projects.project import Project

__all__ = [
    "Group",
    "Groups",
    "Interfaces",
    "Modules",
    "Project",
]
