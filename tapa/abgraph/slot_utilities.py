"""Slot related utilities."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import re

from tapa.abgraph.device.common import Coor

SLOT_PATTERN = r"SLOT_X(\d+)Y(\d+)_TO_SLOT_X(\d+)Y(\d+)"


def is_valid_slot(slot_name: str) -> bool:
    """Check if a slot name is valid."""
    return re.fullmatch(SLOT_PATTERN, slot_name) is not None


def get_coor_from_slot_name(slot_name: str) -> Coor:
    """Get the coordinate from a slot name."""
    match = re.fullmatch(SLOT_PATTERN, slot_name)
    if match is None:
        msg = f"Invalid slot name {slot_name}"
        raise ValueError(msg)

    down_left_x, down_left_y, up_right_x, up_right_y = [int(x) for x in match.groups()]
    return Coor(
        down_left_x=down_left_x,
        down_left_y=down_left_y,
        up_right_x=up_right_x,
        up_right_y=up_right_y,
    )


def is_slot1_inside_slot2(slot1: str, slot2: str) -> bool:
    """Check if slot1 is inside slot2."""
    coor1 = get_coor_from_slot_name(slot1)
    coor2 = get_coor_from_slot_name(slot2)

    return coor1.is_inside(coor2)


def are_slots_adjacent(slot1: str, slot2: str) -> bool:
    """Check if two slots are adjacent."""
    coor1 = get_coor_from_slot_name(slot1)
    coor2 = get_coor_from_slot_name(slot2)

    return (
        coor1.is_north_neighbor_of(coor2)
        or coor1.is_south_neighbor_of(coor2)
        or coor1.is_east_neighbor_of(coor2)
        or coor1.is_west_neighbor_of(coor2)
    )


def are_horizontal_neighbors(slot1: str, slot2: str) -> bool:
    """Check if two slots are horizontal neighbors."""
    coor1 = get_coor_from_slot_name(slot1)
    coor2 = get_coor_from_slot_name(slot2)

    return coor1.is_east_neighbor_of(coor2) or coor1.is_west_neighbor_of(coor2)


def are_vertical_neighbors(slot1: str, slot2: str) -> bool:
    """Check if two slots are vertical neighbors."""
    coor1 = get_coor_from_slot_name(slot1)
    coor2 = get_coor_from_slot_name(slot2)

    return coor1.is_north_neighbor_of(coor2) or coor1.is_south_neighbor_of(coor2)


def convert_to_config_pattern(slot_name: str) -> str:
    """Convert the slot name to the configuration pattern."""
    match = re.fullmatch(SLOT_PATTERN, slot_name)
    if match is None:
        msg = f"Slot name {slot_name} not matching the pattern {SLOT_PATTERN}"
        raise ValueError(msg)

    down_left_x, down_left_y, up_right_x, up_right_y = [int(x) for x in match.groups()]

    # convert to CONFIG_SLOT_PATTERN
    return f"SLOT_X{down_left_x}Y{down_left_y}:SLOT_X{up_right_x}Y{up_right_y}"
