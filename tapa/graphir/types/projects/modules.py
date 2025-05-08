"""Data structure to store the modules in the project."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from collections.abc import Generator, Iterable
from typing import get_args

from tapa.graphir.types.commons import NamedModel, NamespaceModel
from tapa.graphir.types.modules import AnyModuleDefinition
from tapa.graphir.types.modules.definitions.grouped import GroupedModuleDefinition
from tapa.graphir.types.modules.instantiation import ModuleInstantiation

_logger = logging.getLogger(__name__)

DEBUG_MAKR_USED = "Module '%s' is marked as used from module '%s'."
WARN_BLACKBOX_USED = (
    "Blackbox module '%s' is marked used.  However, we cannot analyze if it uses other "
    "modules.  It is assumed to be using no other modules in the call graph analysis.  "
    "The exported design may lack some modules if this assumption is false."
)
RAISE_TOP_NOT_SET = "The top module must be set before analyzing used modules."


class Modules(NamespaceModel):
    """A collection of modules in the project.

    Attributes:
        module_definitions (tuple[AnyModuleDefinition]): The modules in the project.
        top_name (str | None): The name of the top-level module.

    Example:
        >>> import json
        >>> from tapa.graphir.types.modules import StubModuleDefinition
        >>> stub = StubModuleDefinition(
        ...     name="stub", hierarchical_name=None, parameters=[], ports=[]
        ... )
        >>> modules = Modules(name="$root", module_definitions=(stub,), top_name=None)
        >>> print(json.dumps(modules.model_dump()))  # doctest: +NORMALIZE_WHITESPACE
        {"name": "$root", "module_definitions": [{"name": "stub",
        "hierarchical_name": null, "module_type": "stub_module", "parameters": [],
        "ports": [], "metadata": null}],
        "top_name": null}

        >>> assert modules.lookup("stub") == stub

        Note that modules.lookup('stub') may not be the same object as stub as
        copying may be performed when constructing the Modules object.  This is
        fine as all NamedModel objects are immutable.
    """

    # module definitions
    module_definitions: tuple[AnyModuleDefinition, ...]

    # top level module name
    top_name: str | None

    def get_all_named(self) -> Generator[NamedModel]:
        """Yields all the named objects in the namespace."""
        yield from self.module_definitions

    @classmethod
    def sanitze_fields(cls, **kwargs: object) -> dict[str, object]:
        """Sort the tuple arguments by name and return the arguments."""
        cls.sort_tuple_field(kwargs, "module_definitions")
        return super().sanitze_fields(**kwargs)

    def get(self, name: str) -> AnyModuleDefinition:
        """Get the module with the specified name.

        Args:
            name (str): The name of the module.

        Returns:
            AnyModuleDefinition: The module with the specified name.

        Raises:
            KeyError: The module with the specified name does not exist.
        """
        module = self.lookup(name)

        # Make sure that the lookup result is a module.
        if not isinstance(module, get_args(get_args(AnyModuleDefinition)[0])):
            msg = f"Module {name} does not exist."
            raise KeyError(msg)

        return module  # type: ignore [return-value, no-any-return]

    def get_submodules_recursive(
        self, module: AnyModuleDefinition
    ) -> tuple[ModuleInstantiation, ...]:
        """Get all submodules of a module recursively.

        Args:
            module (AnyModuleDefinition): The module to get the submodules of.

        Returns:
            tuple[ModuleInstantiation]: The submodules of the specified module.

        Example:
            >>> from tapa.graphir.types import (
            ...     StubModuleDefinition,
            ...     GroupedModuleDefinition,
            ...     ModuleInstantiation,
            ... )
            >>> stub = StubModuleDefinition(
            ...     name="stub", hierarchical_name=None, parameters=(), ports=()
            ... )
            >>> stub_inst = ModuleInstantiation(
            ...     name="stub_0",
            ...     hierarchical_name=None,
            ...     module="stub",
            ...     parameters=(),
            ...     connections=(),
            ...     floorplan_region=None,
            ...     area=None,
            ... )
            >>> g1 = GroupedModuleDefinition(
            ...     name="g1",
            ...     hierarchical_name=None,
            ...     parameters=(),
            ...     ports=(),
            ...     submodules=(stub_inst,),
            ...     wires=(),
            ... )
            >>> g1_inst = ModuleInstantiation(
            ...     name="g1_0",
            ...     hierarchical_name=None,
            ...     module="g1",
            ...     parameters=(),
            ...     connections=(),
            ...     floorplan_region=None,
            ...     area=None,
            ... )
            >>> g2 = GroupedModuleDefinition(
            ...     name="g2",
            ...     hierarchical_name=None,
            ...     parameters=(),
            ...     ports=(),
            ...     submodules=(g1_inst,),
            ...     wires=(),
            ... )
            >>> modules = Modules(
            ...     name="$root", module_definitions=(stub, g1, g2), top_name=None
            ... )
            >>> modules.get_submodules_recursive(g2)
            ... # doctest: +NORMALIZE_WHITESPACE
            (ModuleInstantiation(name='g1_0',
                hierarchical_name=HierarchicalName(root=None),
                module='g1', connections=(), parameters=(), floorplan_region=None,
                area=None, pragmas=()),
             ModuleInstantiation(name='stub_0',
                hierarchical_name=HierarchicalName(root=None),
                module='stub', connections=(), parameters=(), floorplan_region=None,
                area=None, pragmas=()))
        """
        all_submodules = module.get_submodules()

        for curr_submodule in module.get_submodules():
            module_def = self.get(curr_submodule.module)
            all_submodules += self.get_submodules_recursive(module_def)

        return all_submodules

    def get_all_used_modules(self) -> set[str]:
        """Recursively collect all used modules from the top module."""
        # Collect all used modules
        used_modules: set[str] = set()

        def recurse(module: str) -> None:
            """Recursively collect all used modules."""
            if module in used_modules:
                return
            used_modules.add(module)

            # Ignore blackbox modules
            try:
                # Recursively collect all used modules from the submodule
                for submodule in self.get(module).get_submodules_module_names():
                    recurse(submodule)
                    _logger.debug(DEBUG_MAKR_USED, submodule, module)
            except KeyError:
                _logger.warning(WARN_BLACKBOX_USED, module)

        if not self.top_name:
            raise ValueError(RAISE_TOP_NOT_SET)

        # Recursively collect all used modules from the top module
        recurse(self.top_name)

        return used_modules

    def get_top(self) -> GroupedModuleDefinition:
        """Get the top-level module.

        Returns:
            GroupedModuleDefinition: The top-level module.
        """
        if not self.top_name:
            msg = "The top-level module is not specified."
            raise ValueError(msg)

        top_module = self.get(self.top_name)
        if not isinstance(top_module, GroupedModuleDefinition):
            msg = "The top module is not a grouped module"
            raise ValueError(msg)

        return top_module

    def module_updated(self, module: AnyModuleDefinition) -> "Modules":
        """Update or add the specified module.

        Note that you shall not use the original modules object after the
        update as its namespace will be detached.

        Args:
            module (AnyModuleDefinition): The module to add or update.

        Returns:
            Modules: The updated immutable modules object.
        """
        return self.modules_updated((module,))

    def modules_updated(self, modules: Iterable[AnyModuleDefinition]) -> "Modules":
        """Update or add the specified modules.

        Note that you shall not use the original modules object after the
        update as its namespace will be detached.

        IMPORTANT: If your updated modules' updating process requires the modules
        information from the project, you have to make sure that the modules are always
        updated during the process.  Otherwise, some namespaces will be stale.

        Args:
            modules (Iterable[AnyModuleDefinition]): The modules to add or
                update.

        Returns:
            Modules: The updated immutable modules object.
        """
        # Store the modules from the iterable.
        modules = tuple(modules)

        # The module names to be updated.
        updated_names = {mod.name for mod in modules}

        defs = (
            tuple(
                # Remove the existing modules with the same name, if any.
                m
                for m in self.module_definitions
                if m.name not in updated_names
            )
            + modules  # And add updated modules
        )
        return self.updated(module_definitions=defs)

    def modules_removed(self, names: Iterable[str]) -> "Modules":
        """Remove the specified modules.

        Args:
            names (Iterable[str]): The names of the modules to remove.

        Returns:
            Modules: The updated immutable modules object.
        """
        remained_modules = tuple(
            m for m in self.module_definitions if m.name not in names
        )

        # check if used modules are removed
        used_modules: set[str] = set()
        for mod in remained_modules:
            if isinstance(mod, GroupedModuleDefinition):
                used_modules |= {inst.module for inst in mod.submodules}

        remained_module_names = {mod.name for mod in remained_modules}

        for mod_name in used_modules:
            if mod_name not in remained_module_names:
                msg = f"Module {mod_name} is used but removed."
                raise ValueError(msg)

        return self.updated(module_definitions=remained_modules)

    def has_module(self, name: str) -> bool:
        """Check if a module exists.

        Args:
            name (str): The name of the module.

        Returns:
            bool: True if the module exists, False otherwise.
        """
        try:
            self.get(name)
            return True
        except KeyError:
            return False
