import collections
import enum
import json
import os.path
import shutil
import sys
import tarfile
import tempfile
from typing import (BinaryIO, Dict, Iterator, List, Optional, TextIO, Tuple,
                    Union)

from pyverilog.vparser import ast

import tlp.util


class Task:
  """Describes a TLP task.

  Attributes:
    level: Task.Level, upper or lower.
    name: str, name of the task, function name as defined in the source code.
    code: str, HLS C++ code of this task.
    tasks: A dict mapping child task names to json instance description objects.
    fifos: A dict mapping child fifo names to json FIFO description objects.
    module: rtl.Module, should be attached after RTL code is generated.

  Properties:
    is_upper: bool, True if this task is an upper-level task.
    is_lower: bool, True if this task is an lower-level task.
  """

  class Level(enum.Enum):
    LOWER = 0
    UPPER = 1

  def __init__(self, **kwargs):
    level = kwargs.pop('level')  # type: Union[Task.Level, str]
    if isinstance(level, str):
      if level == 'lower':
        level = Task.Level.LOWER
      elif level == 'upper':
        level = Task.Level.UPPER
    if not isinstance(level, Task.Level):
      raise TypeError('unexpected `level`: ' + level)
    self.level = level
    self.name = kwargs.pop('name')  # type: str
    self.code = kwargs.pop('code')  # type: str
    self.tasks = collections.OrderedDict()
    self.fifos = collections.OrderedDict()
    if self.is_upper:
      self.tasks = collections.OrderedDict(
          sorted((item for item in kwargs.pop('tasks').items()),
                 key=lambda x: x[0]))
      self.fifos = collections.OrderedDict(
          sorted((item for item in kwargs.pop('fifos').items()),
                 key=lambda x: x[0]))
    self.module = None  # type: Any

  @property
  def is_upper(self) -> bool:
    return self.level == Task.Level.UPPER

  @property
  def is_lower(self) -> bool:
    return self.level == Task.Level.LOWER


class Port:

  def __init__(self, obj):
    self.cat = {
        'istream': Instance.Arg.Cat.ISTREAM,
        'ostream': Instance.Arg.Cat.OSTREAM,
        'scalar': Instance.Arg.Cat.SCALAR,
        'mmap': Instance.Arg.Cat.MMAP,
        'async_mmap': Instance.Arg.Cat.ASYNC_MMAP,
    }[obj['cat']]
    self.name = obj['name']
    self.ctype = obj['type']
    self.width = obj['width']


class Program:
  """Describes a TLP self.

  Attributes:
    top: Name of the top-level module.
    work_dir: Working directory.
    is_temp: Whether to delete the working directory after done.
    hls: HLS backend.
    ports: Tuple of Port objects.
    _tasks: Dict mapping names of tasks to Task objects.
  """

  def __init__(self, fp: TextIO, hls, rtl, work_dir: Optional[str] = None):
    """Construct Program object from a json file.

    Args:
      fp: TextIO of json object.
      work_dir: Specifiy a working directory as a string. If None, a temporary
          one will be created.
    """
    obj = json.load(fp)
    self.top = obj['top']  # type: str
    self.hls = hls
    self.rtl = rtl
    self.headers = obj.get('headers', {})  # type: Dict[str, str]
    if work_dir is None:
      self.work_dir = tempfile.mkdtemp(prefix='tlp-')
      self.is_temp = True
    else:
      self.work_dir = os.path.abspath(work_dir)
      os.makedirs(self.work_dir, exist_ok=True)
      self.is_temp = False
    self.ports = tuple(map(Port, obj['tasks'][self.top]['ports']))
    self._tasks = collections.OrderedDict(
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
    return os.path.join(self.rtl_dir, name + self.rtl.RTL_SUFFIX)

  def extract_cpp(self) -> 'Program':
    """Extract HLS C++ files."""
    for task in self._tasks.values():
      with open(self.get_cpp(task.name), 'w') as src_code:
        src_code.write(tlp.util.clang_format(task.code))
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
        with self.hls.RunHls(tarfileobj,
                             kernel_files=[self.get_cpp(task.name)],
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
      task.module = self.rtl.Module([self.get_rtl(task.name)])
      os.chdir(oldcwd)

    # instrument the upper-level RTL
    for task in self._tasks.values():
      if task.is_upper:
        # instantiate FIFOs
        for fifo_name, fifo in task.fifos.items():
          task_name, task_idx = fifo['consumed_by']
          fifo_port = task.tasks[task_name][task_idx]['args'][fifo_name]['port']
          child_ports = self.get_task(task_name).module.ports
          width = child_ports[fifo_port + self.rtl.ISTREAM_SUFFIXES[0]].width
          # TODO: err properly if not integer literals
          fifo_width = int(width.msb.value) - int(width.lsb.value) + 1

          # declare wires for FIFOs
          task.module.add_signals(
              ast.Wire(name=fifo_name + suffix,
                       width=child_ports[fifo_port + suffix].width)
              for suffix in self.rtl.ISTREAM_SUFFIXES)

          task_name, task_idx = fifo['produced_by']
          fifo_port = task.tasks[task_name][task_idx]['args'][fifo_name]['port']
          child_ports = self.get_task(task_name).module.ports
          width = child_ports[fifo_port + self.rtl.OSTREAM_SUFFIXES[0]].width
          # TODO: err properly if not integer literals
          fifo_width = int(width.msb.value) - int(width.lsb.value) + 1

          # declare wires for FIFOs
          task.module.add_signals(
              ast.Wire(name=fifo_name + suffix,
                       width=child_ports[fifo_port + suffix].width)
              for suffix in self.rtl.OSTREAM_SUFFIXES)

          # add FIFO instances
          task.module.add_fifo_instance(name=fifo_name,
                                        width=fifo_width,
                                        depth=fifo['depth'])

        # instantiate children tasks
        reset_if_branch = []
        reset_else_branch = []
        is_done_assignments = []
        start_assignments = []
        handshake_output_signals = {
            'done': [],
            'idle': [],
            'ready': []
        }  # type: Dict[str, List[ast.Identifier]]

        async_mmap_args = collections.OrderedDict(
        )  # type: Dict[str, List[str]]

        # iterate over all sub-tasks
        for task_name, instance_objs in task.tasks.items():
          child = self.get_task(task_name)
          child_port_set = set(child.module.ports)

          # iterate over all instances of sub-tasks
          for instance_idx, instance_obj in enumerate(instance_objs):
            instance = Instance(self.get_task(task_name),
                                verilog=self.rtl,
                                instance_id=instance_idx,
                                **instance_obj)

            for arg_name, arg in sorted(
                (item for item in instance.args.items()), key=lambda x: x[0]):
              # arg_name is the upper-level name
              # port_name is the lower-level name
              port_name = instance_obj['args'][arg_name]['port']

              # check which ports are used for async_mmap
              if arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
                for tag in 'read_addr', 'read_data', 'write_addr', 'write_data':
                  if set(x.portname for x in self.rtl.generate_async_mmap_ports(
                      tag=tag, port=port_name, arg=arg_name)) & set(
                          self._tasks[task_name].module.ports):
                    async_mmap_args.setdefault(arg_name, []).append(tag)

              # add m_axi ports to the upper task
              if arg.cat == Instance.Arg.Cat.MMAP or (
                  arg.cat == Instance.Arg.Cat.ASYNC_MMAP and
                  task.name == self.top):
                task.module.add_m_axi(name=arg_name,
                                      data_width=width_table[arg_name])

              # declare wires or forward async_mmap ports
              for tag in async_mmap_args.get(arg_name, []):
                if task.name == self.top:
                  task.module.add_signals(
                      self.rtl.generate_async_mmap_signals(
                          tag=tag,
                          arg=arg_name,
                          data_width=width_table[arg_name]))
                else:
                  task.module.add_ports(
                      self.rtl.generate_async_mmap_ioports(
                          tag=tag,
                          arg=arg_name,
                          data_width=width_table[arg_name]))

            # set up done reg
            reset_if_branch.append(
                ast.NonblockingSubstitution(left=instance.handshake_done_reg,
                                            right=self.rtl.FALSE))
            reset_else_branch.append(
                ast.NonblockingSubstitution(
                    left=instance.handshake_done_reg,
                    right=ast.Lor(left=instance.handshake_done_reg,
                                  right=instance.handshake_done)))
            is_done_assignments.append(
                ast.Assign(left=instance.is_done,
                           right=ast.Lor(left=instance.handshake_done,
                                         right=instance.handshake_done_reg)))
            start_assignments.append(
                ast.Assign(left=instance.start,
                           right=ast.Land(left=self.rtl.START,
                                          right=ast.Ulnot(instance.is_done))))
            for signal, signals in handshake_output_signals.items():
              signals.append(instance.get_signal(signal))

            # insert handshake signals
            task.module.add_signals(instance.handshake_signals)

            # add task module instances
            portargs = list(self.rtl.generate_handshake_ports(instance.name))
            for arg_name, arg in sorted(
                (item for item in instance.args.items()), key=lambda x: x[0]):
              if arg.cat == Instance.Arg.Cat.SCALAR:
                portargs.append(
                    ast.PortArg(portname=arg.port,
                                argname=ast.Identifier(name=arg_name)))
              elif arg.cat == Instance.Arg.Cat.ISTREAM:
                portargs.extend(
                    self.rtl.generate_istream_ports(port=arg.port,
                                                    arg=arg_name))
                portargs.extend(
                    portarg for portarg in tlp.util.generate_peek_ports(
                        self.rtl, port=arg.port, arg=arg_name)
                    if portarg.portname in child_port_set)

              elif arg.cat == Instance.Arg.Cat.OSTREAM:
                portargs.extend(
                    self.rtl.generate_ostream_ports(port=arg.port,
                                                    arg=arg_name))
              elif arg.cat == Instance.Arg.Cat.MMAP:
                portargs.extend(
                    self.rtl.generate_m_axi_ports(port=arg.port, arg=arg_name))
              elif arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
                for tag in async_mmap_args[arg_name]:
                  portargs.extend(
                      self.rtl.generate_async_mmap_ports(tag=tag,
                                                         port=port_name,
                                                         arg=arg_name))

            task.module.add_instance(module_name=child.name,
                                     instance_name=instance.name,
                                     ports=portargs)

        # instantiate async_mmap module in the top module
        if task.name == self.top:
          for arg_name in async_mmap_args:
            portargs = []
            task.module.add_async_mmap_instance(
                name=arg_name,
                tags=async_mmap_args[arg_name],
                data_width=width_table[arg_name])

        task.module.add_logics(
            (ast.Always(sens_list=self.rtl.CLK_SENS_LIST,
                        statement=ast.Block((ast.IfStatement(
                            cond=self.rtl.RESET_COND,
                            true_statement=ast.Block(reset_if_branch),
                            false_statement=ast.Block(reset_else_branch)),))),
             *is_done_assignments, *start_assignments))
        for signal, signals in handshake_output_signals.items():
          task.module.replace_assign(signal=signal,
                                     content=self.rtl.make_operation(
                                         ast.Land, signals))
        with open(self.get_rtl(task.name), 'w') as rtl_code:
          rtl_code.write(task.module.code)
      for name, content in self.rtl.OTHER_MODULES.items():
        with open(self.get_rtl(name), 'w') as rtl_code:
          rtl_code.write(content)
      for file_name in 'async_mmap.v', 'detect_burst.v', 'generate_last.v':
        shutil.copy(
            os.path.join(os.path.dirname(tlp.__file__), 'assets', 'verilog',
                         file_name), self.rtl_dir)
    return self

  def pack_rtl(self, output_file: BinaryIO) -> 'Program':
    self.rtl.pack(top_name=self.top,
                  ports=self.ports,
                  rtl_dir=self.rtl_dir,
                  output_file=output_file)
    return self


class Instance:
  """Instance of a child Task in an upper-level task.

  A task can be instantiated multiple times in the same upper-level task.
  Each object of this class corresponds to such a instance of a task.

  Attributes:
    task: Task, corresponding task of this instance.
    instance_id: int, index of the instance of the same task.
    step: int, bulk-synchronous step when instantiated.
    args: a dict mapping arg names to Arg.

  Properties:
    name: str, instance name, unique in the parent module.

  """

  class Arg:

    class Cat(enum.Enum):
      INPUT = 1 << 0
      OUTPUT = 1 << 1
      SCALAR = 1 << 2
      STREAM = 1 << 3
      MMAP = 1 << 4
      ASYNC = 1 << 5
      ASYNC_MMAP = MMAP | ASYNC
      ISTREAM = STREAM | INPUT
      OSTREAM = STREAM | OUTPUT

    def __init__(self, cat: Union[str, Cat], port: str):
      if isinstance(cat, str):
        self.cat = {
            'istream': Instance.Arg.Cat.ISTREAM,
            'ostream': Instance.Arg.Cat.OSTREAM,
            'scalar': Instance.Arg.Cat.SCALAR,
            'mmap': Instance.Arg.Cat.MMAP,
            'async_mmap': Instance.Arg.Cat.ASYNC_MMAP
        }[cat]
      else:
        self.cat = cat
      self.port = port
      self.width = None

  def __init__(self, task: Task, verilog, instance_id: int, **kwargs):
    self.verilog = verilog
    self.task = task
    self.instance_id = instance_id
    self.step = kwargs.pop('step')
    self.args = {k: Instance.Arg(**v) for k, v in kwargs.pop('args').items()
                }  # type: Dict[str, Instance.Arg]

  @property
  def name(self) -> str:
    return '{0.task.name}_{0.instance_id}'.format(self)

  @property
  def start(self) -> ast.Identifier:
    return ast.Identifier(self.name + '_' + self.verilog.HANDSHAKE_START)

  @property
  def handshake_done(self) -> ast.Identifier:
    """The handshake done signal.

    This signal is asserted for 1 cycle when the instance of task finishes.

    Returns:
      The ast.Identifier node of this signal.
    """
    return ast.Identifier(self.name + '_' +
                          self.verilog.HANDSHAKE_OUTPUT_PORTS[0])

  @property
  def handshake_done_reg(self) -> ast.Identifier:
    """Registered handshake done signal.

    This signal is asserted 1 cycle after the instance of task finishes and is
    kept until reset.

    Returns:
      The ast.Identifier node of this signal.
    """
    return ast.Identifier(self.handshake_done.name + '_reg')

  @property
  def is_done(self) -> ast.Identifier:
    """Whether this instance has done.

    This signal is asserted as soon as the instance finishes and is kept until
    reset.

    Returns:
      The ast.Identifier node of this signal.
    """
    return ast.Identifier(self.name + '_is_done')

  @property
  def is_idle(self) -> ast.Identifier:
    """Whether this isntance is idle."""
    return ast.Identifier(self.name + '_' +
                          self.verilog.HANDSHAKE_OUTPUT_PORTS[1])

  @property
  def is_ready(self) -> ast.Identifier:
    """Whether this isntance is ready to take new input."""
    return ast.Identifier(self.name + '_' +
                          self.verilog.HANDSHAKE_OUTPUT_PORTS[2])

  def get_signal(self, signal: str) -> ast.Identifier:
    if signal not in {'done', 'idle', 'ready'}:
      raise ValueError(
          'signal should be one of (done, idle, ready), got {}'.format(signal))
    return getattr(self, 'is_' + signal)

  @property
  def handshake_signals(self) -> Iterator[Union[ast.Wire, ast.Reg]]:
    """All handshake signals used for this instance.

    Yields:
      Union[ast.Wire, ast.Reg] of signals.
    """
    yield ast.Wire(name=self.start.name, width=None)
    yield from (ast.Wire(name=self.name + '_' + suffix, width=None)
                for suffix in self.verilog.HANDSHAKE_OUTPUT_PORTS)
    yield ast.Wire(name=self.is_done.name, width=None)
    yield ast.Reg(name=self.handshake_done_reg.name, width=None)
