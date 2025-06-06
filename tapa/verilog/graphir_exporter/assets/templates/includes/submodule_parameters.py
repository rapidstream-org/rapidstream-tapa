"""Submodule parameters template for TAPAS GraphIR exporter."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

SUBMODULE_PARAMETERS = """
{%- set maxlen = submodule.parameters
    | map(attribute='name')
    | map('length')
    | max -%}

{% for param in submodule.parameters %}
{% if (not param.hierarchical_name.is_empty())
    and (not param.hierarchical_name.is_nonexist())
    and (param.hierarchical_name.__str__() != param.name) -%}
/**   {{ param.hierarchical_name }}   **/
{% endif -%}
.{{ param.name.ljust(maxlen) }} ({{ param.expr }})
{%- if not loop.last -%}
,
{%- endif -%}
{% endfor %}
{# #}
"""
