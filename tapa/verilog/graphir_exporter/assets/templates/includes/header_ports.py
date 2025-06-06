"""Header ports template for TAPAS GraphIR exporter."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

HEADER_PORTS = """
{%- set max_type_len = ports
    | map(attribute='type')
    | map('length')
    | max -%}

{%- set max_range_len = ports
| map(attribute='range')
| map('default', '', true)
| map('string')
| map('length')
| max -%}

{%- for port in ports -%}
{% if (not port.hierarchical_name.is_empty())
    and (not port.hierarchical_name.is_nonexist())
    and (port.hierarchical_name.__str__() != port.name) -%}
/**   {{ port.hierarchical_name }}   **/
{% endif -%}
{{ port.type.ljust(max_type_len) }} {# #}

{%- if port.range -%}
[{{ port.range.__str__().rjust(max_range_len) }}] {# #}
{%- else -%}

{%- if max_range_len > 0 -%}
{{ ''.ljust(max_range_len + 3) }}
{%- endif -%}

{%- endif -%}

{{ port.name }}

{%- if not loop.last -%}
,
{# #}
{%- endif -%}
{%- endfor -%}
"""
