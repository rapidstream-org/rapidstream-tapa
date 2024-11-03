"""Generate u250 device."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from pathlib import Path

from rapidstream import get_u250_vitis_device_factory

VITIS_PLATFORM = (
    "xilinx_u250_gen3x16_xdma_4_1_202210_1"  # "xilinx_u280_gen3x16_xdma_1_202211_1"
)

factory = get_u250_vitis_device_factory(VITIS_PLATFORM)

# Reduced because of DDR
factory.reduce_slot_area(1, 0, lut=30000, ff=60000)
factory.reduce_slot_area(1, 2, lut=30000, ff=60000)
factory.reduce_slot_area(1, 3, lut=30000, ff=60000)

# Reduced because of congestion
factory.reduce_slot_area(0, 1, lut=10000, ff=20000)
factory.reduce_slot_area(0, 2, lut=10000, ff=20000)
factory.reduce_slot_area(0, 3, lut=5000, ff=10000)

factory.generate_virtual_device(Path("u250_device.json"))
