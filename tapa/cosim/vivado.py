"""Generate a Vivado TCL script for cosimulation."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import re
import subprocess

from tapa.common import paths

_logger = logging.getLogger().getChild(__name__)


def get_vivado_version() -> str:
    """Return the Vivado version."""
    command = ["vivado", "-version"]
    try:
        output = subprocess.check_output(command, stderr=subprocess.STDOUT)
        version_lines = output.decode("utf-8")
        # e.g., vivado v2024.2  Vivado v2022.2
        match = re.search(r"vivado v(\d+\.\d+)", version_lines, re.IGNORECASE)
        if match is None:
            error = f"Failed to parse Vivado version from:\n{version_lines}"
            raise ValueError(error)
        version = match.group(1)
        _logger.info("Vivado version: %s", version)
        return version

    except FileNotFoundError:
        error = "Vivado not found. Please add Vivado to PATH."
        raise FileNotFoundError(error)

    except subprocess.CalledProcessError as e:
        error = f"Failed to get Vivado version: {e.output.decode('utf-8')}"
        raise ValueError(error) from e


def get_vivado_tcl(
    config: dict,
    tb_rtl_path: str,
    save_waveform: bool,
    start_gui: bool,
) -> list[str]:
    """Generate a Vivado TCL script for cosimulation."""
    dpi_version = (
        "tapa_fast_cosim_dpi_xv"
        if get_vivado_version() >= "2024.2"
        else "tapa_fast_cosim_dpi_legacy_rdi"
    )

    tapa_hdl_path = config["verilog_path"]

    script = []

    part_num = config["part_num"]

    if not part_num:
        msg = (
            "part_num is not set. Either provide an xo that contains HLS reports or "
            "use the --xosim-part-num option to specify the part number."
        )
        raise ValueError(msg)

    script.append(f"create_project -force tapa-fast-cosim ./vivado -part {part_num}")

    # read in the original RTLs by HLS
    script.append(f'set ORIG_RTL_PATH "{tapa_hdl_path}"')

    for suffix in (".v", ".sv", ".dat"):
        for loc in (f"${{ORIG_RTL_PATH}}/*{suffix}", f"${{ORIG_RTL_PATH}}/*/*{suffix}"):
            script.append(f"set rtl_files [glob -nocomplain {loc}]")
            script.append(
                'if {$rtl_files ne ""} '
                "{add_files -norecurse -scan_for_includes ${rtl_files} }"
            )

    # instantiate IPs used in the RTL. Use "-nocomplain" in case no IP is used
    for loc in (r"${ORIG_RTL_PATH}/*.tcl", r"${ORIG_RTL_PATH}/*/*.tcl"):
        script.append(f"set tcl_files [glob -nocomplain {loc}]")
        script.append(r"foreach ip_tcl ${tcl_files} { source ${ip_tcl} }")

    # instantiate IPs used in the RTL. Use "-nocomplain" in case no IP is used
    script.append(r"set xci_ip_files [glob -nocomplain ${ORIG_RTL_PATH}/*/*.xci]")
    script.append(
        'if {$xci_ip_files ne ""} '
        "{add_files -norecurse -scan_for_includes ${xci_ip_files} }"
    )
    script.append(r"set xci_ip_files [glob -nocomplain ${ORIG_RTL_PATH}/*.xci]")
    script.append(
        'if {$xci_ip_files ne ""} '
        "{add_files -norecurse -scan_for_includes ${xci_ip_files} }"
    )

    # IPs may be locked due to version mismatch
    script.append("upgrade_ip -quiet [get_ips *]")

    # read in tb files
    script.append(f"set tb_files [glob {tb_rtl_path}/*.v {tb_rtl_path}/*.sv]")
    script.append(r"set_property SOURCE_SET sources_1 [get_filesets sim_1]")
    script.append(r"add_files -fileset sim_1 -norecurse -scan_for_includes ${tb_files}")

    script.append("set_property top test [get_filesets sim_1]")
    script.append("set_property top_lib xil_defaultlib [get_filesets sim_1]")

    dpi_library_dir = paths.find_resource("tapa-fast-cosim-dpi")
    if dpi_library_dir is None:
        _logger.fatal("DPI directory not found")
    else:
        _logger.info("DPI directory: %s", dpi_library_dir)
        script.append(
            "set_property -name {xelab.more_options} "
            f"-value {{-sv_root {dpi_library_dir} -sv_lib {dpi_version}}} "
            "-objects [get_filesets sim_1]"
        )

    # log all signals
    if save_waveform or start_gui:
        script.append(
            r"set_property -name {xsim.simulate.log_all_signals} "
            r"-value {true} -objects [get_filesets sim_1]"
        )
    if save_waveform:
        script.append(
            r"set_property -name {xsim.simulate.wdb} "
            r"-value {wave.wdb} -objects [get_filesets sim_1]"
        )

    script.append(r"launch_simulation")
    script.append(r"run all")

    return script
