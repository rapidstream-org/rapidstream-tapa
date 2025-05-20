__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import os
import os.path
import re
import signal
import subprocess
import sys
from collections import defaultdict
from collections.abc import Sequence
from contextlib import suppress
from io import TextIOWrapper
from pathlib import Path

import click
import psutil

from tapa import __version__
from tapa.cosim.common import AXI, Arg
from tapa.cosim.config_preprocess import preprocess_config
from tapa.cosim.templates import (
    get_axi_ram_inst,
    get_axi_ram_module,
    get_axis,
    get_begin,
    get_end,
    get_fifo,
    get_hls_dut,
    get_hls_test_signals,
    get_s_axi_control,
    get_srl_fifo_template,
    get_vitis_dut,
    get_vitis_test_signals,
)
from tapa.cosim.vivado import get_vivado_tcl

[logging.root.removeHandler(handler) for handler in logging.root.handlers]
logging.basicConfig(
    level=logging.INFO,
    format="[%(levelname)s:%(name)s:%(lineno)d] %(message)s",
    datefmt="%m%d %H:%M:%S",
)

_logger = logging.getLogger().getChild(__name__)


def parse_register_addr(ctrl_unit_path: str) -> dict[str, list[str]]:
    """Parses register addresses from the given s_axi_control.v file.

    Parses the comments in s_axi_control.v to get the register addresses for each
    argument.
    """
    with open(ctrl_unit_path, encoding="utf-8") as fp:
        ctrl_unit = fp.readlines()
    comments = [line for line in ctrl_unit if line.strip().startswith("//")]

    arg_to_reg_addrs = defaultdict(list)
    for line in comments:
        if " 0x" in line and "Data signal" in line:
            match = re.search(r"(0x\w+) : Data signal of (\w+)", line)
            if match is None:
                continue
            signal = match.group(2)
            addr = match.group(1)
            arg_to_reg_addrs[signal].append(addr.replace("0x", "'h"))

    return dict(arg_to_reg_addrs)


def parse_m_axi_interfaces(top_rtl_path: str) -> list[AXI]:
    """Parse the top RTL to extract all m_axi interface metadata."""
    with open(top_rtl_path, encoding="utf-8") as fp:
        top_rtl = fp.read()

    match_addr = re.findall(
        r"output\s+(?:wire\s+)?\[(.*):\s*0\s*\]\s+m_axi_(\w+)_ARADDR\s*[;,]", top_rtl
    )
    match_data = re.findall(
        r"output\s+(?:wire\s+)?\[(.*):\s*0\s*\]\s+m_axi_(\w+)_WDATA\s*[;,]", top_rtl
    )

    # the width may contain parameters
    params = re.findall(r"parameter\s+(\S+)\s*=\s+(\S+)\s*;", top_rtl)
    param_to_value = dict(params)

    axi_list = []
    name_to_addr_width = {m_axi: addr_width for addr_width, m_axi in match_addr}
    for data_width, m_axi in match_data:
        addr_width = name_to_addr_width[m_axi]

        # substitute the parameters
        for name, val in param_to_value.items():
            data_width = data_width.replace(name, val)  # noqa: PLW2901
            addr_width = addr_width.replace(name, val)

        axi_list.append(AXI(m_axi, eval(data_width) + 1, eval(addr_width) + 1))
    return axi_list


def get_cosim_tb(  # noqa: PLR0913,PLR0917
    top_name: str,
    top_is_leaf_task: bool,
    s_axi_control_path: str,
    axi_list: list[AXI],
    args: Sequence[Arg],
    scalar_to_val: dict[str, str],
    mode: str,
) -> str:
    """Generate a lightweight testbench to test the HLS RTL."""
    tb = get_begin() + "\n"

    for axi in axi_list:
        tb += get_axi_ram_inst(axi) + "\n"

    if mode == "vitis":
        arg_to_reg_addrs = parse_register_addr(s_axi_control_path)
        tb += get_s_axi_control() + "\n"
        tb += get_axis(args) + "\n"
        tb += get_vitis_dut(top_name, args) + "\n"
        tb += get_vitis_test_signals(arg_to_reg_addrs, scalar_to_val, args)
    else:
        tb += get_fifo(args) + "\n"
        tb += get_hls_dut(top_name, top_is_leaf_task, args, scalar_to_val) + "\n"
        tb += get_hls_test_signals(args)

    tb += get_end() + "\n"

    return tb


def set_default_nettype(verilog_path: str) -> None:
    """Appends `default_nettype` to Verilog files in the given directory.

    Sometimes the HLS-generated RTL will directly assign constants to IO ports But
    Vivado does not allow this behaviour. We need to set the `default_nettype to wire to
    bypass this issue.
    """
    _logger.debug("appending `default_nettype wire to every RTL file")
    for file in os.listdir(verilog_path):
        if file.endswith((".v", ".sv")):
            abs_path = os.path.join(verilog_path, file)
            with open(abs_path, "r+", encoding="utf-8") as f:
                content = f.read()
                f.seek(0, 0)
                f.write("`default_nettype wire\n" + content)


@click.command("cosim")
@click.option("--config-path", type=str, required=True)
@click.option("--tb-output-dir", type=str, required=True)
@click.option("--part-num", type=str)
@click.option("--launch-simulation / --no-launch-simulation", type=bool, default=False)
@click.option("--save-waveform / --no-save-waveform", type=bool, default=False)
@click.option("--start-gui / --no-start-gui", type=bool, default=False)
def main(  # noqa: PLR0913, PLR0917
    config_path: str,
    tb_output_dir: str,
    part_num: str | None,
    launch_simulation: bool,
    save_waveform: bool,
    start_gui: bool,
) -> None:
    """Main entry point for the TAPA fast cosim tool."""
    _logger.info("TAPA fast cosim version: %s", __version__)

    _logger.debug("   Loading configuration from %s", config_path)
    _logger.debug("   Generating testbench in %s", tb_output_dir)
    _logger.debug("   Part number: %s", part_num)
    _logger.debug("   Launch simulation: %s", launch_simulation)
    _logger.debug("   Save waveform: %s", save_waveform)
    _logger.debug("   Start GUI: %s", start_gui)

    config = preprocess_config(config_path, tb_output_dir, part_num)

    top_name = config["top_name"]
    verilog_path = config["verilog_path"]
    top_path = f"{verilog_path}/{top_name}.v"
    ctrl_path = f"{verilog_path}/{top_name}_control_s_axi.v"

    # add default nettype to all rtl
    set_default_nettype(verilog_path)

    axi_list = parse_m_axi_interfaces(top_path)
    tb = get_cosim_tb(
        top_name,
        config["top_is_leaf_task"],
        ctrl_path,
        axi_list,
        config["args"],
        config["scalar_to_val"],
        config["mode"],
    )

    # generate test bench RTL files
    Path(tb_output_dir).mkdir(parents=True, exist_ok=True)
    for bin_file in Path(tb_output_dir).glob("*.bin"):
        bin_file.unlink()
    with open(f"{tb_output_dir}/tb.sv", "w", encoding="utf-8") as fp:
        fp.write(tb)
    with open(f"{tb_output_dir}/fifo_srl_tb.v", "w", encoding="utf-8") as fp:
        fp.write(get_srl_fifo_template())

    for axi in axi_list:
        source_data_path = config["axi_to_data_file"][axi.name]
        c_array_size = config["axi_to_c_array_size"][axi.name]
        ram_module = get_axi_ram_module(axi, source_data_path, c_array_size)
        with open(f"{tb_output_dir}/axi_ram_{axi.name}.v", "w", encoding="utf-8") as fp:
            fp.write(ram_module)

    # generate vivado script
    Path(f"{tb_output_dir}/run").mkdir(parents=True, exist_ok=True)
    if save_waveform:
        _logger.warning(
            "Waveform will be saved at %s"
            "/run/vivado/tapa-fast-cosim/tapa-fast-cosim.sim"
            "/sim_1/behav/xsim/wave.wdb",
            tb_output_dir,
        )

    vivado_script = get_vivado_tcl(
        config,
        tb_output_dir,
        save_waveform,
        start_gui,
    )

    with open(f"{tb_output_dir}/run/run_cosim.tcl", "w", encoding="utf-8") as fp:
        fp.write("\n".join(vivado_script))

    if launch_simulation:
        with (
            open(
                f"{tb_output_dir}/cosim.stdout.log", "w", encoding="utf-8"
            ) as stdout_fp,
            open(
                f"{tb_output_dir}/cosim.stderr.log", "w", encoding="utf-8"
            ) as stderr_fp,
        ):
            _launch_simulation(config, start_gui, tb_output_dir, stdout_fp, stderr_fp)


def _launch_simulation(
    config: dict,
    start_gui: bool,
    tb_output_dir: str,
    stdout_fp: TextIOWrapper,
    stderr_fp: TextIOWrapper,
) -> None:
    mode = "gui" if start_gui else "batch"
    command = ["vivado", "-mode", mode, "-source", "run_cosim.tcl"]

    _logger.info("Starting Vivado:")
    _logger.info("   Command: %s", " ".join(command))
    _logger.info("   Working directory: %s/run", tb_output_dir)
    _logger.info("   Stdout: %s/cosim.stdout.log", tb_output_dir)
    _logger.info("   Stderr: %s/cosim.stderr.log", tb_output_dir)

    with subprocess.Popen(
        command,
        cwd=Path(f"{tb_output_dir}/run").resolve(),
        env=os.environ
        | {
            # Vivado generates garbage files in the user home directory.
            # We ask Vivado to dump garbages to the current directory instead
            # of the user home to avoid collisions.
            "HOME": Path(f"{tb_output_dir}/run").resolve().as_posix(),
            "TAPA_FAST_COSIM_DPI_ARGS": ",".join(
                f"{k}:{v}" for k, v in config["axis_to_data_file"].items()
            ),
        },
        stdout=stdout_fp,
        stderr=stderr_fp,
    ) as process:

        def kill_vivado_tree() -> None:
            """Kill the Vivado process and its children."""
            _logger.info("Killing Vivado process and its children")
            for child in psutil.Process(process.pid).children(recursive=True):
                with suppress(psutil.NoSuchProcess):
                    child.kill()
            with suppress(psutil.NoSuchProcess):
                process.kill()

        signal.signal(signal.SIGINT, lambda _s, _f: kill_vivado_tree())
        signal.signal(signal.SIGTERM, lambda _s, _f: kill_vivado_tree())

        process.wait()
        if process.returncode != 0:
            _logger.error(
                "Vivado simulation failed with error code %d", process.returncode
            )
            sys.exit(process.returncode)
        else:
            _logger.info("Vivado simulation finished successfully")


if __name__ == "__main__":
    main()
