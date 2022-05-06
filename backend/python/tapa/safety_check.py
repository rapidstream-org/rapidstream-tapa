import logging
from typing import List

from tapa.task import Task

DISABLED_MMAP_NAME_LIST = {
    "begin",
    "end",
    "in",
    "input",
    "out",
    "output",
    "reg",
    "wire",
}

_logger = logging.getLogger().getChild(__name__)


def check_mmap_arg_name(task_list: List[Task]) -> None:
  """
  mmap arguments cannot be named "in" or "out". Otherwise HLS will have inconsistent naming
  convention in the generated AXI interface
  """
  for task in task_list:
    if task.is_upper:
      for port_name in task.ports.keys():
        if port_name in DISABLED_MMAP_NAME_LIST:
          _logger.error('Task arguments cannot be among the reserved keywords: %s', DISABLED_MMAP_NAME_LIST)
          raise AssertionError
