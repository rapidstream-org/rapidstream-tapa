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
factory.reduce_slot_area(2, 0, lut=55000, ff=70000)
factory.reduce_slot_area(1, 0, lut=30000, ff=40000)
factory.reduce_slot_area(0, 0, lut=30000, ff=40000)

# reduce the right-most column of DSP from 3X3Slot(2,1)
factory.reduce_slot_area(2, 1, dsp=100, lut=5000, ff=10000)

# reduce the wire number between (1,1)<->(2,1), (1,0)<->(2,0)
factory.set_slot_wire_capacity_in_pair(1, 1, 2, 1, 11000)
factory.set_slot_wire_capacity_in_pair(1, 0, 2, 0, 11000)

factory.generate_virtual_device(Path("u55c_device.json"))

# reduce (2,0) LUT from 50,000 to 55,000. FF from 60,000 to 70,000
# reduce (1,0) LUT from 25,000 to 30,000. FF from 30,000 to 40,000
# reduce (0,0) LUT from 25,000 to 30,000. FF from 30,000 to 40,000
# reduce (2,1) LUT from 0 to 5,000, FF from 0 to 10,000

# reduce wire number from 12,000 to 11,000 between (1,1)<->(2,1), (1,0)<->(2,0)
