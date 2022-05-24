import collections
import decimal
import itertools
import json
import logging
import os
import os.path
import re
import shutil
import sys
import tarfile
import tempfile
import xml.etree.ElementTree as ET
from concurrent import futures
from typing import BinaryIO, Dict, List, Optional, TextIO, Tuple, Union

import toposort
import yaml
from haoda.backend import xilinx as hls

from tapa import util
from tapa.codegen.axi_pipeline import get_axi_pipeline_wrapper
from tapa.codegen.duplicate_s_axi_control import duplicate_s_axi_ctrl
from tapa.floorplan import (
    checkpoint_floorplan,
    generate_floorplan,
    generate_new_connectivity_ini,
    get_floorplan_result,
    get_post_synth_area,
)
from tapa.hardware import (
    DEFAULT_REGISTER_LEVEL,
    get_slr_count,
    is_part_num_supported,
)
from tapa.safety_check import check_mmap_arg_name
from tapa.task import Task
from tapa.verilog import ast
from tapa.verilog import xilinx as rtl

# TODO: resolve cyclic dependency
from .instance import Instance, Port

_logger = logging.getLogger().getChild(__name__)

STATE00 = ast.IntConst("2'b00")
STATE01 = ast.IntConst("2'b01")
STATE11 = ast.IntConst("2'b11")
STATE10 = ast.IntConst("2'b10")


class InputError(Exception):
  pass


class Program:
  """Describes a TAPA program.

  Attributes:
    top: Name of the top-level module.
    work_dir: Working directory.
    is_temp: Whether to delete the working directory after done.
    toplevel_ports: Tuple of Port objects.
    _tasks: Dict mapping names of tasks to Task objects.
    frt_interface: Optional string of FRT interface code.
    files: Dict mapping file names to contents that appear in the HDL directory.
  """

  def __init__(self, obj: Dict, work_dir: Optional[str] = None):
    """Construct Program object from a json file.

    Args:
      obj: json object.
      work_dir: Specifiy a working directory as a string. If None, a temporary
          one will be created.
    """
    self.top: str = obj['top']
    self.cflags = ' '.join(obj.get('cflags', []))
    self.headers: Dict[str, str] = obj.get('headers', {})
    if work_dir is None:
      self.work_dir = tempfile.mkdtemp(prefix='tapa-')
      self.is_temp = True
    else:
      self.work_dir = os.path.abspath(work_dir)
      os.makedirs(self.work_dir, exist_ok=True)
      self.is_temp = False
    self.toplevel_ports = tuple(map(Port, obj['tasks'][self.top]['ports']))
    self._tasks: Dict[str, Task] = collections.OrderedDict()
    for name in toposort.toposort_flatten(
        {k: set(v.get('tasks', ())) for k, v in obj['tasks'].items()}):
      task = Task(name=name, **obj['tasks'][name])
      if not task.is_upper or task.tasks:
        self._tasks[name] = task
    self.frt_interface = obj['tasks'][self.top].get('frt_interface')
    self.files: Dict[str, str] = {}
    self._hls_report_xmls: Dict[str, ET.ElementTree] = {}

  def __del__(self):
    if self.is_temp:
      shutil.rmtree(self.work_dir)

  @property
  def tasks(self) -> Tuple[Task, ...]:
    return tuple(self._tasks.values())

  @property
  def top_task(self) -> Task:
    return self._tasks[self.top]

  @property
  def ctrl_instance_name(self) -> str:
    return rtl.ctrl_instance_name(self.top)

  @property
  def register_level(self) -> int:
    return self.top_task.module.register_level

  @property
  def start_q(self) -> rtl.Pipeline:
    return rtl.Pipeline(rtl.START.name, level=self.register_level)

  @property
  def done_q(self) -> rtl.Pipeline:
    return rtl.Pipeline(rtl.DONE.name, level=self.register_level)

  @property
  def rtl_dir(self) -> str:
    return os.path.join(self.work_dir, 'hdl')

  @property
  def autobridge_dir(self) -> str:
    return os.path.join(self.work_dir, 'autobridge')

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

  def get_rtl(self, name: str, prefix: bool = True) -> str:
    return os.path.join(self.rtl_dir,
                        (util.get_module_name(name) if prefix else name) +
                        rtl.RTL_SUFFIX)

  def get_post_syn_rpt(self, module_name: str) -> str:
    return f'{self.work_dir}/report/{module_name}.hier.util.rpt'

  def _get_hls_report_xml(self, name: str) -> ET.ElementTree:
    tree = self._hls_report_xmls.get(name)
    if tree is None:
      filename = os.path.join(self.work_dir, 'report', f'{name}_csynth.xml')
      self._hls_report_xmls[name] = tree = ET.parse(filename)
    return tree

  def get_area(self, name: str) -> Dict[str, int]:
    node = self._get_hls_report_xml(name).find('./AreaEstimates/Resources')
    return {x.tag: int(x.text) for x in sorted(node, key=lambda x: x.tag)}

  def get_clock_period(self, name: str) -> decimal.Decimal:
    return decimal.Decimal(
        self._get_hls_report_xml(name).find('./PerformanceEstimates'
                                            '/SummaryOfTimingAnalysis'
                                            '/EstimatedClockPeriod').text)

  def extract_cpp(self) -> 'Program':
    """Extract HLS C++ files."""
    _logger.info('extracting HLS C++ files')
    check_mmap_arg_name(self._tasks.values())

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
              part_num: str, other_configs: str = '') -> 'Program':
    """Run HLS with extracted HLS C++ files and generate tarballs."""
    self.extract_cpp()

    _logger.info('running HLS')
    def worker(task: Task, idx: int) -> None:
      os.nice(idx % 19)
      hls_cflags = self.cflags + ' -DTAPA_TARGET_=XILINX_HLS'
      with open(self.get_tar(task.name), 'wb') as tarfileobj:
        with hls.RunHls(
            tarfileobj,
            kernel_files=[(self.get_cpp(task.name), hls_cflags)],
            top_name=task.name,
            clock_period=clock_period,
            part_num=part_num,
            auto_prefix=True,
            hls='vitis_hls',
            std='c++17',
            other_configs=other_configs,
        ) as proc:
          stdout, stderr = proc.communicate()
      if proc.returncode != 0:
        if b'Pre-synthesis failed.' in stdout and b'\nERROR:' not in stdout:
          _logger.error(
              'HLS failed for %s, but the failure may be flaky; retrying',
              task.name,
          )
          worker(task, 0)
          return
        sys.stdout.write(stdout.decode('utf-8'))
        sys.stderr.write(stderr.decode('utf-8'))
        raise RuntimeError('HLS failed for {}'.format(task.name))

    worker_num = util.nproc()
    _logger.info('spawn %d workers for parallel HLS synthesis of the tasks', worker_num)
    with futures.ThreadPoolExecutor(max_workers=worker_num) as executor:
      any(executor.map(worker, self._tasks.values(), itertools.count(0)))

    return self

  def generate_task_rtl(
    self,
    additional_fifo_pipelining: bool = False,
    part_num: str = '',
  ) -> 'Program':
    """Extract HDL files from tarballs generated from HLS."""
    _logger.info('extracting RTL files')
    for task in self._tasks.values():
      with tarfile.open(self.get_tar(task.name), 'r') as tarfileobj:
        tarfileobj.extractall(path=self.work_dir)

    for file_name in (
        'async_mmap.v',
        'axi_pipeline.v',
        'detect_burst.v',
        'fifo.v',
        'fifo_bram.v',
        'fifo_fwd.v',
        'fifo_srl.v',
        'generate_last.v',
        'relay_station.v',
        'a_axi_write_broadcastor_1_to_3.v',
        'a_axi_write_broadcastor_1_to_4.v',
    ):
      shutil.copy(
          os.path.join(os.path.dirname(util.__file__), 'assets', 'verilog',
                       file_name), self.rtl_dir)

    # extract and parse RTL and populate tasks
    _logger.info('parsing RTL files and populating tasks')
    for task, module in zip(
        self._tasks.values(),
        futures.ProcessPoolExecutor().map(
            rtl.Module,
            ([self.get_rtl(x.name)] for x in self._tasks.values()),
            (not x.is_upper for x in self._tasks.values()),
        ),
    ):
      _logger.debug('parsing %s', task.name)
      task.module = module
      task.self_area = self.get_area(task.name)
      task.clock_period = self.get_clock_period(task.name)
      _logger.debug('populating %s', task.name)
      self._populate_task(task)

    # instrument the upper-level RTL except the top-level
    _logger.info('instrumenting upper-level RTL')
    for task in self._tasks.values():
      if task.is_upper and task.name != self.top:
        self._instrument_non_top_upper_task(task, part_num, additional_fifo_pipelining)

    return self

  def generate_post_synth_task_area(self, part_num: str, max_parallel_synth_jobs: int = 8):
    get_post_synth_area(
      self.rtl_dir,
      part_num,
      self.top_task,
      self.get_post_syn_rpt,
      self.get_task,
      self.get_cpp,
      max_parallel_synth_jobs,
    )

  def run_floorplanning(
      self,
      part_num,
      connectivity: TextIO,
      floorplan_pre_assignments: TextIO = None,
      read_only_args: List[str] = [],
      write_only_args: List[str] = [],
      **kwargs,
  ) -> 'Program':
    _logger.info('Running floorplanning')

    os.makedirs(self.autobridge_dir, exist_ok=True)

    # generate partitioning constraints if partitioning directive is given
    config, config_with_floorplan = generate_floorplan(
      part_num,
      connectivity,
      read_only_args,
      write_only_args,
      user_floorplan_pre_assignments=floorplan_pre_assignments,
      autobridge_dir=self.autobridge_dir,
      top_task=self.top_task,
      fifo_width_getter=self._get_fifo_width,
      **kwargs,
    )

    open(f'{self.autobridge_dir}/pre-floorplan-config.json', 'w').write(json.dumps(config, indent=2))
    open(f'{self.autobridge_dir}/post-floorplan-config.json', 'w').write(json.dumps(config_with_floorplan, indent=2))
    checkpoint_floorplan(config_with_floorplan, self.autobridge_dir)
    generate_new_connectivity_ini(config_with_floorplan, self.work_dir, self.top)

    return self

  def generate_top_rtl(
      self,
      constraint: TextIO,
      register_level: int,
      additional_fifo_pipelining: bool,
      part_num: str,
  ) -> 'Program':
    """Instrument HDL files generated from HLS.

    Args:
        constraint: where to save the floorplan constraints
        register_level: Non-zero value overrides self.register_level.
        part_num: optinally provide the part_num to enable board-specific optimization
        additional_fifo_pipelining: replace every FIFO by a relay_station of LEVEL 2

    Returns:
        Program: Return self.
    """
    task_inst_to_slr = {}

    # extract the floorplan result
    if constraint:
      fifo_pipeline_level, axi_pipeline_level, task_inst_to_slr, fifo_to_depth = get_floorplan_result(
        self.autobridge_dir, constraint
      )

      if not task_inst_to_slr:
        _logger.warning('generate top rtl without floorplanning')

      self.top_task.module.fifo_partition_count = fifo_pipeline_level
      self.top_task.module.axi_pipeline_level = axi_pipeline_level

      # adjust fifo depth for rebalancing
      for fifo_name, depth in fifo_to_depth.items():
        if fifo_name in self.top_task.fifos:
          self.top_task.fifos[fifo_name]['depth'] = depth
        else:
          # streams has different naming convention
          # change fifo_name_0 to fifo_name[0]
          streams_fifo_name = re.sub(r'_(\d+)$', r'[\1]', fifo_name)
          if streams_fifo_name in self.top_task.fifos:
            self.top_task.fifos[streams_fifo_name]['depth'] = depth
          else:
            _logger.critical('unrecognized FIFO %s, skip depth adjustment', fifo_name)

    if register_level:
      assert register_level > 0
      self.top_task.module.register_level = register_level
    else:
      if is_part_num_supported(part_num):
        self.top_task.module.register_level = get_slr_count(part_num)
      else:
        _logger.info('the part-num is not included in the hardware library, '
                     'using the default register level %d.', DEFAULT_REGISTER_LEVEL)
        self.top_task.module.register_level = DEFAULT_REGISTER_LEVEL

    _logger.info('top task register level set to %d',
                self.top_task.module.register_level)

    # instrument the top-level RTL
    _logger.info('instrumenting top-level RTL')
    self._instrument_top_task(self.top_task, part_num, task_inst_to_slr, additional_fifo_pipelining)

    _logger.info('generating report')
    task_report = self.top_task.report
    with open(os.path.join(self.work_dir, 'report.yaml'), 'w') as fp:
      yaml.dump(task_report, fp, default_flow_style=False, sort_keys=False)
    with open(os.path.join(self.work_dir, 'report.json'), 'w') as fp:
      json.dump(task_report, fp, indent=2)

    # self.files won't be populated until all tasks are instrumented
    _logger.info('writing generated auxiliary RTL files')
    for name, content in self.files.items():
      with open(os.path.join(self.rtl_dir, name), 'w') as fp:
        fp.write(content)

    return self

  def pack_rtl(self, output_file: BinaryIO) -> 'Program':
    _logger.info('packaging RTL code')
    rtl.pack(top_name=self.top,
             ports=self.toplevel_ports,
             rtl_dir=self.rtl_dir,
             output_file=output_file)
    return self

  def _populate_task(self, task: Task) -> None:
    task.instances = tuple(
        Instance(self.get_task(name), verilog=rtl, instance_id=idx, **obj)
        for name, objs in task.tasks.items()
        for idx, obj in enumerate(objs))

  def _connect_fifos(self, task: Task) -> None:
    _logger.debug("  connecting %s's children tasks", task.name)
    for fifo_name in task.fifos:
      for direction in task.get_fifo_directions(fifo_name):
        task_name, _, fifo_port = task.get_connection_to(fifo_name, direction)

        for suffix in task.get_fifo_suffixes(direction):
          # declare wires for FIFOs
          wire_name = rtl.wire_name(fifo_name, suffix)
          wire_width = self.get_task(task_name).module.get_port_of(
              fifo_port, suffix).width
          wire = ast.Wire(name=wire_name, width=wire_width)
          task.module.add_signals([wire])

      if task.is_fifo_external(fifo_name):
        task.connect_fifo_externally(fifo_name, task.name == self.top)

  def _instantiate_fifos(self, task: Task, additional_fifo_pipelining: bool) -> None:
    _logger.debug('  instantiating FIFOs in %s', task.name)

    # skip instantiating if the fifo is not declared in this task
    fifos = {name: fifo for name, fifo in task.fifos.items() if 'depth' in fifo}
    if not fifos:
      return

    col_width = max(
        max(len(name), len(util.get_instance_name(fifo['consumed_by'])),
            len(util.get_instance_name(fifo['produced_by'])))
        for name, fifo in fifos.items())

    for fifo_name, fifo in fifos.items():
      _logger.debug('    instantiating %s.%s', task.name, fifo_name)

      # add FIFO instances
      task.module.add_fifo_instance(
          name=fifo_name,
          width=self._get_fifo_width(task, fifo_name),
          depth=fifo['depth'],
          additional_fifo_pipelining=additional_fifo_pipelining,
      )

      # print debugging info
      debugging_blocks = []
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
                  ast.Identifier(name=rtl.wire_name(fifo_name, suffixes[0])))))
        debugging_blocks.append(
            ast.Always(
                sens_list=rtl.CLK_SENS_LIST,
                statement=ast.make_block(
                    ast.IfStatement(cond=ast.Eq(
                        left=ast.Identifier(
                            name=rtl.wire_name(fifo_name, suffixes[-1])),
                        right=rtl.TRUE,
                    ),
                                    true_statement=ast.make_block(display),
                                    false_statement=None))))
      task.module.add_logics(debugging_blocks)

  def _instantiate_children_tasks(
      self,
      task: Task,
      width_table: Dict[str, int],
      part_num: str,
      instance_name_to_slr: Dict[str, int],
  ) -> List[ast.Identifier]:
    is_done_signals: List[rtl.Pipeline] = []
    arg_table: Dict[str, rtl.Pipeline] = {}
    async_mmap_args: Dict[Instance.Arg, List[str]] = collections.OrderedDict()

    task.add_m_axi(width_table, self.files)

    # now that each SLR has an control_s_axi, slightly reduce the
    # pipeline level of the scalars
    if instance_name_to_slr:
      scalar_register_level = 2
    else:
      scalar_register_level = self.register_level

    for instance in task.instances:
      # connect to the control_s_axi in the corresponding SLR
      if instance.name in instance_name_to_slr:
        argname_suffix = f'_slr_{instance_name_to_slr[instance.name]}'
      else:
        argname_suffix = ''

      child_port_set = set(instance.task.module.ports)

      # add signal delcarations
      for arg in instance.args:
        if arg.cat not in {
            Instance.Arg.Cat.ISTREAM,
            Instance.Arg.Cat.OSTREAM,
        }:
          width = 64  # 64-bit address
          if arg.cat == Instance.Arg.Cat.SCALAR:
            width = width_table.get(arg.name, 0)
            if width == 0:
              width = int(arg.name.split("'d")[0])
          q = rtl.Pipeline(
              name=instance.get_instance_arg(arg.name),
              level=scalar_register_level,
              width=width,
          )
          arg_table[arg.name] = q

          # arg.name may be a constant
          if arg.name in width_table:
            id_name = arg.name + argname_suffix
          else:
            id_name = arg.name
          task.module.add_pipeline(q, init=ast.Identifier(id_name))

        # arg.name is the upper-level name
        # arg.port is the lower-level name

        # check which ports are used for async_mmap
        if arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
          for tag in rtl.ASYNC_MMAP_SUFFIXES:
            if set(x.portname for x in rtl.generate_async_mmap_ports(
                tag=tag,
                port=arg.port,
                arg=arg.name,
                instance=instance,
            )) & child_port_set:
              async_mmap_args.setdefault(arg, []).append(tag)

        # declare wires or forward async_mmap ports
        for tag in async_mmap_args.get(arg, []):
          if task.is_upper and instance.task.is_lower:
            task.module.add_signals(
                rtl.generate_async_mmap_signals(
                    tag=tag,
                    arg=arg.mmap_name,
                    data_width=width_table[arg.name],
                ))
          else:
            task.module.add_ports(
                rtl.generate_async_mmap_ioports(
                    tag=tag,
                    arg=arg.name,
                    data_width=width_table[arg.name],
                ))

      # add reset registers
      rst_q = rtl.Pipeline(instance.rst_n, level=self.register_level)
      task.module.add_pipeline(rst_q, init=rtl.RST_N)

      # add start registers
      start_q = rtl.Pipeline(
          f'{instance.start.name}_global',
          level=self.register_level,
      )
      task.module.add_pipeline(start_q, self.start_q[0])

      if instance.is_autorun:
        # autorun modules start when the global start signal is asserted
        task.module.add_logics([
            ast.Always(
                sens_list=rtl.CLK_SENS_LIST,
                statement=ast.make_block(
                    ast.make_if_with_block(
                        cond=ast.Unot(rst_q[-1]),
                        true=ast.NonblockingSubstitution(
                            left=instance.start,
                            right=rtl.FALSE,
                        ),
                        false=ast.make_if_with_block(
                            cond=start_q[-1],
                            true=ast.NonblockingSubstitution(
                                left=instance.start,
                                right=rtl.TRUE,
                            ),
                        ),
                    )),
            ),
        ])
      else:
        # set up state
        is_done_q = rtl.Pipeline(
            f'{instance.is_done.name}',
            level=self.register_level,
        )
        done_q = rtl.Pipeline(
            f'{instance.done.name}_global',
            level=self.register_level,
        )
        task.module.add_pipeline(is_done_q, instance.is_state(STATE10))
        task.module.add_pipeline(done_q, self.done_q[0])

        if_branch = (instance.set_state(STATE00))
        else_branch = ((
            ast.make_if_with_block(
                cond=instance.is_state(STATE00),
                true=ast.make_if_with_block(
                    cond=start_q[-1],
                    true=instance.set_state(STATE01),
                ),
            ),
            ast.make_if_with_block(
                cond=instance.is_state(STATE01),
                true=ast.make_if_with_block(
                    cond=instance.ready,
                    true=ast.make_if_with_block(
                        cond=instance.done,
                        true=instance.set_state(STATE10),
                        false=instance.set_state(STATE11),
                    )),
            ),
            ast.make_if_with_block(
                cond=instance.is_state(STATE11),
                true=ast.make_if_with_block(
                    cond=instance.done,
                    true=instance.set_state(STATE10),
                ),
            ),
            ast.make_if_with_block(
                cond=instance.is_state(STATE10),
                true=ast.make_if_with_block(
                    cond=done_q[-1],
                    true=instance.set_state(STATE00),
                ),
            ),
        ))
        task.module.add_logics([
            ast.Always(
                sens_list=rtl.CLK_SENS_LIST,
                statement=ast.make_block(
                    ast.make_if_with_block(
                        cond=ast.Unot(rst_q[-1]),
                        true=if_branch,
                        false=else_branch,
                    )),
            ),
            ast.Assign(
                left=instance.start,
                right=instance.is_state(STATE01),
            ),
        ])

        is_done_signals.append(is_done_q)

      # insert handshake signals
      task.module.add_signals(instance.handshake_signals)

      # add task module instances
      portargs = list(rtl.generate_handshake_ports(instance, rst_q))
      for arg in instance.args:
        if arg.cat == Instance.Arg.Cat.SCALAR:
          portargs.append(
              ast.PortArg(portname=arg.port, argname=arg_table[arg.name][-1]))
        elif arg.cat == Instance.Arg.Cat.ISTREAM:
          portargs.extend(
              instance.task.module.generate_istream_ports(
                  port=arg.port,
                  arg=arg.name,
              ))
        elif arg.cat == Instance.Arg.Cat.OSTREAM:
          portargs.extend(
              instance.task.module.generate_ostream_ports(
                  port=arg.port,
                  arg=arg.name,
              ))
        elif arg.cat == Instance.Arg.Cat.MMAP:
          portargs.extend(
              rtl.generate_m_axi_ports(
                  module=instance.task.module,
                  port=arg.port,
                  arg=arg.mmap_name,
                  arg_reg=arg_table[arg.name][-1].name,
              ))
        elif arg.cat == Instance.Arg.Cat.ASYNC_MMAP:
          for tag in async_mmap_args[arg]:
            portargs.extend(
                rtl.generate_async_mmap_ports(
                    tag=tag,
                    port=arg.port,
                    arg=arg.mmap_name,
                    instance=instance,
                ))

      task.module.add_instance(
          module_name=util.get_module_name(instance.task.name),
          instance_name=instance.name,
          ports=portargs,
      )

    # instantiate async_mmap modules at the upper levels
    addr_width = util.get_max_addr_width(part_num)
    _logger.debug('Set the address width of async_mmap to %d', addr_width)

    if task.is_upper:
      for arg in async_mmap_args:
        task.module.add_async_mmap_instance(
            name=arg.mmap_name,
            offset_name=arg_table[arg.name][-1],
            tags=async_mmap_args[arg],
            data_width=width_table[arg.name],
            addr_width=addr_width,
        )

    return is_done_signals

  def _instantiate_global_fsm(
      self,
      task: Task,
      is_done_signals: List[rtl.Pipeline],
  ) -> None:
    # global state machine

    def is_state(state: ast.IntConst) -> ast.Eq:
      return ast.Eq(left=rtl.STATE, right=state)

    def set_state(state: ast.IntConst) -> ast.NonblockingSubstitution:
      return ast.NonblockingSubstitution(left=rtl.STATE, right=state)

    countdown = ast.Identifier('countdown')
    countdown_width = (self.register_level - 1).bit_length()

    task.module.add_signals([
        ast.Reg(rtl.STATE.name, width=ast.make_width(2)),
        ast.Reg(countdown.name, width=ast.make_width(countdown_width)),
    ])

    state01_action = set_state(STATE10)
    if is_done_signals:
      state01_action = ast.make_if_with_block(
          cond=ast.make_operation(
              operator=ast.Land,
              nodes=(x[-1] for x in reversed(is_done_signals)),
          ),
          true=state01_action,
      )

    global_fsm = ast.make_case_with_block(
        comp=rtl.STATE,
        cases=[
            (
                STATE00,
                ast.make_if_with_block(
                    cond=self.start_q[-1],
                    true=set_state(STATE01),
                ),
            ),
            (
                STATE01,
                state01_action,
            ),
            (
                STATE10,
                [
                    set_state(STATE11 if self.register_level else STATE00),
                    ast.NonblockingSubstitution(
                        left=countdown,
                        right=ast.make_int(max(0, self.register_level - 1)),
                    ),
                ],
            ),
            (
                STATE11,
                ast.make_if_with_block(
                    cond=ast.Eq(
                        left=countdown,
                        right=ast.make_int(0, width=countdown_width),
                    ),
                    true=set_state(STATE00),
                    false=ast.NonblockingSubstitution(
                        left=countdown,
                        right=ast.Minus(
                            left=countdown,
                            right=ast.make_int(1, width=countdown_width),
                        ),
                    ),
                ),
            ),
        ],
    )

    task.module.add_logics([
        ast.Always(
            sens_list=rtl.CLK_SENS_LIST,
            statement=ast.make_block(
                ast.make_if_with_block(
                    cond=rtl.RST,
                    true=set_state(STATE00),
                    false=global_fsm,
                )),
        ),
        ast.Assign(left=rtl.IDLE, right=is_state(STATE00)),
        ast.Assign(left=rtl.DONE, right=self.done_q[-1]),
        ast.Assign(left=rtl.READY, right=self.done_q[0]),
    ])

    task.module.add_pipeline(self.start_q, init=rtl.START)
    task.module.add_pipeline(self.done_q, init=is_state(STATE10))

  def _instrument_non_top_upper_task(
      self,
      task: Task,
      part_num: str,
      additional_fifo_pipelining: bool = False,
  ) -> None:
    """ codegen for upper but non-top tasks """
    assert task.is_upper
    task.module.cleanup()

    self._instantiate_fifos(task, additional_fifo_pipelining)
    self._connect_fifos(task)
    width_table = {port.name: port.width for port in task.ports.values()}
    is_done_signals = self._instantiate_children_tasks(task, width_table, part_num, {})
    self._instantiate_global_fsm(task, is_done_signals)

    with open(self.get_rtl(task.name), 'w') as rtl_code:
      rtl_code.write(task.module.code)

  def _instrument_top_task(
      self,
      task: Task,
      part_num: str,
      instance_name_to_slr: Dict[str, int],
      additional_fifo_pipelining: bool = False,
  ) -> None:
    """ codegen for the top task """
    assert task.is_upper
    task.module.cleanup()

    # if floorplan is enabled, add a control_s_axi instance in each SLR
    if instance_name_to_slr:
      num_slr = get_slr_count(part_num)
      duplicate_s_axi_ctrl(task, num_slr)

    self._instantiate_fifos(task, additional_fifo_pipelining)
    self._connect_fifos(task)
    width_table = {port.name: port.width for port in task.ports.values()}
    is_done_signals = self._instantiate_children_tasks(task, width_table, part_num, instance_name_to_slr)
    self._instantiate_global_fsm(task, is_done_signals)

    self._pipeline_top_task(task)

  def _pipeline_top_task(self, task: Task) -> None:
    """
    add axi pipelines to the top task
    """
    # generate the original top module. Append a suffix to it
    top_suffix = '_inner'
    task.module.name += top_suffix
    with open(self.get_rtl(task.name + top_suffix), 'w') as rtl_code:
      rtl_code.write(task.module.code)

    # generate the wrapper that becomes the final top module
    with open(self.get_rtl(task.name), 'w') as rtl_code:
      rtl_code.write(get_axi_pipeline_wrapper(task.name, top_suffix, task))

  def _get_fifo_width(self, task: Task, fifo: str) -> int:
    producer_task, _, fifo_port = task.get_connection_to(fifo, 'produced_by')
    port = self.get_task(producer_task).module.get_port_of(
        fifo_port, rtl.OSTREAM_SUFFIXES[0])
    # TODO: err properly if not integer literals
    return int(port.width.msb.value) - int(port.width.lsb.value) + 1
