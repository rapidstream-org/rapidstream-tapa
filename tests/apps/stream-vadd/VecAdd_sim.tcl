# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

set work_dir $argv
set proj_name "vecadd_proj"
set proj_dir "$work_dir/vivado_proj"
set source_dir "$work_dir/hdl"
set log_file "$proj_dir/$proj_name.sim/sim_1/behav/xsim/simulate.log"
set script_dir [file dirname [file normalize [info script]]]

create_project -force $proj_name $proj_dir

# Source all IP TCL files
set ip_tcl_files [glob -nocomplain "$source_dir/*.tcl"]
foreach tcl_file $ip_tcl_files {
  puts "Sourcing IP TCL file: $tcl_file"
  source $tcl_file
}

# Add all Verilog files
set v_files [glob -nocomplain "$source_dir/*.v"]
foreach v_file $v_files {
  puts "Compiling Verilog file: $v_file"
  add_files -norecurse -fileset sim_1 $v_file
}

# Add the testbench
add_files -norecurse -fileset sim_1 $script_dir/VecAdd_tb.sv

# Set the top module for simulation
set_property top VecAdd_tb [get_filesets sim_1]
set_property top_lib xil_defaultlib [get_filesets sim_1]

# Launch simulation
launch_simulation
run -all
close_sim

# Check for fatal errors in the simulation log
set fp [open $log_file r]
set file_data [read $fp]
close $fp

if {[string match "*Fatal*" $file_data]} {
  puts "Detected fatal error in simulation log."
  exit 1
} else {
  exit 0
}
