{%- set max_name_len = parameters
    | map(attribute='name')
    | map('length')
    | max -%}

{%- set max_range_len = parameters
| map(attribute='range')
| map('default', '', true)
| map('string')
| map('length')
| max -%}

{%- for param in parameters -%}
{% if (not param.hierarchical_name.is_empty())
    and (not param.hierarchical_name.is_nonexist())
    and (param.hierarchical_name.__str__() != param.name) -%}
/**   {{ param.hierarchical_name }}   **/
{% endif -%}
parameter {# #}

{%- if param.range -%}
[{{ param.range.__str__().rjust(max_range_len) }}] {# #}
{%- else -%}

{%- if max_range_len > 0 -%}
{{ ''.ljust(max_range_len + 3) }}
{%- endif -%}

{%- endif -%}

{{ param.name.ljust(max_name_len) }} {# #}

{%- if param.expr -%}
= {{ param.expr }}
{%- endif -%}

{%- if not loop.last -%}
,
{# #}
{%- endif -%}
{%- endfor -%}
