"""Data structure to represent a project."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from collections.abc import Callable

from tapa.graphir.assets.floorplan.instance_area import InstanceArea
from tapa.graphir.types.blackbox import BlackBox
from tapa.graphir.types.commons import MutableModel
from tapa.graphir.types.interfaces import AnyInterface
from tapa.graphir.types.modules.definitions.any import AnyModuleDefinition
from tapa.graphir.types.modules.definitions.grouped import GroupedModuleDefinition
from tapa.graphir.types.modules.instantiation import ModuleInstantiation
from tapa.graphir.types.modules.supports.iface_inst_path import IfaceInstPath
from tapa.graphir.types.projects.interfaces import Interfaces
from tapa.graphir.types.projects.modules import Modules

_logger = logging.getLogger().getChild(__name__)


class Project(MutableModel):
    """A TAPA graph IR project.

    Attributes:
        top (str): Name of the top-level module of the project.
        modules (list[AnyModuleDefinition]): The modules of the project.
    """

    # ========================================
    # ==== Information about the project. ====
    # ========================================
    part_num: str | None = None

    # modules, including all imported whiteboxes and blackboxes files
    modules: Modules
    blackboxes: list[BlackBox] = []  # noqa: RUF012

    # additional optional information about the project, including:
    # ifaces: the protocols of the ports of the modules
    ifaces: Interfaces | None = None

    module_to_rtl_pragmas: dict[str, list[str]] | None = None
    module_to_old_rtl_pragmas: dict[str, dict[str, dict[str, str | None]]] | None = None

    # =================================
    # ==== Context of the project. ====
    # =================================

    # floorplan output: the island of each module.
    island_to_pblock_range: dict[str, list[str]] | None = None
    # records the *grid* coordinates of all rectangles belonging to each island

    # routing info
    routes: list[IfaceInstPath] | None = None

    # floorplan output: the information about the islands.
    resource_to_max_local_usage: dict[str, float] | None = None
    cut_to_crossing_count: dict[str, float] | None = None

    def sort_ifaces(self) -> None:
        """Sort ifaces with the key being a concat of all ports."""
        if self.ifaces:
            for module_name, ifaces in self.ifaces.items():
                self.ifaces[module_name] = sorted(
                    set(ifaces), key=lambda iface: "__".join(sorted(iface.ports))
                )

    def merged_modules_and_ifaces_from(self, other: object) -> "Project":
        """Merge the modules and ifaces of two projects.

        The top of the new project will be the top of this project.
        """
        assert isinstance(other, Project)

        # get a new copy
        prj = self.model_copy(deep=True)

        # merge modules
        prj.modules = Modules(
            name="$root",
            module_definitions=(
                self.modules.module_definitions + other.modules.module_definitions
            ),
            top_name=self.modules.top_name,
        )

        # merge blackboxes
        prj.blackboxes = self.blackboxes + other.blackboxes

        # merge ifaces
        if not self.ifaces:
            prj.ifaces = other.ifaces
        elif not other.ifaces:
            prj.ifaces = self.ifaces
        else:
            prj.ifaces = Interfaces(self.ifaces.root | other.ifaces.root)

        return prj

    def update_top_submodules(
        self, update_inst: Callable[[ModuleInstantiation], ModuleInstantiation]
    ) -> None:
        """Update the top submodule of the project."""
        top_module = self.modules.get_top()
        updated_top_module = update_group_submodules(top_module, update_inst)
        self.add_and_override_modules((updated_top_module,))

    def update_all_group_submodules(
        self, update_inst: Callable[[ModuleInstantiation], ModuleInstantiation]
    ) -> None:
        """Update all submodules of all grouped modules."""
        updated_modules: list[GroupedModuleDefinition] = [
            update_group_submodules(module, update_inst)
            for module in self.get_all_modules()
            if isinstance(module, GroupedModuleDefinition)
        ]

        self.add_and_override_modules(tuple(updated_modules))

    def add_and_override_modules(
        self, new_modules: tuple[AnyModuleDefinition, ...]
    ) -> None:
        """Add new modules to the project and override existing modules."""
        self.modules = self.modules.modules_updated(new_modules)

    def remove_modules(self, module_names: set[str]) -> None:
        """Remove modules from the project."""
        self.modules = self.modules.modules_removed(module_names)

    def set_top_module(self, top_name: str) -> None:
        """Set the top module of the project."""
        assert self.modules.get(top_name) is not None
        self.modules = self.modules.updated(top_name=top_name)

    def get_top_module_submodules(self) -> list[AnyModuleDefinition]:
        """Get the definition of submodules of the top module."""
        return [
            self.modules.get(inst.module) for inst in self.modules.get_top().submodules
        ]

    def get_top_module(self) -> GroupedModuleDefinition:
        """Get the definition of the top module."""
        return self.modules.get_top()

    def get_top_name(self) -> str:
        """Get the name of the top module."""
        if self.modules.top_name is None:
            msg = "Top module is not set"
            raise ValueError(msg)
        return self.modules.top_name

    def get_all_modules(self) -> list[AnyModuleDefinition]:
        """Get all modules of the project."""
        return list(self.modules.module_definitions)

    def get_module(self, name: str) -> AnyModuleDefinition:
        """Get the definition of a module."""
        return self.modules.get(name)

    def has_module(self, name: str) -> bool:
        """Check if the project has a module."""
        return self.modules.has_module(name)

    def update_ifaces(self, new_ifaces: dict[str, list[AnyInterface]]) -> None:
        """Update the interfaces of the modules.

        Interfaces of an existing module are merged with ones from `new_ifaces` so that
        they can be deduplicated and validated for conflicts.
        """
        if self.ifaces:
            for module_name, new_iface_list in new_ifaces.items():
                self.ifaces.root.setdefault(module_name, []).extend(new_iface_list)
        else:
            self.ifaces = Interfaces(new_ifaces)

    def reset_ifaces(self, new_ifaces: dict[str, list[AnyInterface]]) -> None:
        """Set the interfaces of the modules."""
        if self.ifaces:
            for module_name, new_iface_list in new_ifaces.items():
                self.ifaces.root[module_name] = new_iface_list
        else:
            self.ifaces = Interfaces(new_ifaces)

    def get_all_ifaces(self) -> dict[str, list[AnyInterface]]:
        """Get all interfaces of the modules."""
        if self.ifaces:
            return self.ifaces.root
        return {}

    def get_ifaces_of_module(self, module_name: str) -> list[AnyInterface]:
        """Get the interfaces of a module."""
        if self.ifaces:
            return self.ifaces.get(module_name)
        return []

    def assign_inst_under_top_to_region(self, inst_to_region: dict[str, str]) -> None:
        """Assign each instance under top to a region."""
        top_module = self.modules.get_top()
        self.modules = self.modules.module_updated(
            top_module.assign_inst_to_region(inst_to_region)
        )

    def get_top_module_sub_inst_to_area(self) -> dict[str, dict[str, int]]:
        """Get the area of submodules of the top module.

        If an instance does not have area, it will not be included in the returned dict.
        """
        return {
            inst.name: inst.area.to_dict()
            for inst in self.modules.get_top().submodules
            if inst.area is not None
        }

    def update_top_module_subinst_area(
        self, inst_name_to_area: dict[str, InstanceArea]
    ) -> None:
        """Attach the area to the instances in the top-level module."""
        top_module = self.get_top_module()
        top_module_subs = []
        for inst in top_module.submodules:
            if inst.name not in inst_name_to_area:
                top_module_subs.append(inst)
            else:
                updated_inst = inst.updated(area=inst_name_to_area[inst.name])
                top_module_subs.append(updated_inst)

        updated_top_module = top_module.updated(submodules=top_module_subs)
        self.modules = self.modules.module_updated(updated_top_module)


def get_empty_project(part_num: str | None) -> Project:
    """Get an empty project."""
    return Project(
        part_num=part_num,
        modules=Modules(name="$root", module_definitions=(), top_name=None),
    )


def update_group_submodules(
    target_module: GroupedModuleDefinition,
    update_inst: Callable[[ModuleInstantiation], ModuleInstantiation],
) -> GroupedModuleDefinition:
    """Update submodules of a grouped module."""
    updated_insts = tuple(update_inst(inst) for inst in target_module.submodules)
    return target_module.updated(submodules=updated_insts)
