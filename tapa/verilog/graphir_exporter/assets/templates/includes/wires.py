"""Wires template for TAPAS GraphIR exporter."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

WIRES = """
{%- set max_range_len = wires
    | map(attribute='range')
    | map('string')
    | map('length')
    | max -%}

{%- for wire in wires %}
{% if (not wire.hierarchical_name.is_empty())
    and (not wire.hierarchical_name.is_nonexist())
    and (wire.hierarchical_name.__str__() != wire.name) -%}
/**   {{ wire.hierarchical_name }}   **/
{% endif -%}
wire {# #}

{%- if wire.range -%}
[{{ wire.range.__str__().rjust(max_range_len) }}] {# #}
{%- else -%}

{%- if max_range_len > 0 -%}
{{ ''.ljust(max_range_len + 3) }}
{%- endif -%}

{%- endif -%}

{{ wire.name }};
{%- endfor -%}
{# #}
"""
