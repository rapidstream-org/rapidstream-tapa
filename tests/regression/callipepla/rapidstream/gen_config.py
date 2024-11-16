"""Generate rapidstream configuration files."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import os
from pathlib import Path

from rapidstream import get_u55c_vitis_3x3_device_factory
from rapidstream.assets.floorplan.floorplan_config import FloorplanConfig
from rapidstream.assets.utilities.impl import ImplConfig

VITIS_PLATFORM = "xilinx_u55c_gen3x16_xdma_3_202210_1"
TEMP_DIR = Path(".")
FLOOR_PLAN_CONFIG = TEMP_DIR / "floorplan_config.json"
DEVICE_CONFIG = TEMP_DIR / "device_config.json"
IMPL_CONFIG = TEMP_DIR / "impl_config.json"


def gen_device_config() -> None:
    """Generate device configuration."""
    factory = get_u55c_vitis_3x3_device_factory(VITIS_PLATFORM)

    # reduce some budget due to HBM
    factory.reduce_slot_area(2, 0, lut=50000, ff=60000)
    factory.reduce_slot_area(1, 0, lut=25000, ff=30000)
    factory.reduce_slot_area(0, 0, lut=25000, ff=30000)

    # reduce the right-most column of DSP from 3X3Slot(2,1)
    factory.reduce_slot_area(2, 1, dsp=100)

    factory.generate_virtual_device(DEVICE_CONFIG)


def gen_floorplan_config() -> None:
    """Generate floorplan configuration."""
    floorplan_config = FloorplanConfig(
        dse_range_max=0.6,
        dse_range_min=0.45,
        partition_strategy="flat",
        port_pre_assignments={
            "ap_clk": "SLOT_X2Y0:SLOT_X2Y0",
            "ap_rst_n": "SLOT_X2Y0:SLOT_X2Y0",
            "interrupt": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_edge_list_ch_0_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch_10_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch_11_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch_12_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch_13_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch_14_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch_15_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch_1_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch_2_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch_3_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch_4_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch_5_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch_6_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch_7_.*": "SLOT_X0Y0:SLOT_X0Y0",
            "m_axi_edge_list_ch_8_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ch_9_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_edge_list_ptr_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_vec_Ap_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_vec_digA_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_vec_p_0_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_vec_p_1_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_vec_r_0_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_vec_r_1_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_vec_res_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_vec_x_0_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "m_axi_vec_x_1_.*": "SLOT_X2Y0:SLOT_X2Y0",
            "s_axi_control_.*": "SLOT_X2Y1:SLOT_X2Y1",
        },
        slot_to_rtype_to_min_limit={
            "SLOT_X0Y0:SLOT_X0Y0": {"LUT": 0.5},
            "SLOT_X0Y1:SLOT_X0Y1": {"LUT": 0.35},
            "SLOT_X0Y2:SLOT_X0Y2": {"LUT": 0.3},
            "SLOT_X1Y2:SLOT_X1Y2": {"LUT": 0.35},
            "SLOT_X2Y2:SLOT_X2Y2": {"LUT": 0.4},
        },
    )
    floorplan_config.save_to_file(FLOOR_PLAN_CONFIG)


def gen_impl_config() -> None:
    """Generate implementation configuration."""
    impl_config = ImplConfig(
        max_workers=1,  # to avoid too many active jobs in parallel during tests
        placement_strategy="EarlyBlockPlacement",
        port_to_clock_period={"ap_clk": 2.7},
        vitis_platform="xilinx_u55c_gen3x16_xdma_3_202210_1",
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
