import collections
import json
import os.path
import shutil
import sys
import tarfile
import tempfile
from typing import (BinaryIO, Dict, Iterator, List, Optional, TextIO, Tuple,
                    Union)

from haoda.backend import xilinx as hls
from tlp import util
from tlp.verilog import ast
from tlp.verilog import xilinx as rtl

from .instance import Instance, Port
from .task import Task


class Program:
  """Describes a TLP self.

  Attributes:
    top: Name of the top-level module.
    work_dir: Working directory.
    is_temp: Whether to delete the working directory after done.
    ports: Tuple of Port objects.
    _tasks: Dict mapping names of tasks to Task objects.
  """

  def __init__(self,
               fp: TextIO,
               cflags: str = None,
               work_dir: Optional[str] = None):
    """Construct Program object from a json file.

    Args:
      fp: TextIO of json object.
      cflags: The cflags send to HLS.
      work_dir: Specifiy a working directory as a string. If None, a temporary
          one will be created.
    """
    obj = json.load(fp)
    self.top: str = obj['top']
    self.cflags = cflags
    self.headers: Dict[str, str] = obj.get('headers', {})
    if work_dir is None:
      self.work_dir = tempfile.mkdtemp(prefix='tlp-')
      self.is_temp = True
    else:
      self.work_dir = os.path.abspath(work_dir)
      os.makedirs(self.work_dir, exist_ok=True)
      self.is_temp = False
    self.ports = tuple(map(Port, obj['tasks'][self.top]['ports']))
    self._tasks: Dict[str, Task] = collections.OrderedDict(
        (name, Task(name=name, **task))
        for name, task in sorted(obj['tasks'].items(), key=lambda x: x[0]))

  def __del__(self):
    if self.is_temp:
      shutil.rmtree(self.work_dir)

  @property
  def tasks(self) -> Tuple[Task, ...]:
    return tuple(self._tasks.values())

  @property
  def rtl_dir(self) -> str:
    return os.path.join(self.work_dir, 'hdl')

  @property
  def cpp_dir(self) -> str:
    cpp_dir = os.path.join(self.work_dir, 'cpp')
    os.makedirs(cpp_dir, exist_ok=True)
    return cpp_dir

  def get_task(self, name: str) -> Task:
    return self._tasks[name]

  def get_cpp(self, name: str) -> str:
    return os.path.join(self.cpp_dir, name + '.cpp')

  def get_tar(self, name: str) -> str:
    os.makedirs(os.path.join(self.work_dir, 'tar'), exist_ok=True)
    return os.path.join(self.work_dir, 'tar', name + '.tar')

  def get_rtl(self, name: str) -> str:
    return os.path.join(self.rtl_dir, name + rtl.RTL_SUFFIX)

  def extract_cpp(self) -> 'Program':
    """Extract HLS C++ files."""
    for task in self._tasks.values():
      with open(self.get_cpp(task.name), 'w') as src_code:
        src_code.write(util.clang_format(task.code))
    for name, content in self.headers.items():
      header_path = os.path.join(self.cpp_dir, name)
      os.makedirs(os.path.dirname(header_path), exist_ok=True)
      with open(header_path, 'w') as header_fp:
        header_fp.write(content)
    return self

  def run_hls(self, clock_period: Union[int, float, str],
              part_num: str) -> 'Program':
    """Run HLS with extracted HLS C++ files and generate tarballs."""

    def worker(task: Task) -> Iterator[None]:
      with open(self.get_tar(task.name), 'wb') as tarfileobj:
        with hls.RunHls(tarfileobj,
                        kernel_files=[(self.get_cpp(task.name), self.cflags)],
                        top_name=task.name,
                        clock_period=clock_period,
                        part_num=part_num) as proc:
          yield
          stdout, stderr = proc.communicate()
        if proc.returncode != 0:
          sys.stdout.write(stdout.decode('utf-8'))
          sys.stderr.write(stderr.decode('utf-8'))
          raise RuntimeError('HLS failed for {}'.format(task.name))
      yield

    tuple(zip(*map(worker, self._tasks.values())))

    return self

  def extract_rtl(self) -> 'Program':
    """Extract HDL files from tarballs generated from HLS."""
    for task in self._tasks.values():
      with tarfile.open(self.get_tar(task.name), 'r') as tarfileobj:
        tarfileobj.extractall(path=self.work_dir)
    return self

  def instrument_rtl(self) -> 'Program':
    """Instrument HDL files generated from HLS."""
    width_table = {port.name: port.width for port in self.ports}

    # extract and parse RTL
    for task in self._tasks.values():
      oldcwd = os.getcwd()
      os.chdir(self.work_dir)
      task.module = rtl.Module([self.get_rtl(task.name)])
      os.chdir(oldcwd)

    # instrument the upper-level RTL
    for task in self._tasks.values():
      if task.is_upper:
        task.module.cleanup()
        self._instantiate_fifos(task)
        is_done_signals, args = self._instantiate_children_tasks(
            task, width_table)
        self._add_global_control(task, is_done_signals, args)

        with open(self.get_rtl(task.name), 'w') as rtl_code:
          rtl_code.write(task.module.code)

      for name, content in rtl.OTHER_MODULES.items():
        with open(self.get_rtl(name), 'w') as rtl_code:
          rtl_code.write(content)

      for file_name in 'async_mmap.v', 'detect_burst.v', 'generate_last.v':
        shutil.copy(
            os.path.join(os.path.dirname(util.__file__), 'assets', 'verilog',
                         file_name), self.rtl_dir)

    return self

  def pack_rtl(self, output_file: BinaryIO) -> 'Program':
    rtl.pack(top_name=self.top,
             ports=self.ports,
             rtl_dir=self.rtl_dir,
             output_file=output_file)
    return self

  def _instantiate_fifos(self, task: Task) -> None:
    for fifo_name, fifo in task.fifos.items():
      task_name, task_idx = fifo['consumed_by']
      fifo_port = task.tasks[task_name][task_idx]['args'][fifo_name]['port']
      child_ports = self.get_task(task_name).module.ports

      # declare wires for FIFOs
      task.module.add_signals(
          ast.Wire(name=fifo_name + suffix,
                   width=child_ports[fifo_port + suffix].width)
          for suffix in rtl.ISTREAM_SUFFIXES)

      task_name, task_idx = fifo['produced_by']
      fifo_port = task.tasks[task_name][task_idx]['args'][fifo_name]['port']
      child_ports = self.get_task(task_name).module.ports

      # declare wires for FIFOs
      task.module.add_signals(
          ast.Wire(name=fifo_name + suffix,
                   width=child_ports[fifo_port + suffix].width)
          for suffix in rtl.OSTREAM_SUFFIXES)

      # add FIFO instances
      width = child_ports[fifo_port + rtl.OSTREAM_SUFFIXES[0]].width
      # TODO: err properly if not integer literals
      fifo_width = int(width.msb.value) - int(width.lsb.value) + 1
      task.module.add_fifo_instance(name=fifo_name,
                                    width=fifo_width,
                                    depth=fifo['depth'])

      # print debugging info
      debugging_blocks = []
      col_width = max(
          max(len(name), len(util.get_instance_name(fifo['consumed_by'])),
              len(util.get_instance_name(fifo['produced_by'])))
          for name, fifo in task.fifos.items())
      fmtargs = {
          'fifo_prefix': '\\033[97m',
          'fifo_suffix': '\\033[0m',
          'task_prefix': '\\033[90m',
          'task_suffix': '\\033[0m',
      }
      for suffixes, fmt, fifo_tag in zip(
          (rtl.ISTREAM_SUFFIXES, rtl.OSTREAM_SUFFIXES),
          ('DEBUG: R: {fifo_prefix}{fifo:>{width}}{fifo_suffix} -> '
           '{task_prefix}{task:<{width}}{task_suffix} %h',
           'DEBUG: W: {task_prefix}{task:>{width}}{task_suffix} -> '
           '{fifo_prefix}{fifo:<{width}}{fifo_suffix} %h'),
          ('consumed_by', 'produced_by')):
        display = ast.SingleStatement(statement=ast.SystemCall(
            syscall='display',
            args=(ast.StringConst(
                value=fmt.format(width=col_width,
                                 fifo=fifo_name,
                                 task=(util.get_instance_name(fifo[fifo_tag])),
                                 **fmtargs)),
                  ast.Identifier(name=fifo_name + suffixes[0]))))
        debugging_blocks.append(
            ast.Always(
                sens_list=rtl.CLK_SENS_LIST,
                statement=ast.make_block(
                    ast.IfStatement(cond=ast.Eq(
                        left=ast.Identifier(name=fifo_name + suffixes[-1]),
                        right=rtl.TRUE,
                    ),
                                    true_statement=ast.make_block(display),
                                    false_statement=None))))
      task.module.add_logics(debugging_blocks)

  def _instantiate_children_tasks(
      self,
      task: Task,
      width_table: Dict[str, int],
  ) -> Tuple[List[ast.Identifier], Dict[str, rtl.RegMeta]]:
    is_done_signals: List[ast.Identifier] = []
    args: Dict[str, rtl.RegMeta] = {}
    async_mmap_args: Dict[str, List[str]] = collections.OrderedDict()

    # iterate over all sub-tasks
    for task_name, instance_objs in task.tasks.items():
      child = self.get_task(task_name)
      child_port_set = set(child.module.ports)

      # iterate over all instances of sub-tasks
      for instance_idx, instance_obj in enumerate(instance_objs):
        instance = Instance(self.get_task(task_name),
                            verilog=rtl,
                            instance_id=instance_idx,
                            **instance_obj)

        for arg_name, arg in sorted((item for item in instance.args.items()),
                                    key=lambda x: x[0]):
          if arg.cat not in {
              Instance.Arg.Cat.ISTREAM,
              Instance.Arg.Cat.OSTREAM,
          }:
            width = 64  # 64-bit address
            if arg.cat == Instance.Arg.Cat.SCALAR:
              width = width_table[arg_name]
            args.setdefault(arg_name, rtl.RegMeta(name=arg_name, width=width))
          # arg_name is the upper-level name
          # port_name is the lower-level name
          port_name = instance_obj['args'][arg_name]['port']

          # check which ports are used for async_mmap
          if arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
            for tag in 'read_addr', 'read_data', 'write_addr', 'write_data':
              if set(x.portname for x in rtl.generate_async_mmap_ports(
                  tag=tag,
                  port=port_name,
                  arg=arg_name,
                  instance=instance,
              )) & set(self._tasks[task_name].module.ports):
                async_mmap_args.setdefault(arg_name, []).append(tag)

          # add m_axi ports to the upper task
          if arg.cat == Instance.Arg.Cat.MMAP or (
              arg.cat == Instance.Arg.Cat.ASYNC_MMAP and task.name == self.top):
            task.module.add_m_axi(name=arg_name,
                                  data_width=width_table[arg_name])

          # declare wires or forward async_mmap ports
          for tag in async_mmap_args.get(arg_name, []):
            if task.name == self.top:
              task.module.add_signals(
                  rtl.generate_async_mmap_signals(
                      tag=tag, arg=arg_name, data_width=width_table[arg_name]))
            else:
              task.module.add_ports(
                  rtl.generate_async_mmap_ioports(
                      tag=tag, arg=arg_name, data_width=width_table[arg_name]))

        if instance.is_autorun:
          task.module.add_logics([
              ast.Assign(
                  left=instance.start,
                  right=rtl.TRUE,
              ),
          ])
        else:
          # set up state
          state00 = ast.IntConst("2'b00")
          state01 = ast.IntConst("2'b01")
          state11 = ast.IntConst("2'b11")
          state10 = ast.IntConst("2'b10")

          if_branch = (instance.set_state(state00))
          else_branch = ((
              ast.make_if_with_block(
                  cond=instance.is_state(state00),
                  true=ast.make_if_with_block(
                      cond=rtl.START_Q[-1],
                      true=instance.set_state(state01),
                  ),
              ),
              ast.make_if_with_block(
                  cond=instance.is_state(state01),
                  true=ast.make_if_with_block(
                      cond=instance.ready,
                      true=ast.make_if_with_block(
                          cond=instance.done,
                          true=instance.set_state(state10),
                          false=instance.set_state(state11),
                      )),
              ),
              ast.make_if_with_block(
                  cond=instance.is_state(state11),
                  true=ast.make_if_with_block(
                      cond=instance.done,
                      true=instance.set_state(state10),
                  ),
              ),
              ast.make_if_with_block(
                  cond=instance.is_state(state10),
                  true=ast.make_if_with_block(
                      cond=rtl.DONE,
                      true=instance.set_state(state00),
                  ),
              ),
          ))
          task.module.add_logics([
              ast.Always(
                  sens_list=rtl.CLK_SENS_LIST,
                  statement=ast.make_block(
                      ast.make_if_with_block(
                          cond=rtl.RESET_COND,
                          true=if_branch,
                          false=else_branch,
                      )),
              ),
              ast.Assign(
                  left=instance.start,
                  right=instance.is_state(state01),
              ),
              ast.Assign(
                  left=instance.is_done,
                  right=ast.Unot(
                      ast.Pointer(
                          var=instance.state,
                          ptr=ast.IntConst(0),
                      )),
              ),
          ])

        if not instance.is_autorun:
          is_done_signals.append(instance.is_done)

        # insert handshake signals
        task.module.add_signals(instance.handshake_signals)

        # add task module instances
        portargs = list(
            rtl.generate_handshake_ports(instance.name, instance.is_autorun))
        for arg_name, arg in sorted((item for item in instance.args.items()),
                                    key=lambda x: x[0]):
          if arg.cat == Instance.Arg.Cat.SCALAR:
            portargs.append(
                ast.PortArg(portname=arg.port, argname=args[arg_name][-1]))
          elif arg.cat == Instance.Arg.Cat.ISTREAM:
            portargs.extend(
                rtl.generate_istream_ports(port=arg.port, arg=arg_name))
            portargs.extend(portarg for portarg in util.generate_peek_ports(
                rtl, port=arg.port, arg=arg_name)
                            if portarg.portname in child_port_set)

          elif arg.cat == Instance.Arg.Cat.OSTREAM:
            portargs.extend(
                rtl.generate_ostream_ports(port=arg.port, arg=arg_name))
          elif arg.cat == Instance.Arg.Cat.MMAP:
            portargs.extend(
                rtl.generate_m_axi_ports(port=arg.port,
                                         arg=arg_name,
                                         arg_reg=args[arg_name][-1].name))
          elif arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
            for tag in async_mmap_args[arg_name]:
              portargs.extend(
                  rtl.generate_async_mmap_ports(
                      tag=tag,
                      port=instance_obj['args'][arg_name]['port'],
                      arg=arg_name,
                      instance=instance))

        task.module.add_instance(module_name=child.name,
                                 instance_name=instance.name,
                                 ports=portargs)

    # instantiate async_mmap module in the top module
    if task.name == self.top:
      for arg_name in async_mmap_args:
        task.module.add_async_mmap_instance(
            name=arg_name,
            reg_name=args[arg_name][-1],
            tags=async_mmap_args[arg_name],
            data_width=width_table[arg_name],
        )

    return is_done_signals, args

  def _add_global_control(
      self,
      task: Task,
      is_done_signals: List[ast.Identifier],
      args: Dict[str, rtl.RegMeta],
  ) -> None:
    # global state machine
    state00 = ast.IntConst("2'b00")
    state01 = ast.IntConst("2'b01")
    state10 = ast.IntConst("2'b10")

    def is_state(state: ast.IntConst) -> ast.Eq:
      return ast.Eq(left=rtl.STATE, right=state)

    def set_state(state: ast.IntConst) -> ast.NonblockingSubstitution:
      return ast.NonblockingSubstitution(left=rtl.STATE, right=state)

    task.module.add_signals([
        ast.Reg(rtl.STATE.name, width=ast.make_width(2)),
        *rtl.START_Q.signals,
        *rtl.DONE_Q.signals,
        *rtl.IDLE_Q.signals,
        *(y for x in args.values() for y in x.signals),
    ])
    task.module.add_logics([
        ast.Always(
            sens_list=rtl.CLK_SENS_LIST,
            statement=ast.make_block(
                ast.make_if_with_block(
                    cond=rtl.RESET_COND,
                    true=set_state(state00),
                    false=ast.make_block([
                        ast.make_if_with_block(
                            cond=is_state(state00),
                            true=ast.make_if_with_block(
                                cond=rtl.START_Q[-1],
                                true=set_state(state01),
                            ),
                        ),
                        ast.make_if_with_block(
                            cond=is_state(state01),
                            true=ast.make_if_with_block(
                                cond=ast.make_operation(
                                    operator=ast.Land,
                                    nodes=reversed(is_done_signals)),
                                true=set_state(state10),
                            ),
                        ),
                        ast.make_if_with_block(
                            cond=is_state(state10),
                            true=set_state(state00),
                        ),
                    ]),
                )),
        ),
        ast.Always(
            sens_list=rtl.CLK_SENS_LIST,
            statement=ast.make_block(
                ast.NonblockingSubstitution(left=q[i + 1], right=q[i])
                for q in (rtl.START_Q, rtl.DONE_Q, rtl.IDLE_Q)
                for i in range(rtl.REGISTER_LEVEL)),
        ),
        ast.Assign(left=rtl.START_Q[0], right=rtl.START),
        ast.Assign(
            left=rtl.DONE_Q[0],
            right=ast.Pointer(var=rtl.STATE, ptr=ast.IntConst(1)),
        ),
        ast.Assign(left=rtl.IDLE_Q[0], right=is_state(state00)),
        ast.Assign(left=rtl.DONE, right=rtl.DONE_Q[-1]),
        ast.Assign(left=rtl.READY, right=rtl.DONE),
        ast.Assign(left=rtl.IDLE, right=rtl.IDLE_Q[-1]),
        ast.Always(
            sens_list=rtl.CLK_SENS_LIST,
            statement=ast.make_block(
                ast.NonblockingSubstitution(left=q[i + 1], right=q[i])
                for q in args.values()
                for i in range(rtl.REGISTER_LEVEL)),
        ),
        *(ast.Assign(left=meta[0], right=ast.Identifier(name))
          for name, meta in args.items()),
    ])
