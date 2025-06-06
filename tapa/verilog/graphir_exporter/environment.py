"""Jinja2 environment for template rendering."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from pathlib import Path

from jinja2 import Environment, FileSystemLoader, select_autoescape

CURR_DIR = Path(__file__).resolve().parent
EXPORTER_TEMPLATES_DIR = CURR_DIR / "assets" / "templates"

env = Environment(
    loader=FileSystemLoader(EXPORTER_TEMPLATES_DIR),
    autoescape=select_autoescape(),
)
