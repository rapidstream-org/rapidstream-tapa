from cmath import exp
import json
import logging
import os
from typing import Dict, Optional, Tuple

import click

import tapa.core
import tapa.util

_logger = logging.getLogger().getChild(__name__)


def forward_applicable(ctx: click.Context, command: click.Command,
                       kwargs: Dict):
  names = list(map(lambda param: param.name, command.params))
  args = {key: value for (key, value) in kwargs.items() if key in names}
  ctx.invoke(command, **args)


def get_work_dir() -> str:
  """Returns the working directory of TAPA."""
  return click.get_current_context().obj['work-dir']


def is_pipelined(step: str, pipelined: Optional[bool] = None):
  """Gets or sets if a step is pipelined in this single run."""
  if pipelined is None:
    return click.get_current_context().obj.get(f'{step}_pipelined', False)
  else:
    click.get_current_context().obj[f'{step}_pipelined'] = pipelined


def load_persistent_context(name: str) -> Dict:
  """Try load context from the flow or from the workdir.

  Args:
    name: Name of the context, e.g. program, settings.
  
  Returns:
    The context.
  """
  local_ctx = click.get_current_context().obj

  if local_ctx.get(name, None) is not None:
    _logger.info(f'reusing TAPA {name} from upstream flows.')

  else:
    json_file = os.path.join(get_work_dir(), f'{name}.json')
    _logger.info(f'loading TAPA program from json `{json_file}`.')

    try:
      with open(json_file, 'r') as input_fp:
        obj = json.loads(input_fp.read())
    except FileNotFoundError:
      raise click.BadArgumentUsage(
          f'Program description {json_file} does not exist.  Either '
          '`tapa analyze` wasn\'t executed, or you specified a wrong path.')
    local_ctx[name] = obj

  return local_ctx[name]


def load_tapa_program() -> tapa.core.Program:
  """Try load program description from the flow or from the workdir.

  Returns:
    Loaded program description.
  """
  local_ctx = click.get_current_context().obj
  if 'tapa-program' not in local_ctx:
    local_ctx['tapa-program'] = tapa.core.Program(
        load_persistent_context('program'), local_ctx['work-dir'])

  return local_ctx['tapa-program']


def store_persistent_context(name: str, ctx: Dict = None) -> None:
  """Try store context to the workdir.

  Args:
    name: Name of the context, e.g. program, settings.
    ctx: The context to be stored.  If not given, use local context.
  """
  local_ctx = click.get_current_context().obj

  # If the context is given, use that instead
  if ctx is not None:
    local_ctx[name] = ctx

  json_file = os.path.join(get_work_dir(), f'{name}.json')
  _logger.info(f'writing TAPA {name} to json `{json_file}`.')

  with open(json_file, 'w') as output_fp:
    json.dump(local_ctx[name], output_fp)


def store_tapa_program(prog: tapa.core.Program):
  """Store program description to the flow for downstream reuse.

  Args:
    prog: The TAPA program for reuse.
  """
  click.get_current_context().obj['tapa-program'] = prog


def switch_work_dir(path: str):
  """Switch working directory to `path`."""
  os.makedirs(path, exist_ok=True)
  click.get_current_context().obj['work-dir'] = path
