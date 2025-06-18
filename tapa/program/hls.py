__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import itertools
import logging
import os
import os.path
import sys
from concurrent import futures
from typing import Literal

from psutil import cpu_count

from tapa.backend.xilinx import RunAie, RunHls
from tapa.common.paths import find_resource, get_xpfm_path
from tapa.program.abc import ProgramInterface
from tapa.program.directory import ProgramDirectoryInterface
from tapa.safety_check import check_mmap_arg_name
from tapa.task import Task
from tapa.util import clang_format

_logger = logging.getLogger().getChild(__name__)

# _AIE_DEPTH is the number of elements in the AIE windows.
_AIE_DEPTH = 64

_GRAPH_HEADER_TEMPLATE = r"""
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

_GRAPH_CPP_TEMPLATE = r"""
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


class ProgramHlsMixin(
    ProgramDirectoryInterface,
    ProgramInterface,
):
    """Mixin class providing HLS (AIE included) functionalities."""

    top: str
    cflags: str
    _tasks: dict[str, Task]

    def _extract_cpp(self, target: Literal["aie", "hls"]) -> None:
        """Extract HLS/AIE C++ files."""
        _logger.info("extracting %s C++ files", target)
        check_mmap_arg_name(list(self._tasks.values()))

        top_aie_task_is_done = False
        for task in self._tasks.values():
            if task.name == self.top and target == "aie":
                assert not top_aie_task_is_done, (
                    "There should be exactly one top-level task"
                )
                top_aie_task_is_done = True
                code_content = self._get_aie_graph(task)
                with open(
                    self.get_header_path(task.name), "w", encoding="utf-8"
                ) as src_code:
                    src_code.write(code_content)
                with open(
                    self.get_cpp_path(task.name), "w", encoding="utf-8"
                ) as src_code:
                    src_code.write(_GRAPH_CPP_TEMPLATE.format(top_task_name=self.top))
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

    def _is_skippable_based_on_mtime(self, task_name: str) -> bool:
        try:
            tar_path = self.get_tar_path(task_name)
            cpp_path = self.get_cpp_path(task_name)
            if os.path.getmtime(tar_path) > os.path.getmtime(cpp_path):
                _logger.info(
                    "skipping HLS for %s since %s is newer than %s",
                    task_name,
                    tar_path,
                    cpp_path,
                )
                return True
        except OSError:
            pass
        return False

    def run_hls(  # noqa: PLR0913, PLR0917
        self,
        clock_period: float | str,
        part_num: str,
        skip_based_on_mtime: bool,
        other_configs: str,
        jobs: int | None,
        keep_hls_work_dir: bool,
    ) -> None:
        """Run HLS with extracted HLS C++ files and generate tarballs."""
        self._extract_cpp("hls")

        _logger.info("running hls")
        work_dir = os.path.join(self.work_dir, "hls") if keep_hls_work_dir else None

        def worker(task: Task, idx: int) -> None:
            _logger.info("start worker for %s, target: %s", task.name, task.target_type)
            os.nice(idx % 19)
            if skip_based_on_mtime and self._is_skippable_based_on_mtime(task.name):
                return
            hls_defines = "-DTAPA_TARGET_DEVICE_ -DTAPA_TARGET_XILINX_HLS_"
            # WORKAROUND: Vitis HLS requires -I or gflags cannot be found...
            hls_includes = f"-I{find_resource('tapa-extra-runtime-include')}"
            hls_cflags = f"{self.cflags} {hls_defines} {hls_includes}"
            with (
                open(self.get_tar_path(task.name), "wb") as tarfileobj,
                RunHls(
                    tarfileobj,
                    kernel_files=[(self.get_cpp_path(task.name), hls_cflags)],
                    work_dir=work_dir,
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

        jobs = jobs or cpu_count(logical=False)
        _logger.info("spawn %d workers for parallel HLS synthesis of the tasks", jobs)

        try:
            with futures.ThreadPoolExecutor(max_workers=jobs) as executor:
                any(executor.map(worker, self._tasks.values(), itertools.count(0)))
        except RuntimeError:
            if keep_hls_work_dir:
                _logger.error(
                    "HLS failed, see above for details. You may use "
                    "`--keep-hls-work-dir` to keep the HLS work directory "
                    "for debugging."
                )
            else:
                _logger.error(
                    "HLS failed, see above for details. Please check the logs in %s",
                    work_dir,
                )
            sys.exit(1)

    def run_aie(
        self,
        clock_period: float | str,
        skip_based_on_mtime: bool,
        keep_hls_work_dir: bool,
        platform: str,
    ) -> None:
        """Run HLS with extracted HLS C++ files and generate tarballs."""
        self._extract_cpp("aie")

        _logger.info("running aie")
        work_dir = os.path.join(self.work_dir, "aie") if keep_hls_work_dir else None

        # For AIE flow, only the top-level task is synthesized
        task = self.top_task

        if skip_based_on_mtime and self._is_skippable_based_on_mtime(task.name):
            return
        with (
            open(self.get_tar_path(task.name), "wb") as tarfileobj,
            RunAie(
                tarfileobj,
                kernel_files=[self.get_cpp_path(task.name)],
                work_dir=work_dir,
                top_name=task.name,
                clock_period=str(clock_period),
                xpfm=get_xpfm_path(platform),
            ) as proc,
        ):
            stdout, stderr = proc.communicate()

        if proc.returncode != 0:
            sys.stdout.write(stdout.decode("utf-8"))
            sys.stderr.write(stderr.decode("utf-8"))

            # Neglect the dummy bug message from AIE 2022.2
            aie_dummy_bug_msg = "/bin/sh: 1: [[: not found"
            if aie_dummy_bug_msg in stderr.decode("utf-8"):
                return

            if work_dir is None:
                _logger.error(
                    "HLS failed, see above for details. You may use "
                    "`--keep-hls-work-dir` to keep the HLS work directory "
                    "for debugging."
                )
            else:
                _logger.error(
                    "HLS failed, see above for details. Please check the logs in %s",
                    work_dir,
                )
            sys.exit(1)

    def _get_aie_graph(self, task: Task) -> str:
        """Generates the complete AIE graph code."""
        _header_decl, kernel_decl, port_decl = _gen_declarations(task)
        (
            kernel_def,
            kernel_source,
            kernel_header,
            kernel_runtime,
            kernel_loc,
            port_def,
        ) = _gen_definitions(task)
        connect_def = _gen_connections(task)

        return _GRAPH_HEADER_TEMPLATE.format(
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


def _gen_declarations(task: Task) -> tuple[list[str], list[str], list[str]]:
    """Generates kernel and port declarations."""
    port_decl = [
        (
            f"input_plio p_{port.name};"
            if port.is_immap or port.is_istream
            else f"output_plio p_{port.name};"
        )
        for port in task.ports.values()
    ]
    kernel_decl = [
        f"kernel k_{name}{i};"
        for name, insts in task.tasks.items()
        for i in range(len(insts))
    ]
    header_decl = [f'#include "{name}.h"' for name in task.tasks]
    return header_decl, kernel_decl, port_decl


def _gen_definitions(task: Task) -> tuple[list[str], ...]:
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
        (
            f'p_{port.name} = input_plio::create("{port.name}",'
            f' plio_{port.width}_bits, "{port.name}.txt");'
            if port.is_immap or port.is_istream
            else f'p_{port.name} = output_plio::create("{port.name}",'
            f' plio_{port.width}_bits, "{port.name}.txt");'
        )
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


def _gen_connections(task: Task) -> list[str]:  # noqa: C901 PLR0912
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
        window_default_size = int(width) // 8 * _AIE_DEPTH
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
