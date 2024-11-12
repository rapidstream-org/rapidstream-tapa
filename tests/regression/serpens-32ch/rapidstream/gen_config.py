"""Generate rapidstream configuration files."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import os
from pathlib import Path

from rapidstream import get_u280_vitis_3x3_device_factory
from rapidstream.assets.floorplan.floorplan_config import FloorplanConfig
from rapidstream.assets.utilities.impl import ImplConfig

VITIS_PLATFORM = "xilinx_u280_gen3x16_xdma_1_202211_1"
TEMP_DIR = Path(".")
FLOOR_PLAN_CONFIG = TEMP_DIR / "floorplan_config.json"
DEVICE_CONFIG = TEMP_DIR / "device_config.json"
IMPL_CONFIG = TEMP_DIR / "impl_config.json"


def gen_device_config() -> None:
    """Generate device configuration."""
    factory = get_u280_vitis_3x3_device_factory(VITIS_PLATFORM)

    # HBM 100000 LUTs and 120000 FFs on SLR0, even on half
    # DDR 25000 LUTs and 20000 FFs on SLR 0, SLR 1, even on half

    # the same config inherited from id_1, updated to 3x3 version
    factory.reduce_slot_area(2, 0, lut=80000, ff=130000)
    factory.reduce_slot_area(0, 0, lut=40000, ff=65000)
    factory.reduce_slot_area(1, 0, lut=40000, ff=65000)

    factory.reduce_slot_area(0, 1, lut=9000, ff=18000)
    factory.reduce_slot_area(1, 1, lut=9000, ff=18000)
    factory.reduce_slot_area(2, 1, dsp=100, lut=18000, ff=36000)

    factory.set_slot_wire_capacity_in_pair(0, 0, 0, 1, 7500)
    factory.set_slot_wire_capacity_in_pair(1, 0, 1, 1, 7500)
    factory.set_slot_wire_capacity_in_pair(2, 0, 2, 1, 15000)
    factory.set_slot_wire_capacity_in_pair(1, 0, 2, 0, 20000)

    factory.generate_virtual_device(DEVICE_CONFIG)


def gen_floorplan_config() -> None:
    """Generate floorplan configuration."""
    floorplan_config = FloorplanConfig(
        dse_range_max=0.8,
        dse_range_min=0.65,
        partition_strategy="flat",
        port_pre_assignments={
            "ap_clk": "SLOT_X2Y0:SLOT_X2Y0",
            "ap_rst_n": "SLOT_X2Y0:SLOT_X2Y0",
            "interrupt": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch0_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch10_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch11_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch12_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch13_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch14_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch15_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch16_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch17_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch18_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch19_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch1_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch20_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch21_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch22_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch23_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch24_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch25_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch26_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch27_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch28_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch29_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch2_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch30_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch31_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch3_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch4_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch5_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch6_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch7_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch8_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch9_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ptr_.*": "SLOT_X0Y1:SLOT_X2Y1",
            "m_axi_vec_X_.*": "SLOT_X0Y1:SLOT_X2Y1",
            "m_axi_vec_Y_(?!out_).*": "SLOT_X0Y1:SLOT_X2Y1",
            "m_axi_vec_Y_out_.*": "SLOT_X0Y0:SLOT_X2Y0",
            "s_axi_control_.*": "SLOT_X2Y1:SLOT_X2Y1",
        },
    )
    floorplan_config.save_to_file(FLOOR_PLAN_CONFIG)


def gen_impl_config() -> None:
    """Generate implementation configuration."""
    impl_config = ImplConfig(
        max_workers=1,  # to avoid too many active jobs in parallel during tests
        placement_strategy="EarlyBlockPlacement",
        port_to_clock_period={"ap_clk": 2.8},
        vitis_platform="xilinx_u280_gen3x16_xdma_1_202211_1",
    )
    impl_config.save_to_file(IMPL_CONFIG)


def gen_config() -> None:
    """Generate configuration files."""
    if not os.path.exists(TEMP_DIR):
        os.makedirs(TEMP_DIR)
    gen_device_config()
    gen_floorplan_config()
    gen_impl_config()


if __name__ == "__main__":
    gen_config()
