from __future__ import annotations

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import json
import logging
import os

import click

from tapa.core import Program

_logger = logging.getLogger().getChild(__name__)


def forward_applicable(
    ctx: click.Context,
    command: click.Command,
    kwargs: dict,
) -> None:
    """Forward only applicable arguments to a subcommand."""
    names = [param.name for param in command.params]
    args = {key: value for (key, value) in kwargs.items() if key in names}
    ctx.invoke(command, **args)


def get_work_dir() -> str:
    """Returns the working directory of TAPA."""
    return click.get_current_context().obj["work-dir"]


def is_pipelined(step: str, pipelined: bool | None = None) -> bool | None:
    """Gets or sets if a step is pipelined in this single run."""
    if pipelined is None:
        return click.get_current_context().obj.get(f"{step}_pipelined", False)
    click.get_current_context().obj[f"{step}_pipelined"] = pipelined
    return None


def load_persistent_context(name: str) -> dict:
    """Try load context from the flow or from the workdir.

    Args:
    ----
      name: Name of the context, e.g. graph, settings.

    Returns:
    -------
      The context.

    """
    local_ctx = click.get_current_context().obj

    if local_ctx.get(name, None) is not None:
        _logger.info("reusing TAPA %s from upstream flows.", name)

    else:
        json_file = os.path.join(get_work_dir(), f"{name}.json")
        _logger.info("loading TAPA graph from json `%s`.", json_file)

        try:
            with open(json_file, encoding="utf-8") as input_fp:
                obj = json.loads(input_fp.read())
        except FileNotFoundError:
            msg = (
                f"Graph description {json_file} does not exist.  Either "
                "`tapa analyze` wasn't executed, or you specified a wrong path."
            )
            raise click.BadArgumentUsage(
                msg,
            )
        local_ctx[name] = obj

    return local_ctx[name]


def load_tapa_program() -> Program:
    """Try load program description from the flow or from the workdir.

    Returns
    -------
      Loaded program description.

    """
    local_ctx = click.get_current_context().obj
    if "tapa-program" not in local_ctx:
        local_ctx["tapa-program"] = Program(
            load_persistent_context("graph"),
            local_ctx["work-dir"],
        )

    return local_ctx["tapa-program"]


def store_persistent_context(name: str, ctx: dict | None = None) -> None:
    """Try store context to the workdir.

    Args:
    ----
      name: Name of the context, e.g. program, settings.
      ctx: The context to be stored.  If not given, use local context.

    """
    local_ctx = click.get_current_context().obj

    # If the context is given, use that instead
    if ctx is not None:
        local_ctx[name] = ctx

    json_file = os.path.join(get_work_dir(), f"{name}.json")
    _logger.info("writing TAPA %s to json `%s`.", name, json_file)

    with open(json_file, "w", encoding="utf-8") as output_fp:
        json.dump(local_ctx[name], output_fp)


def store_tapa_program(prog: Program) -> None:
    """Store program description to the flow for downstream reuse.

    Args:
    ----
      prog: The TAPA program for reuse.

    """
    click.get_current_context().obj["tapa-program"] = prog


def switch_work_dir(path: str) -> None:
    """Switch working directory to `path`."""
    os.makedirs(path, exist_ok=True)
    click.get_current_context().obj["work-dir"] = path

    # update tapa.core.Program if created
    if "tapa-program" in click.get_current_context().obj:
        click.get_current_context().obj["tapa-program"].work_dir = path
