"""Core logic of TAPA."""

from __future__ import annotations

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import collections
import decimal
import glob
import itertools
import json
import logging
import os
import os.path
import shutil
import sys
import tarfile
import tempfile
import zipfile
from concurrent import futures
from xml.etree import ElementTree

import toposort
import yaml
from pyverilog.vparser.ast import (
    Always,
    Assign,
    Eq,
    Identifier,
    IfStatement,
    Input,
    IntConst,
    Land,
    Minus,
    NonblockingSubstitution,
    Output,
    PortArg,
    Reg,
    SingleStatement,
    StringConst,
    SystemCall,
    Wire,
)

from tapa.backend.xilinx import RunHls
from tapa.instance import Instance, Port
from tapa.safety_check import check_mmap_arg_name
from tapa.task import Task
from tapa.util import (
    clang_format,
    get_instance_name,
    get_module_name,
    get_vendor_include_paths,
    nproc,
)
from tapa.verilog.ast_utils import (
    make_block,
    make_case_with_block,
    make_if_with_block,
    make_int,
    make_operation,
    make_port_arg,
    make_width,
)
from tapa.verilog.util import Pipeline, wire_name
from tapa.verilog.xilinx import ctrl_instance_name, generate_handshake_ports, pack
from tapa.verilog.xilinx.async_mmap import (
    ASYNC_MMAP_SUFFIXES,
    generate_async_mmap_ioports,
    generate_async_mmap_ports,
    generate_async_mmap_signals,
)
from tapa.verilog.xilinx.const import (
    CLK_SENS_LIST,
    DONE,
    FALSE,
    HANDSHAKE_CLK,
    HANDSHAKE_DONE,
    HANDSHAKE_IDLE,
    HANDSHAKE_INPUT_PORTS,
    HANDSHAKE_OUTPUT_PORTS,
    HANDSHAKE_READY,
    HANDSHAKE_RST_N,
    HANDSHAKE_START,
    IDLE,
    ISTREAM_SUFFIXES,
    OSTREAM_SUFFIXES,
    READY,
    RST,
    RST_N,
    RTL_SUFFIX,
    START,
    STATE,
    TRUE,
)
from tapa.verilog.xilinx.module import Module, generate_m_axi_ports

_logger = logging.getLogger().getChild(__name__)

STATE00 = IntConst("2'b00")
STATE01 = IntConst("2'b01")
STATE11 = IntConst("2'b11")
STATE10 = IntConst("2'b10")


class Program:  # noqa: PLR0904  # TODO: refactor this class
    """Describes a TAPA program.

    Attributes
    ----------
      top: Name of the top-level module.
      work_dir: Working directory.
      is_temp: Whether to delete the working directory after done.
      toplevel_ports: Tuple of Port objects.
      _tasks: Dict mapping names of tasks to Task objects.
      files: Dict mapping file names to contents that appear in the HDL directory.
      vitis_mode: Whether the generated RTL should match Vitis XO requirements.

    """

    def __init__(
        self, obj: dict, vitis_mode: bool, work_dir: str | None = None
    ) -> None:
        """Construct Program object from a json file.

        Args:
        ----
          obj: json object.
          work_dir: Specify a working directory as a string. If None, a temporary
              one will be created.

        """
        self.top: str = obj["top"]
        self.cflags = " ".join(obj.get("cflags", []))
        self.headers: dict[str, str] = obj.get("headers", {})
        self.vitis_mode = vitis_mode
        if work_dir is None:
            self.work_dir = tempfile.mkdtemp(prefix="tapa-")
            self.is_temp = True
        else:
            self.work_dir = os.path.abspath(work_dir)
            os.makedirs(self.work_dir, exist_ok=True)
            self.is_temp = False
        self.toplevel_ports = tuple(map(Port, obj["tasks"][self.top]["ports"]))
        self._tasks: dict[str, Task] = collections.OrderedDict()
        for name in toposort.toposort_flatten(
            {k: set(v.get("tasks", ())) for k, v in obj["tasks"].items()},
        ):
            task_properties = obj["tasks"][name]
            task = Task(
                name=name,
                code=task_properties["code"],
                level=task_properties["level"],
                tasks=task_properties.get("tasks", {}),
                fifos=task_properties.get("fifos", {}),
                ports=task_properties.get("ports", []),
            )
            if not task.is_upper or task.tasks:
                self._tasks[name] = task
        self.files: dict[str, str] = {}
        self._hls_report_xmls: dict[str, ElementTree.ElementTree] = {}

    def __del__(self) -> None:
        if self.is_temp:
            shutil.rmtree(self.work_dir)

    @property
    def tasks(self) -> tuple[Task, ...]:
        return tuple(self._tasks.values())

    @property
    def top_task(self) -> Task:
        return self._tasks[self.top]

    @property
    def ctrl_instance_name(self) -> str:
        return ctrl_instance_name(self.top)

    @property
    def register_level(self) -> int:
        return self.top_task.module.register_level

    @property
    def start_q(self) -> Pipeline:
        return Pipeline(START.name, level=self.register_level)

    @property
    def done_q(self) -> Pipeline:
        return Pipeline(DONE.name, level=self.register_level)

    @property
    def rtl_dir(self) -> str:
        return os.path.join(self.work_dir, "hdl")

    @property
    def report_dir(self) -> str:
        return os.path.join(self.work_dir, "report")

    @property
    def cpp_dir(self) -> str:
        cpp_dir = os.path.join(self.work_dir, "cpp")
        os.makedirs(cpp_dir, exist_ok=True)
        return cpp_dir

    def get_task(self, name: str) -> Task:
        return self._tasks[name]

    def get_cpp(self, name: str) -> str:
        return os.path.join(self.cpp_dir, name + ".cpp")

    def get_tar(self, name: str) -> str:
        os.makedirs(os.path.join(self.work_dir, "tar"), exist_ok=True)
        return os.path.join(self.work_dir, "tar", name + ".tar")

    def get_rtl(self, name: str, prefix: bool = True) -> str:
        return os.path.join(
            self.rtl_dir,
            (get_module_name(name) if prefix else name) + RTL_SUFFIX,
        )

    def _get_hls_report_xml(self, name: str) -> ElementTree.ElementTree:
        tree = self._hls_report_xmls.get(name)
        if tree is None:
            filename = os.path.join(self.report_dir, f"{name}_csynth.xml")
            self._hls_report_xmls[name] = tree = ElementTree.parse(filename)
        return tree

    def _get_part_num(self, name: str) -> str:
        xml = self._get_hls_report_xml(name)
        part = xml.find("./UserAssignments/Part")
        assert part is not None
        assert part.text
        return part.text

    def get_area(self, name: str) -> dict[str, int]:
        node = self._get_hls_report_xml(name).find("./AreaEstimates/Resources")
        assert node is not None
        return {x.tag: int(x.text or "-1") for x in sorted(node, key=lambda x: x.tag)}

    def get_clock_period(self, name: str) -> decimal.Decimal:
        xml = self._get_hls_report_xml(name)
        period = xml.find(
            "./PerformanceEstimates/SummaryOfTimingAnalysis/EstimatedClockPeriod",
        )
        assert period is not None
        assert period.text
        return decimal.Decimal(period.text)

    def extract_cpp(self) -> Program:
        """Extract HLS C++ files."""
        _logger.info("extracting HLS C++ files")
        check_mmap_arg_name(list(self._tasks.values()))

        for task in self._tasks.values():
            code_content = clang_format(task.code)
            try:
                with open(self.get_cpp(task.name), encoding="utf-8") as src_code:
                    if src_code.read() == code_content:
                        _logger.debug(
                            "not updating %s since its content is up-to-date",
                            src_code.name,
                        )
                        continue
            except FileNotFoundError:
                pass
            with open(self.get_cpp(task.name), "w", encoding="utf-8") as src_code:
                src_code.write(code_content)
        for name, content in self.headers.items():
            header_path = os.path.join(self.cpp_dir, name)
            os.makedirs(os.path.dirname(header_path), exist_ok=True)
            with open(header_path, "w", encoding="utf-8") as header_fp:
                header_fp.write(content)
        return self

    def run_hls(
        self,
        clock_period: float | str,
        part_num: str,
        skip_based_on_mtime: bool = False,
        other_configs: str = "",
    ) -> Program:
        """Run HLS with extracted HLS C++ files and generate tarballs."""
        self.extract_cpp()

        _logger.info("running HLS")

        def worker(task: Task, idx: int) -> None:
            os.nice(idx % 19)
            try:
                if skip_based_on_mtime and os.path.getmtime(
                    self.get_tar(task.name),
                ) > os.path.getmtime(self.get_cpp(task.name)):
                    _logger.info(
                        "skipping HLS for %s since %s is newer than %s",
                        task.name,
                        self.get_tar(task.name),
                        self.get_cpp(task.name),
                    )
                    return
            except OSError:
                pass
            hls_cflags = " ".join(
                (
                    self.cflags,
                    "-DTAPA_TARGET_DEVICE_",
                    "-DTAPA_TARGET_XILINX_HLS_",
                    *(f"-isystem{x}" for x in get_vendor_include_paths()),
                ),
            )
            with (
                open(self.get_tar(task.name), "wb") as tarfileobj,
                RunHls(
                    tarfileobj,
                    kernel_files=[(self.get_cpp(task.name), hls_cflags)],
                    top_name=task.name,
                    clock_period=str(clock_period),
                    part_num=part_num,
                    auto_prefix=True,
                    hls="vitis_hls",
                    std="c++17",
                    other_configs=other_configs,
                ) as proc,
            ):
                stdout, stderr = proc.communicate()
            if proc.returncode != 0:
                if b"Pre-synthesis failed." in stdout and b"\nERROR:" not in stdout:
                    _logger.error(
                        "HLS failed for %s, but the failure may be flaky; retrying",
                        task.name,
                    )
                    worker(task, 0)
                    return
                sys.stdout.write(stdout.decode("utf-8"))
                sys.stderr.write(stderr.decode("utf-8"))
                msg = f"HLS failed for {task.name}"
                raise RuntimeError(msg)

        worker_num = nproc()
        if worker_num_str := os.getenv("TAPA_CONCURRENCY", None):
            if int(worker_num_str) > 0:
                worker_num = int(worker_num_str)
            else:
                _logger.warning(
                    "TAPA_CONCURRENCY is set to %s, which is invalid; using %d",
                    worker_num_str,
                    worker_num,
                )

        _logger.info(
            "spawn %d workers for parallel HLS synthesis of the tasks", worker_num
        )
        _logger.info("set TAPA_CONCURRENCY to change the number of workers")
        with futures.ThreadPoolExecutor(max_workers=worker_num) as executor:
            any(executor.map(worker, self._tasks.values(), itertools.count(0)))

        return self

    def generate_task_rtl(self, print_fifo_ops: bool) -> Program:
        """Extract HDL files from tarballs generated from HLS."""
        _logger.info("extracting RTL files")
        for task in self._tasks.values():
            with tarfile.open(self.get_tar(task.name), "r") as tarfileobj:
                tarfileobj.extractall(path=self.work_dir)

        for file_name in (
            "arbiter.v",
            "async_mmap.v",
            "axi_pipeline.v",
            "axi_crossbar_addr.v",
            "axi_crossbar_rd.v",
            "axi_crossbar_wr.v",
            "axi_crossbar.v",
            "axi_register_rd.v",
            "axi_register_wr.v",
            "detect_burst.v",
            "fifo.v",
            "fifo_bram.v",
            "fifo_fwd.v",
            "fifo_srl.v",
            "generate_last.v",
            "priority_encoder.v",
            "relay_station.v",
            "a_axi_write_broadcastor_1_to_3.v",
            "a_axi_write_broadcastor_1_to_4.v",
        ):
            shutil.copy(
                os.path.join(os.path.dirname(__file__), "assets", "verilog", file_name),
                self.rtl_dir,
            )

        # extract and parse RTL and populate tasks
        _logger.info("parsing RTL files and populating tasks")
        for task, module in zip(
            self._tasks.values(),
            futures.ProcessPoolExecutor().map(
                Module,
                ([self.get_rtl(x.name)] for x in self._tasks.values()),
                (not x.is_upper for x in self._tasks.values()),
            ),
        ):
            _logger.debug("parsing %s", task.name)
            task.module = module
            task.self_area = self.get_area(task.name)
            task.clock_period = self.get_clock_period(task.name)
            _logger.debug("populating %s", task.name)
            self._populate_task(task)

        # instrument the upper-level RTL except the top-level
        _logger.info("instrumenting upper-level RTL")
        for task in self._tasks.values():
            if task.is_upper and task.name != self.top:
                self._instrument_upper_task(task, print_fifo_ops)

        return self

    def generate_top_rtl(self, print_fifo_ops: bool) -> Program:
        """Instrument HDL files generated from HLS.

        Args:
        ----
            print_fifo_ops: Whether to print debugging info for FIFO operations.

        Returns:
        -------
            Program: Return self.

        """
        # instrument the top-level RTL
        _logger.info("instrumenting top-level RTL")
        self._instrument_upper_task(
            self.top_task,
            print_fifo_ops,
        )

        _logger.info("generating report")
        task_report = self.top_task.report
        with open(
            os.path.join(self.work_dir, "report.yaml"),
            "w",
            encoding="utf-8",
        ) as fp:
            yaml.dump(task_report, fp, default_flow_style=False, sort_keys=False)
        with open(
            os.path.join(self.work_dir, "report.json"),
            "w",
            encoding="utf-8",
        ) as fp:
            json.dump(task_report, fp, indent=2)

        # self.files won't be populated until all tasks are instrumented
        _logger.info("writing generated auxiliary RTL files")
        for name, content in self.files.items():
            with open(os.path.join(self.rtl_dir, name), "w", encoding="utf-8") as fp:
                fp.write(content)

        return self

    def pack_rtl(self, output_file: str) -> Program:
        _logger.info("packaging RTL code")
        with open(output_file, "wb") as packed_obj:
            pack(
                top_name=self.top,
                ports=self.toplevel_ports,
                rtl_dir=self.rtl_dir,
                part_num=self._get_part_num(self.top),
                output_file=packed_obj,
            )

        _logger.info("packaging HLS report")
        with zipfile.ZipFile(output_file, "a", zipfile.ZIP_DEFLATED) as packed_obj:
            report_glob = os.path.join(glob.escape(self.report_dir), "*_csynth.xml")
            for filename in glob.iglob(report_glob):
                arcname = os.path.join("report", os.path.basename(filename))
                _logger.debug("  packing %s", arcname)
                packed_obj.write(filename, arcname)

        _logger.info("generated the v++ xo file at %s", output_file)
        return self

    def _populate_task(self, task: Task) -> None:
        task.instances = tuple(
            Instance(self.get_task(name), instance_id=idx, **obj)
            for name, objs in task.tasks.items()
            for idx, obj in enumerate(objs)
        )

    def _connect_fifos(self, task: Task) -> None:
        _logger.debug("  connecting %s's children tasks", task.name)
        for fifo_name in task.fifos:
            for direction in task.get_fifo_directions(fifo_name):
                task_name, _, fifo_port = task.get_connection_to(fifo_name, direction)

                for suffix in task.get_fifo_suffixes(direction):
                    # declare wires for FIFOs
                    w_name = wire_name(fifo_name, suffix)
                    wire_width = (
                        self.get_task(task_name)
                        .module.get_port_of(fifo_port, suffix)
                        .width
                    )
                    wire = Wire(name=w_name, width=wire_width)
                    task.module.add_signals([wire])

            if task.is_fifo_external(fifo_name):
                task.connect_fifo_externally(
                    fifo_name, task.name == self.top and self.vitis_mode
                )

    def _instantiate_fifos(self, task: Task, print_fifo_ops: bool) -> None:
        _logger.debug("  instantiating FIFOs in %s", task.name)

        # skip instantiating if the fifo is not declared in this task
        fifos = {name: fifo for name, fifo in task.fifos.items() if "depth" in fifo}
        if not fifos:
            return

        col_width = max(
            max(
                len(name),
                len(get_instance_name(fifo["consumed_by"])),
                len(get_instance_name(fifo["produced_by"])),
            )
            for name, fifo in fifos.items()
        )

        for fifo_name, fifo in fifos.items():
            _logger.debug("    instantiating %s.%s", task.name, fifo_name)

            # add FIFO instances
            task.module.add_fifo_instance(
                name=fifo_name,
                rst=RST,
                width=self._get_fifo_width(task, fifo_name),
                depth=fifo["depth"],
            )

            if not print_fifo_ops:
                continue

            # print debugging info
            debugging_blocks = []
            fmtargs = {
                "fifo_prefix": "\\033[97m",
                "fifo_suffix": "\\033[0m",
                "task_prefix": "\\033[90m",
                "task_suffix": "\\033[0m",
            }
            for suffixes, fmt, fifo_tag in zip(
                (ISTREAM_SUFFIXES, OSTREAM_SUFFIXES),
                (
                    "DEBUG: R: {fifo_prefix}{fifo:>{width}}{fifo_suffix} -> "
                    "{task_prefix}{task:<{width}}{task_suffix} %h",
                    "DEBUG: W: {task_prefix}{task:>{width}}{task_suffix} -> "
                    "{fifo_prefix}{fifo:<{width}}{fifo_suffix} %h",
                ),
                ("consumed_by", "produced_by"),
            ):
                display = SingleStatement(
                    statement=SystemCall(
                        syscall="display",
                        args=(
                            StringConst(
                                value=fmt.format(
                                    width=col_width,
                                    fifo=fifo_name,
                                    task=(get_instance_name(fifo[fifo_tag])),
                                    **fmtargs,
                                ),
                            ),
                            Identifier(name=wire_name(fifo_name, suffixes[0])),
                        ),
                    ),
                )
                debugging_blocks.append(
                    Always(
                        sens_list=CLK_SENS_LIST,
                        statement=make_block(
                            IfStatement(
                                cond=Eq(
                                    left=Identifier(
                                        name=wire_name(fifo_name, suffixes[-1]),
                                    ),
                                    right=TRUE,
                                ),
                                true_statement=make_block(display),
                                false_statement=None,
                            ),
                        ),
                    ),
                )
            task.module.add_logics(debugging_blocks)

    def _instantiate_children_tasks(  # noqa: C901,PLR0912,PLR0915,PLR0914  # TODO: refactor this method
        self,
        task: Task,
        width_table: dict[str, int],
    ) -> list[Pipeline]:
        _logger.debug("  instantiating children tasks in %s", task.name)
        is_done_signals: list[Pipeline] = []
        arg_table: dict[str, Pipeline] = {}
        async_mmap_args: dict[Instance.Arg, list[str]] = collections.OrderedDict()

        task.add_m_axi(width_table, self.files)

        # Wires connecting to the upstream (s_axi_control).
        fsm_upstream_portargs: list[PortArg] = [
            make_port_arg(x, x) for x in HANDSHAKE_INPUT_PORTS + HANDSHAKE_OUTPUT_PORTS
        ]
        fsm_upstream_module_ports = {}  # keyed by arg.name for deduplication
        task.fsm_module.add_ports(
            [
                Input(HANDSHAKE_CLK),
                Input(HANDSHAKE_RST_N),
                Input(HANDSHAKE_START),
                Output(HANDSHAKE_READY),
                *(Output(x) for x in (HANDSHAKE_DONE, HANDSHAKE_IDLE)),
            ]
        )

        # Wires connecting to the downstream (task instances).
        fsm_downstream_portargs: list[PortArg] = []
        fsm_downstream_module_ports = []

        for instance in task.instances:
            child_port_set = set(instance.task.module.ports)

            # add signal declarations
            for arg in instance.args:
                if not arg.cat.is_stream:
                    # Find arg width.
                    width = 64  # 64-bit address
                    if arg.cat.is_scalar:
                        width = width_table.get(arg.name, 0)
                        if width == 0:
                            # Constant literals are not in the width table.
                            width = int(arg.name.split("'d")[0])

                    # Find identifier name of the arg. May be a constant with "'d" in it
                    # If `arg` is an hmap, `arg.name` refers to the mmap offset, which
                    # needs to be set to 0. The actual address mapping will be handled
                    # between the AXI interconnect and the upstream M-AXI interface.
                    id_name = "64'd0" if arg.chan_count is not None else arg.name
                    # arg.name may be a constant

                    # Instantiate a pipeline for the arg.
                    q = Pipeline(
                        name=instance.get_instance_arg(id_name),
                        level=self.register_level,
                        width=width,
                    )
                    arg_table[arg.name] = q

                    # Add signals only for non-consts. Constants are passed as literals.
                    if "'d" not in q.name:
                        task.module.add_signals(
                            [
                                Wire(name=q[-1].name, width=make_width(width)),
                            ]
                        )
                        task.fsm_module.add_pipeline(q, init=Identifier(id_name))
                        _logger.debug("    pipelined signal: %s => %s", id_name, q.name)
                        fsm_upstream_module_ports.setdefault(
                            arg.name,
                            Input(arg.name, make_width(width)),
                        )
                        fsm_downstream_module_ports.append(
                            Output(q[-1].name, make_width(width)),
                        )
                        fsm_downstream_portargs.append(
                            make_port_arg(q[-1].name, q[-1].name),
                        )

                # arg.name is the upper-level name
                # arg.port is the lower-level name

                # check which ports are used for async_mmap
                if arg.cat.is_async_mmap:
                    for tag in ASYNC_MMAP_SUFFIXES:
                        if {
                            x.portname
                            for x in generate_async_mmap_ports(
                                tag=tag,
                                port=arg.port,
                                arg=arg.name,
                                offset_name=arg_table[arg.name][-1],
                                instance=instance,
                            )
                        } & child_port_set:
                            async_mmap_args.setdefault(arg, []).append(tag)

                # declare wires or forward async_mmap ports
                for tag in async_mmap_args.get(arg, []):
                    if task.is_upper and instance.task.is_lower:
                        task.module.add_signals(
                            generate_async_mmap_signals(
                                tag=tag,
                                arg=arg.mmap_name,
                                data_width=width_table[arg.name],
                            ),
                        )
                    else:
                        task.module.add_ports(
                            generate_async_mmap_ioports(
                                tag=tag,
                                arg=arg.name,
                                data_width=width_table[arg.name],
                            ),
                        )

            # add start registers
            start_q = Pipeline(
                f"{instance.start.name}_global",
                level=self.register_level,
            )
            task.fsm_module.add_pipeline(start_q, self.start_q[0])

            if instance.is_autorun:
                # autorun modules start when the global start signal is asserted
                task.fsm_module.add_logics(
                    [
                        Always(
                            sens_list=CLK_SENS_LIST,
                            statement=make_block(
                                make_if_with_block(
                                    cond=RST,
                                    true=NonblockingSubstitution(
                                        left=instance.start,
                                        right=FALSE,
                                    ),
                                    false=make_if_with_block(
                                        cond=start_q[-1],
                                        true=NonblockingSubstitution(
                                            left=instance.start,
                                            right=TRUE,
                                        ),
                                    ),
                                ),
                            ),
                        ),
                    ],
                )
            else:
                # set up state
                is_done_q = Pipeline(
                    f"{instance.is_done.name}",
                    level=self.register_level,
                )
                done_q = Pipeline(
                    f"{instance.done.name}_global",
                    level=self.register_level,
                )
                task.fsm_module.add_pipeline(is_done_q, instance.is_state(STATE10))
                task.fsm_module.add_pipeline(done_q, self.done_q[0])

                if_branch = instance.set_state(STATE00)
                else_branch = (
                    make_if_with_block(
                        cond=instance.is_state(STATE00),
                        true=make_if_with_block(
                            cond=start_q[-1],
                            true=instance.set_state(STATE01),
                        ),
                    ),
                    make_if_with_block(
                        cond=instance.is_state(STATE01),
                        true=make_if_with_block(
                            cond=instance.ready,
                            true=make_if_with_block(
                                cond=instance.done,
                                true=instance.set_state(STATE10),
                                false=instance.set_state(STATE11),
                            ),
                        ),
                    ),
                    make_if_with_block(
                        cond=instance.is_state(STATE11),
                        true=make_if_with_block(
                            cond=instance.done,
                            true=instance.set_state(STATE10),
                        ),
                    ),
                    make_if_with_block(
                        cond=instance.is_state(STATE10),
                        true=make_if_with_block(
                            cond=done_q[-1],
                            true=instance.set_state(STATE00),
                        ),
                    ),
                )
                task.fsm_module.add_logics(
                    [
                        Always(
                            sens_list=CLK_SENS_LIST,
                            statement=make_block(
                                make_if_with_block(
                                    cond=RST,
                                    true=if_branch,
                                    false=else_branch,
                                ),
                            ),
                        ),
                        Assign(
                            left=instance.start,
                            right=instance.is_state(STATE01),
                        ),
                    ],
                )

                is_done_signals.append(is_done_q)

            # insert handshake signals
            fsm_downstream_portargs.extend(
                make_port_arg(x.name, x.name) for x in instance.public_handshake_signals
            )
            task.module.add_signals(
                Wire(x.name, x.width) for x in instance.public_handshake_signals
            )
            task.fsm_module.add_signals(instance.all_handshake_signals)
            fsm_downstream_module_ports.extend(instance.public_handshake_ports)

            # add task module instances
            portargs = list(generate_handshake_ports(instance, RST_N))
            for arg in instance.args:
                if arg.cat.is_scalar:
                    portargs.append(
                        PortArg(portname=arg.port, argname=arg_table[arg.name][-1]),
                    )
                elif arg.cat.is_istream:
                    portargs.extend(
                        instance.task.module.generate_istream_ports(
                            port=arg.port,
                            arg=arg.name,
                        ),
                    )
                elif arg.cat.is_ostream:
                    portargs.extend(
                        instance.task.module.generate_ostream_ports(
                            port=arg.port,
                            arg=arg.name,
                        ),
                    )
                elif arg.cat.is_sync_mmap:
                    portargs.extend(
                        generate_m_axi_ports(
                            module=instance.task.module,
                            port=arg.port,
                            arg=arg.mmap_name,
                            arg_reg=arg_table[arg.name][-1].name,
                        ),
                    )
                elif arg.cat.is_async_mmap:
                    for tag in async_mmap_args[arg]:
                        portargs.extend(
                            generate_async_mmap_ports(
                                tag=tag,
                                port=arg.port,
                                arg=arg.mmap_name,
                                offset_name=arg_table[arg.name][-1],
                                instance=instance,
                            ),
                        )

            task.module.add_instance(
                module_name=get_module_name(instance.task.name),
                instance_name=instance.name,
                ports=portargs,
            )

        # Add scalar ports to the FSM module.
        fsm_upstream_portargs.extend(
            [make_port_arg(x.name, x.name) for x in fsm_upstream_module_ports.values()]
        )
        task.fsm_module.add_ports(fsm_upstream_module_ports.values())
        task.fsm_module.add_ports(fsm_downstream_module_ports)

        # instantiate async_mmap modules at the upper levels
        # the base address may not be 0, so must use full 64 bit
        addr_width = 64
        _logger.debug("Set the address width of async_mmap to %d", addr_width)

        if task.is_upper:
            for arg, tag in async_mmap_args.items():
                task.module.add_async_mmap_instance(
                    name=arg.mmap_name,
                    tags=tag,
                    rst=RST,
                    data_width=width_table[arg.name],
                    addr_width=addr_width,
                )

            task.module.add_instance(
                module_name=task.fsm_module.name,
                instance_name="__tapa_fsm_unit",
                ports=fsm_upstream_portargs + fsm_downstream_portargs,
            )

        return is_done_signals

    def _instantiate_global_fsm(
        self,
        module: Module,
        is_done_signals: list[Pipeline],
    ) -> None:
        # global state machine

        def is_state(state: IntConst) -> Eq:
            return Eq(left=STATE, right=state)

        def set_state(state: IntConst) -> NonblockingSubstitution:
            return NonblockingSubstitution(left=STATE, right=state)

        countdown = Identifier("countdown")
        countdown_width = (self.register_level - 1).bit_length()

        module.add_signals(
            [
                Reg(STATE.name, width=make_width(2)),
                Reg(countdown.name, width=make_width(countdown_width)),
            ],
        )

        state01_action = set_state(STATE10)
        if is_done_signals:
            state01_action = make_if_with_block(
                cond=make_operation(
                    operator=Land,
                    nodes=(x[-1] for x in reversed(is_done_signals)),
                ),
                true=state01_action,
            )

        global_fsm = make_case_with_block(
            comp=STATE,
            cases=[
                (
                    STATE00,
                    make_if_with_block(cond=self.start_q[-1], true=set_state(STATE01)),
                ),
                (
                    STATE01,
                    state01_action,
                ),
                (
                    STATE10,
                    [
                        set_state(STATE11 if self.register_level else STATE00),
                        NonblockingSubstitution(
                            left=countdown,
                            right=make_int(max(0, self.register_level - 1)),
                        ),
                    ],
                ),
                (
                    STATE11,
                    make_if_with_block(
                        cond=Eq(
                            left=countdown,
                            right=make_int(0, width=countdown_width),
                        ),
                        true=set_state(STATE00),
                        false=NonblockingSubstitution(
                            left=countdown,
                            right=Minus(
                                left=countdown,
                                right=make_int(1, width=countdown_width),
                            ),
                        ),
                    ),
                ),
            ],
        )

        module.add_logics(
            [
                Always(
                    sens_list=CLK_SENS_LIST,
                    statement=make_block(
                        make_if_with_block(
                            cond=RST,
                            true=set_state(STATE00),
                            false=global_fsm,
                        ),
                    ),
                ),
                Assign(left=IDLE, right=is_state(STATE00)),
                Assign(left=DONE, right=self.done_q[-1]),
                Assign(left=READY, right=self.done_q[0]),
            ],
        )

        module.add_pipeline(self.start_q, init=START)
        module.add_pipeline(self.done_q, init=is_state(STATE10))

    def _instrument_upper_task(
        self,
        task: Task,
        print_fifo_ops: bool,
    ) -> None:
        """Codegen for the top task."""
        assert task.is_upper
        task.module.cleanup()
        if task.name == self.top and self.vitis_mode:
            task.module.add_rs_pragmas()
        task.add_rs_pragmas_to_fsm()

        self._instantiate_fifos(task, print_fifo_ops)
        self._connect_fifos(task)
        width_table = {port.name: port.width for port in task.ports.values()}
        is_done_signals = self._instantiate_children_tasks(
            task,
            width_table,
        )
        self._instantiate_global_fsm(task.fsm_module, is_done_signals)

        with open(
            self.get_rtl(task.fsm_module.name),
            "w",
            encoding="utf-8",
        ) as rtl_code:
            rtl_code.write(task.fsm_module.code)

        # generate the top-level task
        with open(self.get_rtl(task.name), "w", encoding="utf-8") as rtl_code:
            rtl_code.write(task.module.code)

    def _get_fifo_width(self, task: Task, fifo: str) -> int:
        producer_task, _, fifo_port = task.get_connection_to(fifo, "produced_by")
        port = self.get_task(producer_task).module.get_port_of(
            fifo_port,
            OSTREAM_SUFFIXES[0],
        )
        # TODO: err properly if not integer literals
        return int(port.width.msb.value) - int(port.width.lsb.value) + 1
