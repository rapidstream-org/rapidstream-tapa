module {{ name -}}

{%- if parameters|length > 0 -%}
{# #} #(
{# #}    {# #}
    {%- filter indent(width=4) -%}
    {%- include "includes/header-parameters.v" -%}
    {%- endfilter -%}{# #}
) {%- endif -%}

{# #} (
{%- if ports|length > 0 -%}{# #}
    {# #}{%- filter indent(width=4) -%}
    {%- include "includes/header-ports.v" -%}
    {%- endfilter -%}{# #}
{# #}{%- endif -%}
);
