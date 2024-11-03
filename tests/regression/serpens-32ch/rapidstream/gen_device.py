"""Generate U280 device."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from pathlib import Path

from rapidstream import get_u280_vitis_3x3_device_factory

VITIS_PLATFORM = (
    "xilinx_u280_gen3x16_xdma_1_202211_1"  # "xilinx_u55c_gen3x16_xdma_3_202210_1"
)

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

factory.generate_virtual_device(Path("u280_device.json"))
