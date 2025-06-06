"""Grouped module template for TAPAS GraphIR exporter."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.verilog.graphir_exporter.assets.templates.includes.common_header import (
    COMMON_HEADER,
)
from tapa.verilog.graphir_exporter.assets.templates.includes.module_footer import (
    MODULE_FOOTER,
)
from tapa.verilog.graphir_exporter.assets.templates.includes.module_header import (
    MODULE_HEADER,
)
from tapa.verilog.graphir_exporter.assets.templates.includes.submodules import (
    SUBMODULES,
)
from tapa.verilog.graphir_exporter.assets.templates.includes.wires import WIRES

GROUPED_MODULE = f"""
{COMMON_HEADER}
{MODULE_HEADER}

{{% set wires = module.wires %}}
{WIRES}

{{% set submodules = module.submodules %}}
{SUBMODULES}
{MODULE_FOOTER}
"""
