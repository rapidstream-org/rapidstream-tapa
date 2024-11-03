"""Generate U55c device."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from pathlib import Path

from rapidstream import get_u55c_vitis_3x3_device_factory

VITIS_PLATFORM = (
    "xilinx_u55c_gen3x16_xdma_3_202210_1"  # "xilinx_u280_gen3x16_xdma_1_202211_1"
)

factory = get_u55c_vitis_3x3_device_factory(VITIS_PLATFORM)

# reduce some budget due to HBM
factory.reduce_slot_area(2, 0, lut=50000, ff=60000)
factory.reduce_slot_area(1, 0, lut=25000, ff=30000)
factory.reduce_slot_area(0, 0, lut=25000, ff=30000)

# reduce the right-most column of DSP from 3X3Slot(2,1)
factory.reduce_slot_area(2, 1, dsp=100)

factory.generate_virtual_device(Path("u55c_device.json"))
