"""Generate rapidstream configuration files."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import os
from pathlib import Path

from rapidstream import get_u250_vitis_device_factory
from rapidstream.assets.floorplan.floorplan_config import FloorplanConfig
from rapidstream.assets.utilities.impl import ImplConfig

VITIS_PLATFORM = "xilinx_u250_gen3x16_xdma_4_1_202210_1"
TEMP_DIR = Path(".")
FLOOR_PLAN_CONFIG = TEMP_DIR / "floorplan_config.json"
DEVICE_CONFIG = TEMP_DIR / "device_config.json"
IMPL_CONFIG = TEMP_DIR / "impl_config.json"


def gen_device_config() -> None:
    """Generate device configuration."""
    factory = get_u250_vitis_device_factory(VITIS_PLATFORM)

    # Reduced because of DDR
    factory.reduce_slot_area(1, 0, lut=30000, ff=60000)
    factory.reduce_slot_area(1, 2, lut=30000, ff=60000)
    factory.reduce_slot_area(1, 3, lut=30000, ff=60000)

    # Reduced because of congestion
    factory.reduce_slot_area(0, 1, lut=10000, ff=20000)
    factory.reduce_slot_area(0, 2, lut=10000, ff=20000)
    factory.reduce_slot_area(0, 3, lut=5000, ff=10000)

    factory.generate_virtual_device(DEVICE_CONFIG)


def gen_floorplan_config() -> None:
    """Generate floorplan configuration."""
    floorplan_config = FloorplanConfig(
        dse_range_max=0.8,
        dse_range_min=0.65,
        max_seconds=1200,
        partition_strategy="flat",
        port_pre_assignments={
            "ap_clk": "SLOT_X1Y0:SLOT_X1Y0",
            "ap_rst_n": "SLOT_X1Y0:SLOT_X1Y0",
            "interrupt": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_A_.*": "SLOT_X1Y0:SLOT_X1Y0",
            "m_axi_B_.*": "SLOT_X1Y2:SLOT_X1Y2",
            "m_axi_C_.*": "SLOT_X1Y3:SLOT_X1Y3",
            "s_axi_control_.*": "SLOT_X1Y0:SLOT_X1Y0",
        },
        slot_to_rtype_to_min_limit={
            "SLOT_X0Y0:SLOT_X0Y0": {"LUT": 0.5},
            "SLOT_X0Y1:SLOT_X0Y1": {"LUT": 0.5},
            "SLOT_X0Y2:SLOT_X0Y2": {"LUT": 0.5},
            "SLOT_X0Y3:SLOT_X0Y3": {"LUT": 0.5},
            "SLOT_X1Y0:SLOT_X1Y0": {"LUT": 0.5},
            "SLOT_X1Y2:SLOT_X1Y2": {"LUT": 0.5},
            "SLOT_X1Y3:SLOT_X1Y3": {"LUT": 0.5},
        },
    )
    floorplan_config.save_to_file(FLOOR_PLAN_CONFIG)


def gen_impl_config() -> None:
    """Generate implementation configuration."""
    impl_config = ImplConfig(
        max_workers=1,  # to avoid too many active jobs in parallel during tests
        placement_strategy="EarlyBlockPlacement",
        port_to_clock_period={"ap_clk": 2.8},
        vitis_platform="xilinx_u250_gen3x16_xdma_4_1_202210_1",
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
