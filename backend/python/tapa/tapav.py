#!/usr/bin/python3
import argparse
import collections
import sys
from typing import Dict, Set

import tapa.core


def main():
  parser = argparse.ArgumentParser(
      prog='tapav', description='TAPA Visualizer')
  parser.add_argument(dest='program',
                      help='input ‘program.json’, default to stdin',
                      nargs='?',
                      type=argparse.FileType('r'),
                      default=sys.stdin)
  parser.add_argument('-o',
                      '--output',
                      dest='output',
                      help='output graphviz file, default to stdout',
                      type=argparse.FileType('w'),
                      default=sys.stdout)
  args = parser.parse_args()

  task_fmt = '"{name}#{id}"'
  font = 'Arial'
  program = tapa.core.Program(args.program)
  output = args.output
  output.write(f'digraph "{program.top}" {{\n')
  output.write(f'  label = "{program.top}";\n')
  output.write(f'  graph [fontname = "{font}"];\n')
  output.write(f'  node [fontname = "{font}"];\n')
  output.write(f'  edge [fontname = "{font}"];\n')
  output.write('  rankdir = LR;\n')
  levels: Dict[str, Set[int]] = collections.defaultdict(set)
  for task in program.tasks:
    if task.is_upper:
      for fifo_name, fifo_attr in task.fifos.items():
        src_task_name, src_task_id = fifo_attr['produced_by']
        dst_task_name, dst_task_id = fifo_attr['consumed_by']
        src = task_fmt.format(name=src_task_name, id=src_task_id)
        dst = task_fmt.format(name=dst_task_name, id=dst_task_id)
        label = fifo_name
        label += '#%s' % fifo_attr['depth']
        levels[src_task_name].add(src_task_id)
        levels[dst_task_name].add(dst_task_id)
        output.write(f'  {src} -> {dst} [ label = "{label}" ];\n')
  for name, ids in levels.items():
    instances = ', '.join(task_fmt.format(name=name, id=x) for x in ids)
    output.write(f'  {{ rank = same; {instances} }}\n')
  output.write('}\n')


if __name__ == '__main__':
  main()
