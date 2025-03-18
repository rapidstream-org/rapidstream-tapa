"""Core logic of TAPA."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import contextlib
import decimal
import glob
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
import zipfile
from collections.abc import Generator
from concurrent import futures
from pathlib import Path
from typing import NamedTuple
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
    Node,
    NonblockingSubstitution,
    Output,
    Plus,
    PortArg,
    Reg,
    SingleStatement,
    StringConst,
    SystemCall,
    Wire,
)
from pyverilog.vparser.parser import ParseError

from tapa.backend.xilinx import RunAie, RunHls
from tapa.common.paths import find_resource
from tapa.instance import Instance, Port
from tapa.safety_check import check_mmap_arg_name
from tapa.synthesis import ProgramSynthesisMixin
from tapa.task import Task
from tapa.util import (
    Options,
    clang_format,
    get_instance_name,
    get_module_name,
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
from tapa.verilog.xilinx import generate_handshake_ports, pack
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
    HANDSHAKE_INPUT_PORTS,
    HANDSHAKE_OUTPUT_PORTS,
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
from tapa.verilog.xilinx.module import Module, generate_m_axi_ports, get_streams_fifos

_logger = logging.getLogger().getChild(__name__)

STATE00 = IntConst("2'b00")
STATE01 = IntConst("2'b01")
STATE11 = IntConst("2'b11")
STATE10 = IntConst("2'b10")

FIFO_DIRECTIONS = ["consumed_by", "produced_by"]

# AIE_DEPTH is the number of elements in the AIE windows.
AIE_DEPTH = 64


def gen_declarations(task: Task) -> tuple[list[str], list[str], list[str]]:
    """Generates kernel and port declarations."""
    port_decl = [
        f"input_plio p_{port.name};"
        if port.is_immap or port.is_istream
        else f"output_plio p_{port.name};"
        for port in task.ports.values()
    ]
    kernel_decl = [
        f"kernel k_{name}{i};"
        for name, insts in task.tasks.items()
        for i in range(len(insts))
    ]
    header_decl = [f'#include "{name}.h"' for name in task.tasks]
    return header_decl, kernel_decl, port_decl


def gen_definitions(task: Task) -> tuple[list[str], ...]:
    """Generates kernel and port definitions."""
    kernel_def = [
        f"k_{name}{i} = kernel::create({name});"
        for name, insts in task.tasks.items()
        for i in range(len(insts))
    ]
    kernel_source = [
        f'source(k_{name}{i}) = "../../cpp/{name}.cpp";'
        for name, insts in task.tasks.items()
        for i in range(len(insts))
    ]

    kernel_header = [
        f'//headers(k_{name}{i}) = {{"{name}.h"}};'
        for name, insts in task.tasks.items()
        for i in range(len(insts))
    ]
    kernel_header = []
    kernel_runtime = [
        f"runtime<ratio>(k_{name}{i}) = OCCUPANCY;"
        for name, insts in task.tasks.items()
        for i in range(len(insts))
    ]
    kernel_loc = [
        f"//location<kernel>(k_{name}{i}) = tile(X, X);"
        for name, insts in task.tasks.items()
        for i in range(len(insts))
    ]

    port_def = [
        f'p_{port.name} = input_plio::create("{port.name}",'
        f' plio_{port.width}_bits, "{port.name}.txt");'
        if port.is_immap or port.is_istream
        else f'p_{port.name} = output_plio::create("{port.name}",'
        f' plio_{port.width}_bits, "{port.name}.txt");'
        for port in task.ports.values()
    ]
    return (
        kernel_def,
        kernel_source,
        kernel_header,
        kernel_runtime,
        kernel_loc,
        port_def,
    )


def gen_connections(task: Task) -> list[str]:  # noqa: C901 PLR0912
    """Generates connections between ports and kernels."""
    # TODO: refactor this function
    link_from_src = {}
    link_to_dst = {}
    for name, insts in task.tasks.items():
        for i, inst in enumerate(insts):
            arg_dict = inst["args"]
            in_num = 0
            out_num = 0
            for conn_dict in arg_dict.values():
                if conn_dict["cat"] == "istream":
                    link_to_dst[conn_dict["arg"]] = [
                        f"{name}{i}.in[{in_num}]",
                        "net",
                    ]
                    in_num += 1
                elif conn_dict["cat"] == "ostream":
                    link_from_src[conn_dict["arg"]] = [
                        f"{name}{i}.out[{out_num}]",
                        "net",
                    ]
                    out_num += 1
                elif conn_dict["cat"] == "immap":
                    link_to_dst[conn_dict["arg"]] = [
                        f"{name}{i}.in[{in_num}]",
                        "io",
                    ]
                    in_num += 1
                elif conn_dict["cat"] == "ommap":
                    link_from_src[conn_dict["arg"]] = [
                        f"{name}{i}.out[{out_num}]",
                        "io",
                    ]
                    out_num += 1
                else:
                    msg = f"Unknown connection category: {conn_dict['cat']}"
                    raise ValueError(msg)

    for k, val in link_from_src.items():
        pass

    for k, val in link_to_dst.items():
        pass

    connect_def = [
        f"connect<stream> {name} (k_{link_from_src[name][0]},"
        f" k_{link_to_dst[name][0]});"
        for name in task.fifos
        if name in link_from_src and name in link_to_dst
    ]

    for port in task.ports.values():
        name = port.name
        width = port.width
        window_default_size = int(width) // 8 * AIE_DEPTH
        if name in link_from_src:
            if link_from_src[name][1] == "io":
                connect_def.append(
                    f"connect<window<{window_default_size}>> {name}_link"
                    f" (k_{link_from_src[name][0]}, p_{name}.in[0]);"
                )
            elif link_from_src[name][1] == "net":
                connect_def.append(
                    f"connect<stream> {name}_link"
                    f" (k_{link_from_src[name][0]}, p_{name}.in[0]);"
                )
            else:
                msg = f"Port[{name}] should be connected to io/net"
                raise ValueError(msg)

        if name in link_to_dst:
            if link_to_dst[name][1] == "io":
                connect_def.append(
                    f"connect<window<{window_default_size}>> {name}_link"
                    f" (p_{name}.out[0], k_{link_to_dst[name][0]});"
                )
            elif link_to_dst[name][1] == "net":
                connect_def.append(
                    f"connect<stream> {name}_link"
                    f" (p_{name}.out[0], k_{link_to_dst[name][0]});"
                )
            else:
                msg = f"Port[{name}] should be connected to io"
                raise ValueError(msg)

    return connect_def


class Program(ProgramSynthesisMixin):  # noqa: PLR0904  # TODO: refactor this class
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

    GRAPH_HEADER_TEMPLATE = r"""
#pragma once
#include <adf.h>
#include "common.h"
#define OCCUPANCY 0.9
using namespace adf;
class {graph_name}: public graph
{{
private:
    {kernel_decl}

public:
    {port_decl}

    {graph_name}()
	{{
		// create kernel
        {kernel_def}
        {kernel_source}
        {kernel_header}
        {kernel_runtime}
        {kernel_loc}

		// create port
        {port_def}

		// connect port and kernel
        {connect_def}
	}};
}};
"""

    GRAPH_CPP_TEMPLATE = r"""
// Copyright 2024 RapidStream Design Automation, Inc.
// All Rights Reserved.

#include "{top_task_name}.h"

using namespace adf;

{top_task_name} my_graph;

int main(int argc, char ** argv)
{{
	my_graph.init();
#if defined(__X86SIM__)
	my_graph.run(1);
#else
	my_graph.run();
#endif
	my_graph.end();
	return 0;
}}
"""

    def __init__(
        self,
        obj: dict,
        vitis_mode: bool,
        work_dir: str | None = None,
        gen_templates: tuple[str, ...] = (),
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
        self._tasks: dict[str, Task] = {}

        task_names = toposort.toposort_flatten(
            {k: set(v.get("tasks", ())) for k, v in obj["tasks"].items()},
        )
        for template in gen_templates:
            assert template in task_names, (
                f"template task {template} not found in design"
            )
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
                target_type=task_properties["target"],
            )
            if not task.is_upper or task.tasks:
                self._tasks[name] = task

            # add non-synthesizable tasks to gen_templates
            if (
                task_properties["target"] == "non_synthesizable"
                and name not in self.gen_templates
            ):
                self.gen_templates += (name,)

        self.files: dict[str, str] = {}
        self._hls_report_xmls: dict[str, ET.ElementTree] = {}

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
    def start_q(self) -> Pipeline:
        return Pipeline(START.name)

    @property
    def done_q(self) -> Pipeline:
        return Pipeline(DONE.name)

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

    @property
    def report(self):
        # ruff: noqa: ANN201
        """Returns all formats of TAPA report as a namedtuple."""

        class Report(NamedTuple):
            json: str
            yaml: str

        return Report(
            json=os.path.join(self.work_dir, "report.json"),
            yaml=os.path.join(self.work_dir, "report.yaml"),
        )

    @staticmethod
    def _get_custom_rtl_files(rtl_paths: tuple[Path, ...]) -> list[Path]:
        custom_rtl: list[Path] = []
        for path in rtl_paths:
            if path.is_file():
                custom_rtl.append(path)
            elif path.is_dir():
                rtl_files = list(path.rglob("*"))
                if not rtl_files:
                    msg = f"no rtl files found in {path}"
                    raise ValueError(msg)
                custom_rtl.extend(rtl_files)
            elif path.exists():
                msg = f"unsupported path: {path}"
                raise ValueError(msg)
            else:
                msg = f"path does not exist: {path.absolute()}"
                raise ValueError(msg)
        return custom_rtl

    def get_task(self, name: str) -> Task:
        return self._tasks[name]

    def get_cpp_path(self, name: str) -> str:
        return os.path.join(self.cpp_dir, name + ".cpp")

    def get_common_path(self) -> str:
        return os.path.join(self.cpp_dir, "common.h")

    def get_header_path(self, name: str) -> str:
        return os.path.join(self.cpp_dir, name + ".h")

    def get_post_syn_rpt_path(self, module_name: str) -> str:
        return os.path.join(self.report_dir, f"{module_name}.hier.util.rpt")

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

    def extract_cpp(self, target: str = "hls") -> "Program":
        """Extract HLS/AIE C++ files."""
        _logger.info("extracting %s C++ files", target)
        check_mmap_arg_name(list(self._tasks.values()))

        top_aie_task_is_done = False
        for task in self._tasks.values():
            if task.name == self.top and target == "aie":
                assert top_aie_task_is_done is False, (
                    "There should be exactly one top-level task"
                )
                top_aie_task_is_done = True
                code_content = self.get_aie_graph(task)
                with open(
                    self.get_header_path(task.name), "w", encoding="utf-8"
                ) as src_code:
                    src_code.write(code_content)
                with open(
                    self.get_cpp_path(task.name), "w", encoding="utf-8"
                ) as src_code:
                    src_code.write(
                        self.GRAPH_CPP_TEMPLATE.format(top_task_name=self.top)
                    )
                with open(self.get_common_path(), "w", encoding="utf-8") as src_code:
                    src_code.write(
                        clang_format(task.code).replace(
                            "#include <tapa.h>", "#include <adf.h>"
                        )
                    )
            else:
                code_content = clang_format(task.code)
                if target == "aie":
                    code_content = code_content.replace(
                        "#include <tapa.h>", "#include <adf.h>"
                    )
                try:
                    with open(
                        self.get_cpp_path(task.name), encoding="utf-8"
                    ) as src_code:
                        if src_code.read() == code_content:
                            _logger.debug(
                                "not updating %s since its content is up-to-date",
                                src_code.name,
                            )
                            continue
                except FileNotFoundError:
                    pass
                with open(
                    self.get_cpp_path(task.name), "w", encoding="utf-8"
                ) as src_code:
                    src_code.write(code_content)
        for name, content in self.headers.items():
            header_path = os.path.join(self.cpp_dir, name)
            os.makedirs(os.path.dirname(header_path), exist_ok=True)
            with open(header_path, "w", encoding="utf-8") as header_fp:
                header_fp.write(content)
        return self

    def run_hls_or_aie(  # noqa: PLR0913,PLR0917,C901  # TODO: refactor this method
        self,
        clock_period: float | str,
        part_num: str,
        skip_based_on_mtime: bool = False,
        other_configs: str = "",
        jobs: int | None = None,
        keep_hls_work_dir: bool = False,
        flow_type: str = "hls",
        platform: str | None = None,
    ) -> "Program":
        """Run HLS with extracted HLS C++ files and generate tarballs."""
        self.extract_cpp(flow_type)

        _logger.info("running %s", flow_type)

        def worker(task: Task, idx: int) -> None:
            _logger.info("start worker for %s, target: %s", task.name, task.target_type)
            os.nice(idx % 19)
            try:
                if skip_based_on_mtime and os.path.getmtime(
                    self.get_tar(task.name),
                ) > os.path.getmtime(self.get_cpp_path(task.name)):
                    _logger.info(
                        "skipping HLS for %s since %s is newer than %s",
                        task.name,
                        self.get_tar(task.name),
                        self.get_cpp_path(task.name),
                    )
                    return
            except OSError:
                pass
            hls_defines = "-DTAPA_TARGET_DEVICE_ -DTAPA_TARGET_XILINX_HLS_"
            # WORKAROUND: Vitis HLS requires -I or gflags cannot be found...
            hls_includes = f"-I{find_resource('tapa-extra-runtime-include')}"
            hls_cflags = f"{self.cflags} {hls_defines} {hls_includes}"
            if flow_type == "hls":
                with (
                    open(self.get_tar(task.name), "wb") as tarfileobj,
                    RunHls(
                        tarfileobj,
                        kernel_files=[(self.get_cpp_path(task.name), hls_cflags)],
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
                if task.name != self.top:
                    # For AIE flow, only the top-level task is synthesized
                    return
                assert platform is not None, "Platform must be specified for AIE flow."
                with (
                    open(self.get_tar(task.name), "wb") as tarfileobj,
                    RunAie(
                        tarfileobj,
                        kernel_files=[self.get_cpp_path(task.name)],
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

                # Neglect the dummy bug message from AIE 2022.2
                aie_dummy_bug_msg = "/bin/sh: 1: [[: not found"
                if aie_dummy_bug_msg not in stderr.decode("utf-8"):
                    raise RuntimeError(msg)

        jobs = jobs or cpu_count(logical=False)
        _logger.info(
            "spawn %d workers for parallel %s synthesis of the tasks", jobs, flow_type
        )

        try:
            with futures.ThreadPoolExecutor(max_workers=jobs) as executor:
                any(executor.map(worker, self._tasks.values(), itertools.count(0)))
        except RuntimeError:
            if not keep_hls_work_dir:
                _logger.error(
                    "HLS failed, see above for details. You may use "
                    "`--keep-hls-work-dir` to keep the HLS work directory "
                    "for debugging."
                )
            else:
                _logger.error(
                    "HLS failed, see above for details. Please check the logs in %s",
                    os.path.join(self.work_dir, "hls"),
                )
            sys.exit(1)

        return self

    def generate_task_rtl(self, print_fifo_ops: bool) -> "Program":
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
            (map if Options.enable_pyslang else futures.ProcessPoolExecutor().map)(
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

    def generate_top_rtl(self, print_fifo_ops: bool) -> "Program":
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

        # instrument the top-level RTL if it is a upper-level task
        if self.top_task.is_upper:
            self._instrument_upper_and_template_task(
                self.top_task,
                print_fifo_ops,
            )

        _logger.info("generating report")
        task_report = self.top_task.report
        with open(self.report.yaml, "w", encoding="utf-8") as fp:
            yaml.dump(task_report, fp, default_flow_style=False, sort_keys=False)
        with open(self.report.json, "w", encoding="utf-8") as fp:
            json.dump(task_report, fp, indent=2)

        # self.files won't be populated until all tasks are instrumented
        _logger.info("writing generated auxiliary RTL files")
        for name, content in self.files.items():
            with open(os.path.join(self.rtl_dir, name), "w", encoding="utf-8") as fp:
                fp.write(content)

        return self

    def pack_rtl(self, output_file: str) -> "Program":
        _logger.info("packaging RTL code")
        with contextlib.ExitStack() as stack:  # avoid nested with statement
            tmp_fp = stack.enter_context(tempfile.TemporaryFile())
            pack(
                top_name=self.top,
                ports=self.toplevel_ports,
                rtl_dir=self.rtl_dir,
                part_num=self._get_part_num(self.top),
                output_file=tmp_fp,
            )
            tmp_fp.seek(0)

            _logger.info("packaging HLS report")
            packed_obj = stack.enter_context(zipfile.ZipFile(tmp_fp, "a"))
            output_fp = stack.enter_context(zipfile.ZipFile(output_file, "w"))
            for filename in self.report:
                arcname = os.path.basename(filename)
                _logger.debug("  packing %s", arcname)
                packed_obj.write(filename, arcname)

            report_glob = os.path.join(glob.escape(self.report_dir), "*_csynth.xml")
            for filename in glob.iglob(report_glob):
                arcname = os.path.join("report", os.path.basename(filename))
                _logger.debug("  packing %s", arcname)
                packed_obj.write(filename, arcname)

            # redact timestamp, source location etc. to make xo reproducible
            for info in packed_obj.infolist():
                redacted_info = zipfile.ZipInfo(info.filename)
                redacted_info.compress_type = zipfile.ZIP_DEFLATED
                redacted_info.external_attr = info.external_attr
                output_fp.writestr(redacted_info, _redact(packed_obj, info))

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
                width=self.get_fifo_width(task, fifo_name),
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
        async_mmap_args: dict[Instance.Arg, list[str]] = {}

        task.add_m_axi(width_table, self.files)

        # Wires connecting to the upstream (s_axi_control).
        fsm_upstream_portargs: list[PortArg] = [
            make_port_arg(x, x) for x in HANDSHAKE_INPUT_PORTS + HANDSHAKE_OUTPUT_PORTS
        ]
        fsm_upstream_module_ports = {}  # keyed by arg.name for deduplication

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
            start_q = Pipeline(f"{instance.start.name}_global")
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
                is_done_q = Pipeline(f"{instance.is_done.name}")
                done_q = Pipeline(f"{instance.done.name}_global")
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
        task.add_rs_pragmas_to_fsm()

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

    def _instrument_upper_and_template_task(  # noqa: C901, PLR0912 # TODO: refactor this method
        self,
        task: Task,
        print_fifo_ops: bool,
    ) -> None:
        """Codegen for the top task."""
        # assert task.is_upper
        task.module.cleanup()
        if task.name == self.top and self.vitis_mode:
            task.module.add_rs_pragmas()

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

                        # Cannot use find_ports instead of get_port_of
                        # since find port only check if the port name start with
                        # the given fifo name and end with the suffix. This causes
                        # wrong port being matched.
                        # e.g. fifo = "a_fifo", suffix = "dout", find_ports will
                        # match both "a_fifo_dout" and "a_fifo_ack_dout" even
                        # though the latter has fifo name "a_fifo_ack" instead of
                        # "a_fifo".
                        try:
                            task.module.get_port_of(peek_port, suffix)
                        except Module.NoMatchingPortError:
                            continue

                        name = task.module.get_port_of(peek_port, suffix).name
                        _logger.debug("  remove %s", name)
                        task.module.del_port(name)
                        top_fifos.add(fifo)

        if task.name in self.gen_templates:
            _logger.info("skip instrumenting template task %s", task.name)
            if task.name in self.gen_templates:
                with open(
                    self.get_rtl_template(task.name), "w", encoding="utf-8"
                ) as rtl_code:
                    rtl_code.write(task.module.get_template_code())
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

    def get_fifo_width(self, task: Task, fifo: str) -> Node:
        producer_task, _, fifo_port = task.get_connection_to(fifo, "produced_by")
        port = self.get_task(producer_task).module.get_port_of(
            fifo_port,
            OSTREAM_SUFFIXES[0],
        )
        # TODO: err properly if not integer literals
        return Plus(Minus(port.width.msb, port.width.lsb), IntConst(1))

    def replace_custom_rtl(
        self, rtl_paths: tuple[Path, ...], templates_info: dict[str, list[str]]
    ) -> None:
        """Add custom RTL files to the project.

        It will replace all files that originally exist in the project.

        Args:
            file_paths (List[Path]): List of file paths to copy.
            destination_folder (Path): The target folder where files will be copied.
        """
        rtl_path = Path(self.rtl_dir)
        assert Path.exists(rtl_path)

        custom_rtl = self._get_custom_rtl_files(rtl_paths)
        _logger.info("Adding custom RTL files to the project:")
        for file_path in custom_rtl:
            _logger.info("  %s", file_path)
        self._check_custom_rtl_format(custom_rtl, templates_info)

        for file_path in custom_rtl:
            assert file_path.is_file()

            # Determine destination path
            dest_path = rtl_path / file_path.name

            if dest_path.exists():
                _logger.info("Replacing %s with custom RTL.", file_path.name)
            else:
                _logger.info("Adding custom RTL %s.", file_path.name)

            # Copy file to destination, replacing if necessary
            shutil.copy2(file_path, dest_path)

    def get_rtl_templates_info(self) -> dict[str, list[str]]:
        """Get the template information for each task.

        Return:
            dict[str, list[str]]: A dictionary where the key is the task
            name and the value is the list of ports of the task.
        """
        return {
            task: [str(port) for port in self._tasks[task].module.ports.values()]
            for task in self.gen_templates
        }

    def _check_custom_rtl_format(
        self, rtl_paths: list[Path], templates_info: dict[str, list[str]]
    ) -> None:
        """Check if the custom RTL files are in the correct format."""
        if rtl_paths:
            _logger.info("checking custom RTL files format")
        for rtl_path in rtl_paths:
            if rtl_path.suffix != ".v":
                _logger.warning(
                    "Skip checking custom rtl format for non-verilog file: %s",
                    str(rtl_path),
                )
                continue
            try:
                rtl_module = Module([str(rtl_path)])
            except ParseError:
                msg = (
                    f"Failed to parse custom RTL file: {rtl_path!s}. "
                    "Skip port checking."
                )
                _logger.warning(msg)
                continue
            if (task := self._tasks.get(rtl_module.name)) is None:
                continue  # ignore RTL modules that are not tasks
            if {str(port) for port in rtl_module.ports.values()} == set(
                templates_info[task.name]
            ):
                continue  # ports match exactly
            msg = [
                f"Custom RTL file {rtl_path} for task {task.name}"
                " does not match the expected ports.",
                "Task ports:",
                *(f"  {port}" for port in templates_info[task.name]),
                "Custom RTL ports:",
                *(f"  {port}" for port in rtl_module.ports.values()),
            ]
            _logger.warning("\n".join(msg))

    def get_aie_graph(self, task: Task) -> str:
        """Generates the complete AIE graph code."""

        _header_decl, kernel_decl, port_decl = gen_declarations(task)
        (
            kernel_def,
            kernel_source,
            kernel_header,
            kernel_runtime,
            kernel_loc,
            port_def,
        ) = gen_definitions(task)
        connect_def = gen_connections(task)

        return self.GRAPH_HEADER_TEMPLATE.format(
            graph_name=self.top,
            kernel_decl="\n\t".join(kernel_decl),
            kernel_def="\n\t\t".join(kernel_def),
            kernel_source="\n\t\t".join(kernel_source),
            kernel_header="\n\t\t".join(kernel_header),
            kernel_runtime="\n\t\t".join(kernel_runtime),
            kernel_loc="\n\t\t".join(kernel_loc),
            port_decl="\n\t".join(port_decl),
            port_def="\n\t\t".join(port_def),
            connect_def="\n\t\t".join(connect_def),
        )

    def _find_task_inst_hierarchy(
        self,
        target_task: str,
        current_task: str,
        current_inst: str,
        current_hierarchy: tuple[str, ...],
    ) -> Generator[tuple[str, ...]]:
        """Find hierarchies of all instances of the given task name."""
        if current_task == target_task:
            yield (*current_hierarchy, current_inst)
        for inst in self._tasks[current_task].instances:
            assert inst.name
            self._find_task_inst_hierarchy(
                target_task,
                inst.task.name,
                inst.name,
                (*current_hierarchy, current_inst),
            )

    @staticmethod
    def get_inst_by_port_arg_name(
        target_task: str | None, parent_task: Task, port_arg_name: str
    ) -> Instance:
        """Get the instance of the target task that connect to the port arg name.

        If target_task is None, return the first instance that connects to the port arg
        name. If target_task is not None, return the instance that connects to the port
        arg name and is of the target task.
        """
        matched_inst = None
        for inst in parent_task.instances:
            if target_task and inst.task.name != target_task:
                continue
            for arg in inst.args:
                if arg.name == port_arg_name:
                    matched_inst = inst
                    break
        assert matched_inst is not None
        return matched_inst

    def get_grouping_constraints(
        self, nonpipeline_fifos: list[str] | None = None
    ) -> list[list[str]]:
        """Generates the grouping constraints based on critical path."""
        _logger.info("Resolving grouping constraints from non-pipeline FIFOs")

        if not nonpipeline_fifos:
            return []

        grouping_constraints = []
        for task_fifo_name in nonpipeline_fifos:
            # dfs all tasks to find all task instances
            task_name, fifo_name = tuple(task_fifo_name.split("."))
            found_hierarchies = self._find_task_inst_hierarchy(
                task_name, self.top, self.top, ()
            )
            fifo = self._tasks[task_name].fifos[fifo_name]
            assert all(direction in fifo for direction in FIFO_DIRECTIONS)
            consumer_task: str = fifo["consumed_by"][0]
            producer_task: str = fifo["produced_by"][0]
            for hierarchy in found_hierarchies:
                # find fifo producer and consumer instance names as fifo object only
                # contains task names
                producer_inst = self.get_inst_by_port_arg_name(
                    producer_task, self._tasks[task_name], fifo_name
                ).name
                consumer_inst = self.get_inst_by_port_arg_name(
                    consumer_task, self._tasks[task_name], fifo_name
                ).name

                grouping_constraints.append(
                    [
                        "/".join((*hierarchy, producer_inst)),
                        "/".join((*hierarchy, fifo_name)),
                        "/".join((*hierarchy, consumer_inst)),
                    ]
                )

        return grouping_constraints


def _redact(xo: zipfile.ZipFile, info: zipfile.ZipInfo) -> bytes:
    content = xo.read(info)
    if not info.filename.endswith(".xml"):
        return content

    # Although we only redact xml files, regex substitution seems more readable
    # than parsing and mutating xml. We'll add tests to prevent regression in
    # case of file format change.
    text = content.decode()

    # Redact the timestamp stored in xml. The same timestamp is used by files in
    # the xo zip file and is the earliest timestamp supported by the zip format.
    text = re.sub(
        r"<xilinx:coreCreationDateTime>....-..-..T..:..:..Z</xilinx:coreCreationDateTime>",
        r"<xilinx:coreCreationDateTime>1980-01-01T00:00:00Z</xilinx:coreCreationDateTime>",
        text,
    )

    # Redact the absolute path but keep the path relative to the work directory.
    text = re.sub(
        r"<SourceLocation>.*/(cpp/.*)</SourceLocation>",
        r"<SourceLocation>\1</SourceLocation>",
        text,
    )

    # Redact the random(?) project ID.
    text = re.sub(
        r'ProjectID="................................"',
        r'ProjectID="0123456789abcdef0123456789abcdef"',
        text,
    )

    return text.encode()
