{%- set maxlen = submodule.connections
    | map(attribute='name')
    | map('length')
    | max -%}

{% for conn in submodule.connections %}
{% if (not conn.hierarchical_name.is_empty())
    and (not conn.hierarchical_name.is_nonexist())
    and (conn.hierarchical_name.__str__() != conn.name) -%}
/**   {{ conn.hierarchical_name }}   **/
{% endif -%}
.{{ conn.name.ljust(maxlen) }} ({{ conn.expr }})
{%- if not loop.last -%}
,
{%- endif -%}
{% endfor %}
{# #}
