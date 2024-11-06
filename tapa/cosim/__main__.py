__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import argparse
import logging
import os
import os.path
import re
import subprocess
from collections import defaultdict
from collections.abc import Sequence
from pathlib import Path

from tapa import __version__
from tapa.cosim.common import AXI, Arg
from tapa.cosim.config_preprocess import preprocess_config
from tapa.cosim.templates import (
    get_axi_ram_inst,
    get_axi_ram_module,
    get_axis,
    get_begin,
    get_dut,
    get_end,
    get_s_axi_control,
    get_srl_fifo_template,
    get_test_signals,
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
    """
    parse the comments in s_axi_control.v to get the register addresses for each
    argument
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
    """
    parse the top RTL to extract all m_axi interfaces, the data width and addr width
    """
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


def get_cosim_tb(
    top_name: str,
    s_axi_control_path: str,
    axi_list: list[AXI],
    args: Sequence[Arg],
    scalar_to_val: dict[str, str],
) -> str:
    """
    generate a lightweight testbench to test the HLS RTL
    """
    arg_to_reg_addrs = parse_register_addr(s_axi_control_path)

    tb = ""
    tb += get_begin() + "\n"

    for axi in axi_list:
        tb += get_axi_ram_inst(axi) + "\n"

    tb += get_s_axi_control() + "\n"

    tb += get_axis(args) + "\n"

    tb += get_dut(top_name, args) + "\n"

    tb += get_test_signals(arg_to_reg_addrs, scalar_to_val, args)

    tb += get_end() + "\n"

    return tb


def set_default_nettype(verilog_path: str) -> None:
    """
    Sometimes the HLS-generated RTL will directly assign constants to IO ports
    But Vivado does not allow this behaviour. We need to set the `default_nettype
    to wire to bypass this issue.
    """
    _logger.info("append `default_nettype wire to every RTL file")
    for file in os.listdir(verilog_path):
        if file.endswith((".v", ".sv")):
            abs_path = os.path.join(verilog_path, file)
            with open(abs_path, "r+", encoding="utf-8") as f:
                content = f.read()
                f.seek(0, 0)
                f.write("`default_nettype wire\n" + content)


def main() -> None:  # pylint: disable=too-many-locals
    """Main entry point for the TAPA fast cosim tool."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--config_path", type=str, required=True)
    parser.add_argument("--tb_output_dir", type=str, required=True)
    parser.add_argument("--part_num", type=str, required=False)
    parser.add_argument("--launch_simulation", action="store_true")
    parser.add_argument("--save_waveform", action="store_true")
    parser.add_argument("--start_gui", action="store_true")
    parser.add_argument("--setup_only", action="store_true")
    args = parser.parse_args()

    _logger.info("TAPA fast cosim version: %s", __version__)

    config = preprocess_config(args.config_path, args.tb_output_dir, args.part_num)

    top_name = config["top_name"]
    verilog_path = config["verilog_path"]
    top_path = f"{verilog_path}/{top_name}.v"
    ctrl_path = f"{verilog_path}/{top_name}_control_s_axi.v"

    # add default nettype to all rtl
    set_default_nettype(verilog_path)

    axi_list = parse_m_axi_interfaces(top_path)
    tb = get_cosim_tb(
        top_name,
        ctrl_path,
        axi_list,
        config["args"],
        config["scalar_to_val"],
    )

    # generate test bench RTL files
    Path(args.tb_output_dir).mkdir(parents=True, exist_ok=True)
    for bin_file in Path(args.tb_output_dir).glob("*.bin"):
        bin_file.unlink()
    with open(f"{args.tb_output_dir}/tb.sv", "w", encoding="utf-8") as fp:
        fp.write(tb)
    with open(f"{args.tb_output_dir}/fifo_srl_tb.v", "w", encoding="utf-8") as fp:
        fp.write(get_srl_fifo_template())

    for axi in axi_list:
        source_data_path = config["axi_to_data_file"][axi.name]
        c_array_size = config["axi_to_c_array_size"][axi.name]
        ram_module = get_axi_ram_module(axi, source_data_path, c_array_size)
        with open(
            f"{args.tb_output_dir}/axi_ram_{axi.name}.v", "w", encoding="utf-8"
        ) as fp:
            fp.write(ram_module)

    # generate vivado script
    Path(f"{args.tb_output_dir}/run").mkdir(parents=True, exist_ok=True)
    if args.save_waveform:
        _logger.warning(
            "Waveform will be saved at %s"
            "/run/vivado/tapa-fast-cosim/tapa-fast-cosim.sim"
            "/sim_1/behav/xsim/wave.wdb",
            args.tb_output_dir,
        )
    else:
        _logger.warning(
            "Waveform is not saved. "
            "Use --save_waveform to save the simulation waveform."
        )

    vivado_script = get_vivado_tcl(config, args.tb_output_dir, args.save_waveform)

    with open(f"{args.tb_output_dir}/run/run_cosim.tcl", "w", encoding="utf-8") as fp:
        fp.write("\n".join(vivado_script))

    if args.setup_only:
        _logger.info("User requested to only setup the cosim environment, exiting...")
        return

    # launch simulation
    mode = "gui" if args.start_gui else "batch"
    command = ["vivado", "-mode", mode, "-source", "run_cosim.tcl"]
    if args.launch_simulation:
        _logger.info("Running vivado command: %s", command)
        subprocess.run(
            command,
            cwd=Path(f"{args.tb_output_dir}/run").resolve(),
            check=True,
            env=os.environ
            | {
                "TAPA_FAST_COSIM_DPI_ARGS": ",".join(
                    f"{k}:{v}" for k, v in config["axis_to_data_file"].items()
                )
            },
        )


if __name__ == "__main__":
    main()
