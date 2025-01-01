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
from pathlib import Path
from xml.etree import ElementTree as ET

import toposort
import yaml
from psutil import cpu_count
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
    ModuleDef,
    Node,
    NonblockingSubstitution,
    Output,
    Plus,
    PortArg,
    Reg,
    SingleStatement,
    StringConst,
    SystemCall,
    Width,
    Wire,
)
from pyverilog.vparser.parser import VerilogCodeParser

from tapa.backend.xilinx import RunAie, RunHls
from tapa.instance import Instance, Port
from tapa.safety_check import check_mmap_arg_name
from tapa.task import Task
from tapa.util import (
    clang_format,
    extract_ports_from_module,
    get_instance_name,
    get_module_name,
    get_vendor_include_paths,
    get_xpfm_path,
)
from tapa.verilog.ast_utils import (
    make_block,
    make_case_with_block,
    make_if_with_block,
    make_operation,
    make_port_arg,
    make_width,
)
from tapa.verilog.util import Pipeline, match_array_name, wire_name
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
    STREAM_DATA_SUFFIXES,
    STREAM_EOT_SUFFIX,
    STREAM_PORT_DIRECTION,
    TRUE,
)
from tapa.verilog.xilinx.module import Module, generate_m_axi_ports, get_streams_fifos

_logger = logging.getLogger().getChild(__name__)

STATE00 = IntConst("2'b00")
STATE01 = IntConst("2'b01")
STATE11 = IntConst("2'b11")
STATE10 = IntConst("2'b10")

MOD_DEF_IOS = []


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
        self,
        obj: dict,
        vitis_mode: bool,
        work_dir: str | None = None,
        gen_templates: tuple[str, ...] = (),
        rtl_paths: tuple[Path, ...] = (),
    ) -> None:
        """Construct Program object from a json file.

        Args:
        ----
          obj: json object.
          work_dir: Specify a working directory as a string. If None, a temporary
              one will be created.
          gen_templates: Tuple of task names that are templates. If a task is
              specified as a template, its verilog module will only contains io
              ports and no logic.

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

        task_names = toposort.toposort_flatten(
            {k: set(v.get("tasks", ())) for k, v in obj["tasks"].items()},
        )
        for template in gen_templates:
            assert (
                template in task_names
            ), f"template task {template} not found in design"
        self.gen_templates = gen_templates

        for name in task_names:
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
        self._hls_report_xmls: dict[str, ET.ElementTree] = {}

        # Collect user custom RTL files
        self.custom_rtl: list[Path] = Program.get_custom_rtl_files(rtl_paths)

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
    def start_q(self) -> Pipeline:
        return Pipeline(START.name, level=0)

    @property
    def done_q(self) -> Pipeline:
        return Pipeline(DONE.name, level=0)

    @property
    def rtl_dir(self) -> str:
        return os.path.join(self.work_dir, "hdl")

    @property
    def template_dir(self) -> str:
        return os.path.join(self.work_dir, "template")

    @property
    def report_dir(self) -> str:
        return os.path.join(self.work_dir, "report")

    @property
    def cpp_dir(self) -> str:
        cpp_dir = os.path.join(self.work_dir, "cpp")
        os.makedirs(cpp_dir, exist_ok=True)
        return cpp_dir

    @staticmethod
    def get_custom_rtl_files(rtl_paths: tuple[Path, ...]) -> list[Path]:
        custom_rtl: list[Path] = []
        for path in rtl_paths:
            if path.is_file():
                if path.suffix != ".v":
                    msg = f"unsupported file type: {path}"
                    raise ValueError(msg)
                custom_rtl.append(path)
            elif path.is_dir():
                vlg_files = list(path.rglob("*.v"))
                if not vlg_files:
                    msg = f"no verilog files found in {path}"
                    raise ValueError(msg)
                custom_rtl.extend(vlg_files)
            elif path.exists():
                msg = f"unsupported path: {path}"
                raise ValueError(msg)
            else:
                msg = f"path does not exist: {path.absolute()}"
                raise ValueError(msg)
        return custom_rtl

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

    def get_rtl_template(self, name: str) -> str:
        os.makedirs(self.template_dir, exist_ok=True)
        return os.path.join(
            self.template_dir,
            name + RTL_SUFFIX,
        )

    def _get_hls_report_xml(self, name: str) -> ET.ElementTree:
        tree = self._hls_report_xmls.get(name)
        if tree is None:
            filename = os.path.join(self.report_dir, f"{name}_csynth.xml")
            self._hls_report_xmls[name] = tree = ET.parse(filename)
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

    def extract_cpp(self, target: str = "hls") -> Program:
        """Extract HLS/AIE C++ files."""
        _logger.info("extracting %s C++ files", target)
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

    def run_hls_or_aie(  # noqa: PLR0913,PLR0917  # TODO: refactor this method
        self,
        clock_period: float | str,
        part_num: str,
        skip_based_on_mtime: bool = False,
        other_configs: str = "",
        jobs: int | None = None,
        keep_hls_work_dir: bool = False,
        flow_type: str = "hls",
        platform: str | None = None,
    ) -> Program:
        """Run HLS with extracted HLS C++ files and generate tarballs."""
        self.extract_cpp(flow_type)

        _logger.info("running %s", flow_type)

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
            if flow_type == "hls":
                with (
                    open(self.get_tar(task.name), "wb") as tarfileobj,
                    RunHls(
                        tarfileobj,
                        kernel_files=[(self.get_cpp(task.name), hls_cflags)],
                        work_dir=f"{self.work_dir}/hls" if keep_hls_work_dir else None,
                        top_name=task.name,
                        clock_period=str(clock_period),
                        part_num=part_num,
                        auto_prefix=True,
                        hls="vitis_hls",
                        std="c++14",
                        other_configs=other_configs,
                    ) as proc,
                ):
                    stdout, stderr = proc.communicate()
            elif flow_type == "aie":
                assert platform is not None, "Platform must be specified for AIE flow."
                with (
                    open(self.get_tar(task.name), "wb") as tarfileobj,
                    RunAie(
                        tarfileobj,
                        kernel_files=[self.get_cpp(task.name)],
                        work_dir=f"{self.work_dir}/aie" if keep_hls_work_dir else None,
                        top_name=task.name,
                        clock_period=str(clock_period),
                        xpfm=get_xpfm_path(platform),
                    ) as proc,
                ):
                    stdout, stderr = proc.communicate()
            else:
                msg = f"Unknown flow type: {flow_type}"
                raise ValueError(msg)

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
                msg = f"{flow_type} failed for {task.name}"
                raise RuntimeError(msg)

        jobs = jobs or cpu_count(logical=False)
        _logger.info(
            "spawn %d workers for parallel %s synthesis of the tasks", jobs, flow_type
        )
        with futures.ThreadPoolExecutor(max_workers=jobs) as executor:
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
                self._instrument_upper_and_template_task(task, print_fifo_ops)
            elif not task.is_upper and task.name in self.gen_templates:
                assert task.ports
                self._instrument_upper_and_template_task(task, print_fifo_ops)

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
        if self.top_task.name in self.gen_templates:
            msg = "top task cannot be a template"
            raise ValueError(msg)

        # instrument the top-level RTL
        _logger.info("instrumenting top-level RTL")
        self._instrument_upper_and_template_task(
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

                    # if the task is lower, the eot and data are concated together
                    # so we need to subtract the eot width to get the data width
                    if (
                        suffix in STREAM_DATA_SUFFIXES
                        and self.get_task(task_name).is_lower
                    ):
                        wire_width = Width(
                            Minus(wire_width.msb, IntConst(1)),
                            wire_width.lsb,
                        )

                    if suffix in STREAM_DATA_SUFFIXES:
                        if not isinstance(wire_width, Width):
                            msg = (
                                f"width of {w_name} is not a pyverilog Width: "
                                f"{wire_width}"
                            )
                            raise ValueError(msg)
                        data_wire = Wire(
                            name=w_name,
                            width=wire_width,
                        )
                        task.module.add_signals([data_wire])
                        eot_wire = Wire(
                            name=wire_name(fifo_name, suffix + STREAM_EOT_SUFFIX)
                        )
                        task.module.add_signals([eot_wire])
                    else:
                        data_wire = Wire(name=w_name, width=wire_width)
                        task.module.add_signals([data_wire])

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
        ignore_peeks_fifos: tuple[str, ...] = (),
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
                        level=0,
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
                level=0,
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
                    level=0,
                )
                done_q = Pipeline(
                    f"{instance.done.name}_global",
                    level=0,
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
                            ignore_peek_fifos=ignore_peeks_fifos,
                            split_eot=instance.task.is_upper,
                        ),
                    )
                elif arg.cat.is_ostream:
                    portargs.extend(
                        instance.task.module.generate_ostream_ports(
                            port=arg.port,
                            arg=arg.name,
                            split_eot=instance.task.is_upper,
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

        module.add_signals(
            [
                Reg(STATE.name, width=make_width(2)),
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
                        set_state(STATE00),
                    ],
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

    def _instrument_upper_and_template_task(  # noqa: C901,PLR0912, PLR0915 # TODO: refactor this method
        self,
        task: Task,
        print_fifo_ops: bool,
    ) -> None:
        """Codegen for the top task."""
        # assert task.is_upper
        task.module.cleanup()
        if task.name == self.top and self.vitis_mode:
            task.module.add_rs_pragmas()
        if task.name not in self.gen_templates:
            task.add_rs_pragmas_to_fsm()

        # remove top level peek ports
        top_fifos = set()
        if task.name == self.top_task.name:
            _logger.debug("remove top peek ports")
            for port_name, port in task.ports.items():
                if port.cat.is_istream:
                    fifos = [port_name]
                elif port.is_istreams:
                    fifos = get_streams_fifos(task.module, port_name)
                else:
                    continue
                for fifo in fifos:
                    for suffix in ISTREAM_SUFFIXES:
                        match = match_array_name(fifo)
                        if match is None:
                            peek_port = f"{fifo}_peek"
                        else:
                            peek_port = f"{match[0]}_peek[{match[1]}]"

                        if not task.module.find_port(peek_port, suffix):
                            continue

                        _logger.debug(
                            "  remove %s",
                            task.module.get_port_of(peek_port, suffix).name,
                        )
                        if task.module.del_ports(
                            [task.module.get_port_of(peek_port, suffix).name]
                        ):
                            top_fifos.add(fifo)
                        else:
                            msg = f"failed to remove {peek_port} from {task.name}"
                            raise ValueError(msg)

        # split stream dout to data and eot
        _logger.info("split stream data ports to data and eot on %s", task.name)
        for port_name, port in task.ports.items():
            if not port.cat.is_stream and not port.is_streams:
                continue
            for suffix in STREAM_DATA_SUFFIXES:
                if port.cat.is_stream:
                    fifos = [port_name]
                else:
                    # streams
                    fifos = get_streams_fifos(task.module, port_name)
                # peek
                if (
                    self.top_task.name != task.name
                    and STREAM_PORT_DIRECTION[suffix] == "input"
                ):
                    match = match_array_name(port_name)
                    if match is None:
                        peek_port = f"{port_name}_peek"
                    else:
                        peek_port = f"{match[0]}_peek[{match[1]}]"
                    if port.cat.is_stream:
                        fifos.append(peek_port)
                    else:
                        # streams
                        fifos.extend(get_streams_fifos(task.module, peek_port))

                for subport_name in fifos:
                    if not task.module.find_port(subport_name, suffix):
                        continue

                    _logger.info("  split %s.%s", subport_name, suffix)

                    dout_port = task.module.get_port_of(subport_name, suffix)
                    dout_width = dout_port.width
                    assert isinstance(dout_width, Width)

                    # remove original dout port
                    removed_ports = task.module.del_ports(
                        [task.module.get_port_of(subport_name, suffix).name]
                    )
                    assert removed_ports[0] == dout_port.name

                    # add new data and eot ports
                    new_data_port = type(dout_port)(
                        name=dout_port.name,
                        width=Width(
                            msb=Minus(dout_width.msb, IntConst(1)),
                            lsb=dout_width.lsb,
                        ),
                    )
                    new_eot_port = type(dout_port)(
                        name=f"{dout_port.name}{STREAM_EOT_SUFFIX}"
                    )
                    task.module.add_ports([new_data_port, new_eot_port])

        if task.name in self.gen_templates:
            _logger.info("skip instrumenting template task %s", task.name)
            if task.name in self.gen_templates:
                with open(
                    self.get_rtl_template(task.name), "w", encoding="utf-8"
                ) as rtl_code:
                    rtl_code.write(task.module.code)
        else:
            self._instantiate_fifos(task, print_fifo_ops)
            self._connect_fifos(task)
            width_table = {port.name: port.width for port in task.ports.values()}
            is_done_signals = self._instantiate_children_tasks(
                task,
                width_table,
                tuple(top_fifos),
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

    def _get_fifo_width(self, task: Task, fifo: str) -> Node:
        producer_task, _, fifo_port = task.get_connection_to(fifo, "produced_by")
        port = self.get_task(producer_task).module.get_port_of(
            fifo_port,
            OSTREAM_SUFFIXES[0],
        )
        if task.is_upper:
            # upper task have separated data and eot ports
            # so the width should be the data port width + 1
            return Plus(Minus(port.width.msb, port.width.lsb), IntConst(2))
        # TODO: err properly if not integer literals
        return Plus(Minus(port.width.msb, port.width.lsb), IntConst(1))

    def replace_custom_rtl(self) -> None:
        """Add custom RTL files to the project.

        It will replace all files that originally exist in the project.

        Args:
            file_paths (List[Path]): List of file paths to copy.
            destination_folder (Path): The target folder where files will be copied.
        """
        rtl_path = Path(self.rtl_dir)
        assert Path.exists(rtl_path)

        self.check_custom_rtl_format(self.custom_rtl)

        for file_path in self.custom_rtl:
            assert file_path.is_file()

            # Determine destination path
            dest_path = rtl_path / file_path.name

            if dest_path.exists():
                _logger.info("Replacing %s with custom RTL.", file_path.name)
            else:
                _logger.info("Adding custom RTL %s.", file_path.name)

            # Copy file to destination, replacing if necessary
            shutil.copy2(file_path, dest_path)

    def check_custom_rtl_format(self, rtl_paths: list[Path]) -> None:
        """Check if the custom RTL files are in the correct format."""
        if not rtl_paths:
            return
        _logger.info("Checking custom RTL files format.")
        with tempfile.TemporaryDirectory(prefix="pyverilog-") as output_dir:
            codeparser = VerilogCodeParser(
                rtl_paths,
                preprocess_output=os.path.join(output_dir, "preprocess.output"),
                outputdir=output_dir,
                debug=False,
            )
            ast = codeparser.parse()
        # Traverse the AST to find module definitions
        module_defs: list[ModuleDef] = [
            mod_def
            for mod_def in ast.description.definitions
            if isinstance(mod_def, ModuleDef)
        ]

        for module_def in module_defs:
            for task_name, task in self._tasks.items():
                if task_name != module_def.name:
                    continue
                _logger.info("Checking custom RTL file format for task %s.", task_name)
                task_port_infos = extract_ports_from_module(
                    task.module.get_module_def()
                )
                custom_rtl_port_infos = extract_ports_from_module(module_def)
                if task_port_infos != custom_rtl_port_infos:
                    assert set(task_port_infos.keys()) == set(
                        custom_rtl_port_infos.keys()
                    )
                    task_port_infos_str = "\n".join(
                        f"  {port}" for port in task_port_infos.values()
                    )
                    custom_rtl_port_infos_str = "\n".join(
                        f"  {port}" for port in custom_rtl_port_infos.values()
                    )
                    msg = (
                        f"Custom RTL file for task {task_name} does not match the "
                        f"expected ports. \nTask ports: \n{task_port_infos_str}\n"
                        f"Custom RTL ports:\n{custom_rtl_port_infos_str}"
                    )
                    raise ValueError(msg)
