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
