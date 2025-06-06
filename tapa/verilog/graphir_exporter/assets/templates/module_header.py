"""Module header template for TAPAS GraphIR exporter."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.verilog.graphir_exporter.assets.templates.includes.header_parameters import (
    HEADER_PARAMETERS,
)
from tapa.verilog.graphir_exporter.assets.templates.includes.header_ports import (
    HEADER_PORTS,
)

MODULE_HEADER = f"""
module {{{{ name -}}}}

{{%- if parameters|length > 0 -%}}
{{# #}} #(
{{# #}}    {{# #}}
    {{%- filter indent(width=4) -%}}
    {HEADER_PARAMETERS}
    {{%- endfilter -%}}{{# #}}
) {{%- endif -%}}

{{# #}} (
{{%- if ports|length > 0 -%}}{{# #}}
    {{# #}}{{%- filter indent(width=4) -%}}
    {HEADER_PORTS}
    {{%- endfilter -%}}{{# #}}
{{# #}}{{%- endif -%}}
);
"""
