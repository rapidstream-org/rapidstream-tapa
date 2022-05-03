import json
import logging
import os
from typing import Dict

import tapa.core
import tapa.util

_logger = logging.getLogger().getChild(__name__)


def load_program(flow_ctx: Dict) -> tapa.core.Program:
  """Try load program description from the flow or from the workdir.

  Args:
    flow_ctx: Flow context from upstream.
  
  Returns:
    Loaded program description.
  """
  
  work_dir: str
  work_dir = flow_ctx['work-dir']

  if flow_ctx.get('program', None) is not None:
    _logger.info('reusing the TAPA program from upstream flow.')
    return flow_ctx.get('program', None)
    
  else:
    program_json_file = os.path.join(work_dir, 'program.json')
    _logger.info(f'loading TAPA program from json `{program_json_file}`.')
    with open(program_json_file, 'r') as input_fp:
      obj = json.loads(input_fp.read())

  return tapa.core.Program(obj, work_dir)