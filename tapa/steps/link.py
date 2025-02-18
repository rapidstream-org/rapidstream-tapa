"""Link the generated RTL with the floorplan results."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


import click

from tapa.steps.common import (
    is_pipelined,
    load_persistent_context,
    store_persistent_context,
)


@click.command()
def link() -> None:
    """Link the generated RTL."""
    settings = load_persistent_context("settings")
    settings["linked"] = True
    store_persistent_context("settings")

    is_pipelined("link", True)
