"""Print TAPA version to standard output."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import sys

import click

from tapa import __version__


@click.command()
def version() -> None:
    """Print TAPA version to standard output."""
    sys.stdout.write(__version__)
