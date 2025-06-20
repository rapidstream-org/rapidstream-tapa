"""Analyze TAPA program and store the program description."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import hashlib
import json
import logging
import os
import re
import shutil
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

import click

from tapa.common.graph import Graph as TapaGraph
from tapa.common.paths import find_resource, get_system_cflags, get_tapacc_cflags
from tapa.common.target import Target
from tapa.common.task_definition import TaskDefinition
from tapa.core import Program
from tapa.steps.common import (
    get_work_dir,
    is_pipelined,
    store_persistent_context,
    store_tapa_program,
)
from tapa.util import clang_format

_logger = logging.getLogger().getChild(__name__)


@click.command()
@click.option(
    "input_files",
    "--input",
    "-f",
    required=True,
    multiple=True,
    type=click.Path(dir_okay=False, readable=True, exists=True),
    default=(),
    help="Input file, usually TAPA C++ source code.",
)
@click.option(
    "--top",
    "-t",
    metavar="TASK",
    required=True,
    type=str,
    help="Name of the top-level task.",
)
@click.option(
    "--cflags",
    "-c",
    multiple=True,
    type=str,
    default=(),
    help="Compiler flags for the kernel, may appear many times.",
)
@click.option(
    "--flatten-hierarchy / --keep-hierarchy",
    type=bool,
    default=False,
    help=(
        "`--keep-hierarchy` (default) will generate RTL with the "
        "same hierarchy as the TAPA C++ source code; "
        "`--flatten-hierarchy` will flatten the hierarchy with all "
        "leaf-level tasks instantiated in the top module"
    ),
)
@click.option(
    "--target",
    type=click.Choice([t.value for t in Target]),
    help="Target flow of TAPA compiler, default to `xilinx-vitis`.",
    default=Target.XILINX_VITIS.value,
)
def analyze(
    input_files: tuple[str, ...],
    top: str,
    cflags: tuple[str, ...],
    flatten_hierarchy: bool,
    target: str,
) -> None:
    """Analyze TAPA program and store the program description."""
    tapacc = find_clang_binary("tapacc-binary")
    tapa_cpp = find_clang_binary("tapa-cpp-binary")

    work_dir = get_work_dir()
    # Vitis HLS only supports until C++14. If the flag sets to C++17, gcc's
    # header files will use C++17 features, such as type inference in
    # template argument deduction, which is not supported by Vitis HLS.
    cflags += ("-std=c++14",)

    all_cflags = cflags + get_tapacc_cflags() + get_system_cflags()
    flatten_files = run_flatten(
        tapa_cpp,
        input_files,
        all_cflags,
        work_dir,
    )
    graph_dict = run_tapacc(
        tapacc,
        flatten_files,
        top,
        all_cflags,
        target,
    )
    graph_dict["cflags"] = cflags

    # Flatten the graph if flatten_hierarchy is set
    tapa_graph = TapaGraph(None, graph_dict)
    if flatten_hierarchy:
        tapa_graph = tapa_graph.get_flatten_graph()

    # If the top task is a leaf task, Vitis mode will not work.
    if (
        tapa_graph.get_top_task_def().get_level() == TaskDefinition.Level.LEAF
        and target == Target.XILINX_VITIS.name
    ):
        msg = "The top task is a leaf task, target `xilinx-vitis` is not supported."
        raise click.UsageError(msg)

    fp_slots = []
    graph_dict = tapa_graph.to_dict()
    store_tapa_program(
        Program(
            obj=graph_dict,
            target=target,
            work_dir=work_dir,
            floorplan_slots=fp_slots,
            flattened=flatten_hierarchy,
        )
    )

    store_persistent_context("graph", graph_dict)
    store_persistent_context("settings", {"target": target})

    is_pipelined("analyze", True)


def get_slot_to_inst(floorplan_path: Path, graph: TapaGraph) -> dict[str, list[str]]:
    """Get slot to instance mapping from floorplan file."""
    with open(floorplan_path, encoding="utf-8") as f:
        vertex_to_region = json.load(f)
    slot_to_insts = defaultdict(list)
    task_inst_names = [
        inst.name for inst in graph.get_top_task_inst().get_subtasks_insts()
    ]
    for vertex, region in vertex_to_region.items():
        if vertex not in task_inst_names:
            continue
        slot_name = "_".join(region.split(":"))
        slot_to_insts[slot_name].append(vertex)

    return slot_to_insts


def find_clang_binary(name: str) -> str:
    """Find executable from PATH if not overridden.

    From PATH and user `override` value, look for a clang-based executable
    `name` and then verify if that is an executable binary.

    Args:
      name: The name of the binary.
      override: A user specified path of the binary, or None

    Returns:
      Verified binary path.

    Raises:
      ValueError: If the binary is not found.
    """
    # Lookup binary from the distribution
    binary = find_resource(name)

    # Lookup binary from PATH
    if not binary:
        path_str = shutil.which(name)
        if path_str is not None:
            binary = Path(path_str)

    if binary is None or not binary.exists():
        msg = f"Cannot find `{name}` in PATH."
        raise ValueError(msg)

    # Check if the binary is working
    version = subprocess.check_output([binary, "--version"], universal_newlines=True)
    match = re.compile(R"version (\d+)(\.\d+)*").search(version)
    if match is None:
        msg = f"Failed to parse output: {version}"
        raise ValueError(msg)

    return str(binary.resolve())


def run_and_check(cmd: tuple[str, ...]) -> str:
    """Run command and check return code.

    Args:
      cmd: The command to execute.

    Returns:
      Stdout of the command execution.
    """
    proc = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        text=True,
        check=False,
        encoding="utf-8",
    )
    if proc.returncode != 0:
        quoted_cmd = " ".join(f'"{arg}"' if " " in arg else arg for arg in cmd)
        _logger.error(
            "command %s failed with exit code %d",
            quoted_cmd,
            proc.returncode,
        )
        sys.exit(proc.returncode)

    return proc.stdout


def run_flatten(
    tapa_cpp: str,
    files: tuple[str, ...],
    cflags: tuple[str, ...],
    work_dir: str,
) -> tuple[str, ...]:
    """Flatten input files.

    Preprocess input C/C++ files so that all macros are expanded, and all
    header files, excluding system and TAPA header files, are inlined.

    Args:
      tapa_cpp: The path of the tapa-clang binary.
      files: C/C++ files to flatten.
      cflags: User specified CFLAGS.
      work_dir: Working directory of TAPA, for output of the flatten files.

    Returns:
      Tuple of the flattened output files.
    """
    flatten_folder = os.path.join(work_dir, "flatten")
    os.makedirs(flatten_folder, exist_ok=True)
    flatten_files = []

    for file in files:
        # Generate hash-based file name for flattened files
        hash_val = hashlib.sha256()
        hash_val.update(os.path.abspath(file).encode())
        flatten_name = (
            "flatten-" + hash_val.hexdigest()[:8] + "-" + os.path.basename(file)
        )
        flatten_path = os.path.join(flatten_folder, flatten_name)
        flatten_files.append(flatten_path)

        # Output flatten code to the file
        with open(flatten_path, "w", encoding="utf-8") as output_fp:
            tapa_cpp_cmd = (
                tapa_cpp,
                "-x",
                "c++",
                "-E",
                "-CC",
                "-P",
                "-fkeep-system-includes",
                # FIXME: If we don't define __SYNTHESIS__, the generated code
                #        may not be synthesizable if the user depends on this
                #        synthesis-specific macros, as the macros will be
                #        expanded by clang cpp.
                "-D__SYNTHESIS__",
                "-DAESL_SYN",
                "-DAP_AUTOCC",
                "-DTAPA_TARGET_DEVICE_",
                "-DTAPA_TARGET_STUB_",
                *cflags,
                file,
            )
            flatten_code = run_and_check(tapa_cpp_cmd)
            formated_code = clang_format(flatten_code)
            output_fp.write(formated_code)

    return tuple(flatten_files)


def run_tapacc(
    tapacc: str,
    files: tuple[str, ...],
    top: str,
    cflags: tuple[str, ...],
    target: str,
) -> dict:
    """Execute tapacc and return the program description.

    Args:
      tapacc: The path of the tapacc binary.
      files: C/C++ files to flatten.
      top: Top task name.
      cflags: User specified CFLAGS with TAPA specific headers.
      target: Target flow of TAPA compiler, e.g., `xilinx-vitis`.

    Returns:
      Output description of the TAPA program.
    """
    tapacc_args = (
        "-top",
        top,
        "--target",
        target,
        "--",
        *cflags,
        "-DTAPA_TARGET_DEVICE_",
        "-DTAPA_TARGET_STUB_",
    )
    tapacc_cmd = (tapacc, *files, *tapacc_args)
    quoted_cmd = " ".join(f'"{arg}"' if " " in arg else arg for arg in tapacc_cmd)
    _logger.info("running tapacc command: %s", quoted_cmd)

    return json.loads(run_and_check(tapacc_cmd))
