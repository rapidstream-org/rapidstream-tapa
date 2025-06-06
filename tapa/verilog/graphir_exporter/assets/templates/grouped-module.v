{% include 'includes/common-header.v' %}
{% include 'includes/module-header.v' %}

{%- set wires = module.wires %}
{% include 'includes/wires.v' %}

{%- set submodules = module.submodules %}
{% include 'includes/submodules.v' %}
{% include 'includes/module-footer.v' %}
