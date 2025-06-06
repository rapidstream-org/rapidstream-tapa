"""Sanity check for graph IR."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging

from tapa.graphir.types import GroupedModuleDefinition

_logger = logging.getLogger().getChild(__name__)


def check_missing_wire(mod: GroupedModuleDefinition) -> None:
    """Check if wire definition is missing in group modules."""
    defined_ids = {wire.name for wire in mod.wires}
    defined_ids |= {port.name for port in mod.ports}
    defined_ids |= {param.name for param in mod.parameters}

    used_ids: set[str] = set()
    for inst in mod.submodules:
        for conn in inst.connections:
            used_ids |= conn.expr.get_used_identifiers()

    if not used_ids:
        _logger.critical("Check if no identifiers used in module %s", mod.name)

    if missing_wires := used_ids - defined_ids:
        _logger.error("Missing wire definitions in module %s:", mod.name)
        for wire in missing_wires:
            _logger.error("    %s", wire)

        msg = "Check for missing wire definitions."
        raise ValueError(msg)
