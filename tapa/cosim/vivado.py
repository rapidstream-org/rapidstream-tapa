"""Generate a Vivado TCL script for cosimulation."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


def get_vivado_tcl(
    config: dict,
    tb_rtl_path: str,
    save_waveform: bool,
) -> list[str]:
    """Generate a Vivado TCL script for cosimulation."""
    tapa_hdl_path = config["verilog_path"]

    script = []

    part_num = config["part_num"]
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
    script.append(f"set tb_files [glob {tb_rtl_path}/*.v]")
    script.append(r"set_property SOURCE_SET sources_1 [get_filesets sim_1]")
    script.append(r"add_files -fileset sim_1 -norecurse -scan_for_includes ${tb_files}")

    script.append("set_property top test [get_filesets sim_1]")
    script.append("set_property top_lib xil_defaultlib [get_filesets sim_1]")

    # log all signals
    if save_waveform:
        script.append(
            r"set_property -name {xsim.simulate.log_all_signals} "
            r"-value {true} -objects [get_filesets sim_1]"
        )
        script.append(
            r"set_property -name {xsim.simulate.wdb} "
            r"-value {wave.wdb} -objects [get_filesets sim_1]"
        )

    script.append(r"launch_simulation")
    script.append(r"run all")
    script.append(r"exit")

    return script
