"""Generate rapidstream configuration files."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""
import os
from pathlib import Path

from rapidstream.assets.device_library.u250.u250 import get_u250_precollected_device
from rapidstream.assets.floorplan.floorplan_config import FloorplanConfig

TEMP_DIR = Path("config_build")
FLOOR_PLAN_CONFIG = TEMP_DIR / "floorplan_config.json"
DEVICE_CONFIG = TEMP_DIR / "device_config.json"


def gen_config() -> None:
    """Generate configuration files."""
    if not os.path.exists(TEMP_DIR):
        os.makedirs(TEMP_DIR)
    floorplan_config = FloorplanConfig(
        port_pre_assignments={".*": "SLOT_X0Y0:SLOT_X0Y0"},
    )
    floorplan_config.save_to_file(FLOOR_PLAN_CONFIG)
    get_u250_precollected_device(DEVICE_CONFIG)


if __name__ == "__main__":
    gen_config()
