"""The Virtual Device class."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import math
import re
from typing import Any, Literal

from pydantic import BaseModel, model_validator

from tapa.abgraph.device.common import Coor

_logger = logging.getLogger(__name__)

WIRE_CAPACITY_INF = 10**8


class Area(BaseModel):
    """Represents an area."""

    lut: int
    ff: int
    bram_18k: int
    dsp: int
    uram: int

    @model_validator(mode="after")
    def _check_negtive(self) -> "Area":
        """Check all values are non-negative."""
        if not all(value >= 0 for value in self.to_dict().values()):
            msg = f"All area values must be non-negative: {self.to_dict()}"
            raise ValueError(msg)

        return self

    def to_dict(self) -> dict[str, int]:
        """For compatibility with old code."""
        return {
            "LUT": self.lut,
            "FF": self.ff,
            "BRAM_18K": self.bram_18k,
            "DSP": self.dsp,
            "URAM": self.uram,
        }

    def is_empty(self) -> bool:
        """Check if the area is empty."""
        return all(value == 0 for value in self.to_dict().values())

    def is_smaller_than(self, other: "Area") -> bool:
        """Check if the current area is smaller than another area."""
        this_dict = self.to_dict()
        other_dict = other.to_dict()
        return all(this_val <= other_dict[key] for key, this_val in this_dict.items())

    @staticmethod
    def from_dict(area_dict: dict[str, int]) -> "Area":
        """Create an Area from a dictionary."""
        return Area(
            lut=area_dict["LUT"],
            ff=area_dict["FF"],
            bram_18k=area_dict["BRAM_18K"],
            dsp=area_dict["DSP"],
            uram=area_dict["URAM"],
        )


def sum_area(areas: list[Area]) -> Area:
    """Sum up a list of areas."""
    return Area(
        lut=sum(area.lut for area in areas),
        ff=sum(area.ff for area in areas),
        bram_18k=sum(area.bram_18k for area in areas),
        dsp=sum(area.dsp for area in areas),
        uram=sum(area.uram for area in areas),
    )


def get_empty_area() -> Area:
    """Get an empty area."""
    return Area(lut=0, ff=0, bram_18k=0, dsp=0, uram=0)


class VirtualSlot(BaseModel):
    """Represents a virtual slot."""

    area: Area
    x: int
    y: int
    centroid_x_coor: int
    centroid_y_coor: int
    pblock_ranges: list[str] | None = None
    north_wire_capacity: int = WIRE_CAPACITY_INF
    south_wire_capacity: int = WIRE_CAPACITY_INF
    east_wire_capacity: int = WIRE_CAPACITY_INF
    west_wire_capacity: int = WIRE_CAPACITY_INF
    down_left_x: int = -1
    down_left_y: int = -1
    up_right_x: int = -1
    up_right_y: int = -1

    north_anchor_region: list[str] = []
    south_anchor_region: list[str] = []
    east_anchor_region: list[str] = []
    west_anchor_region: list[str] = []

    tags: list[str] = []

    # ruff: noqa: ANN401
    def __init__(self, **data: Any) -> None:
        """Init with extra validation."""
        super().__init__(**data)

        pblock_fields = [
            self.pblock_ranges,
            self.north_anchor_region,
            self.south_anchor_region,
            self.east_anchor_region,
            self.west_anchor_region,
        ]

        for field in pblock_fields:
            if field is not None:
                for pblock_line in field:
                    if pblock_line.startswith(("-add", "-remove")):
                        continue

                    msg = (
                        f"Invalid pblock: {pblock_line}, must start "
                        "with -add or -remove"
                    )
                    raise ValueError(msg)

        self.sanitize_pblock_range()

    def __hash__(self) -> int:
        """Make this hashable."""
        return hash((self.x, self.y))

    def __eq__(self, other: object) -> bool:
        """Make this comparable."""
        if not isinstance(other, VirtualSlot):
            return False
        return self.x == other.x and self.y == other.y

    def get_name(self) -> str:
        """Get the name of the slot."""
        return f"SLOT_X{self.x}Y{self.y}_TO_SLOT_X{self.x}Y{self.y}"

    def add_range(self, pblock_ranges: list[str]) -> None:
        """Add a pblock range."""
        for pblock_range in pblock_ranges:
            self._edit_range(pblock_range, "-add")

        self.sanitize_pblock_range()

    def remove_range(self, pblock_ranges: list[str]) -> None:
        """Remove a pblock range."""
        for pblock_range in pblock_ranges:
            self._edit_range(pblock_range, "-remove")

        self.sanitize_pblock_range()

    def _edit_range(self, pblock_range: str, op: Literal["-add", "-remove"]) -> None:
        """Add a pblock range."""
        if self.pblock_ranges is None:
            self.pblock_ranges = []

        self._check_range_valid(pblock_range)

        self.pblock_ranges.append(f"{op} {pblock_range}")

    def add_anchor_region(self, direction: str, pblock_ranges: list[str]) -> None:
        """Add an anchor region."""
        for pblock_range in pblock_ranges:
            self._edit_anchor_region_range(direction, pblock_range, "-add")

        self.sanitize_pblock_range()

    def remove_anchor_region(self, direction: str, pblock_ranges: list[str]) -> None:
        """Remove an anchor region."""
        for pblock_range in pblock_ranges:
            self._edit_anchor_region_range(direction, pblock_range, "-remove")

        self.sanitize_pblock_range()

    def _edit_anchor_region_range(
        self,
        direction: str,
        pblock_range: str,
        op: Literal["-add", "-remove"],
    ) -> None:
        """Edit an anchor region range."""
        self._check_range_valid(pblock_range)
        line = f"{op} {pblock_range}"

        if direction == "N":
            self.north_anchor_region.append(line)
        elif direction == "S":
            self.south_anchor_region.append(line)
        elif direction == "E":
            self.east_anchor_region.append(line)
        elif direction == "W":
            self.west_anchor_region.append(line)
        else:
            msg = f"Invalid direction: {direction}"
            raise ValueError(msg)

    # ruff: noqa: PLR6301
    def _check_range_valid(self, pblock_range: str) -> None:
        """Check if a pblock range is valid."""
        if not re.search(r"_X\d+Y\d+", pblock_range):
            msg = f"Invalid pblock range: {pblock_range}"
            raise ValueError(msg)

        if pblock_range.startswith(("-add", "-remove")):
            msg = f"Remove -add or -remove from pblock range: {pblock_range}"
            raise ValueError(msg)

    def sanitize_pblock_range(self) -> None:
        """Sanitize all pblock-related attributes."""

        def _sanitize_pblock_range_helper(pblock_lines: list[str]) -> list[str]:
            """Merge all -add and -remove lines together."""
            assert all(line.startswith(("-add", "-remove")) for line in pblock_lines)

            add_ranges = []
            remove_ranges = []
            for _line in pblock_lines:
                # in case the original ranges already have {}
                line = _line.replace("{", "").replace("}", "")

                if line.startswith("-add"):
                    add_ranges.append(re.sub(r"^-add", "", line))
                elif line.startswith("-remove"):
                    remove_ranges.append(re.sub(r"^-remove", "", line))
                else:
                    msg = f"Invalid pblock line: {line}"
                    raise ValueError(msg)

            merged_range = []
            if add_ranges:
                merged_range.append("-add {" + " ".join(add_ranges) + " }")
            if remove_ranges:
                merged_range.append("-remove {" + " ".join(remove_ranges) + " }")

            return merged_range

        attributes = [
            "pblock_ranges",
            "north_anchor_region",
            "south_anchor_region",
            "east_anchor_region",
            "west_anchor_region",
        ]

        for attr in attributes:
            value = getattr(self, attr, None)
            if value:
                setattr(self, attr, _sanitize_pblock_range_helper(value))

    def get_corner_tile_coors(self) -> Coor:
        """Get the corner tile coordinates."""
        return Coor(
            down_left_x=self.down_left_x,
            down_left_y=self.down_left_y,
            up_right_x=self.up_right_x,
            up_right_y=self.up_right_y,
        )


class VirtualDevice(BaseModel):
    """Represents a virtual device."""

    slots: list[VirtualSlot]
    rows: int
    cols: int
    pp_dist: int
    part_num: str
    board_name: str | None
    platform_name: str | None
    user_pblock_name: str | None

    # ruff: noqa: ANN401
    def __init__(self, **data: Any) -> None:
        """Init with extra validation."""
        super().__init__(**data)

        if len(self.slots) != self.rows * self.cols:
            msg = (
                f"Number of slots ({len(self.slots)}) does not match rows * cols "
                f"({self.rows} * {self.cols})"
            )
            raise ValueError(msg)

        for x in range(self.cols):
            for y in range(self.rows):
                self.get_slot(x, y)

    def get_num_col(self) -> int:
        """Get the number of columns."""
        return self.cols

    def get_num_row(self) -> int:
        """Get the number of rows."""
        return self.rows

    def get_slot(self, x: int, y: int) -> VirtualSlot:
        """Get a slot in O(N) time."""
        for slot in self.slots:
            if slot.x == x and slot.y == y:
                return slot

        msg = f"Slot ({x}, {y}) not found"
        raise ValueError(msg)

    def get_island_centroid(self, coor: Coor) -> dict[str, float]:
        """Get the centroid of an island."""
        dl_slot = self.get_slot(coor.down_left_x, coor.down_left_y)
        ur_slot = self.get_slot(coor.up_right_x, coor.up_right_y)

        island_centroid_x = (dl_slot.centroid_x_coor + ur_slot.centroid_x_coor) / 2
        island_centroid_y = (dl_slot.centroid_y_coor + ur_slot.centroid_y_coor) / 2

        return {"x": island_centroid_x, "y": island_centroid_y}

    def get_island_area(self, coor: Coor) -> Area:
        """Get the area of an island."""
        areas = [self.get_slot(x, y).area for x, y in coor.get_all_slot_coors()]
        return sum_area(areas)

    def set_slot_tags(self, x: int, y: int, tags: list[str]) -> None:
        """Set the tags of a slot."""
        self.get_slot(x, y).tags = tags

    def get_tag_to_slot_name(self) -> dict[str, str]:
        """Get the mapping from tag to slot name."""
        tag_to_slot_name = {}
        for slot in self.slots:
            for tag in slot.tags:
                assert tag not in tag_to_slot_name
                tag_to_slot_name[tag] = slot.get_name()

        return tag_to_slot_name

    def set_slot_wire_capacity(
        self,
        x: int,
        y: int,
        direction: str,
        capacity: int,
        mirror: bool = True,
    ) -> None:
        """Set the wire capacity of a slot.

        If mirror is True, the south wire capacity of the up neighbor is also set.
        """
        if direction == "N":
            self.set_slot_north_wire_capacity(x, y, capacity, mirror)
        elif direction == "S":
            self.set_slot_south_wire_capacity(x, y, capacity, mirror)
        elif direction == "E":
            self.set_slot_east_wire_capacity(x, y, capacity, mirror)
        elif direction == "W":
            self.set_slot_west_wire_capacity(x, y, capacity, mirror)
        else:
            msg = f"Invalid direction: {direction}"
            raise ValueError(msg)

    def set_slot_north_wire_capacity(
        self, x: int, y: int, capacity: int, mirror: bool = True
    ) -> None:
        """Set the north wire capacity of a slot.

        If mirror is True, the south wire capacity of the up neighbor is also set.
        """
        self.get_slot(x, y).north_wire_capacity = capacity

        up_neighbor_y = y + 1
        if mirror and up_neighbor_y < self.rows:
            self.get_slot(x, up_neighbor_y).south_wire_capacity = capacity

    def set_slot_south_wire_capacity(
        self, x: int, y: int, capacity: int, mirror: bool = True
    ) -> None:
        """Set the south wire capacity of a slot."""
        self.get_slot(x, y).south_wire_capacity = capacity

        down_neighbor_y = y - 1
        if mirror and down_neighbor_y >= 0:
            self.get_slot(x, down_neighbor_y).north_wire_capacity = capacity

    def set_slot_east_wire_capacity(
        self, x: int, y: int, capacity: int, mirror: bool = True
    ) -> None:
        """Set the east wire capacity of a slot."""
        self.get_slot(x, y).east_wire_capacity = capacity

        right_neighbor_x = x + 1
        if mirror and right_neighbor_x < self.cols:
            self.get_slot(right_neighbor_x, y).west_wire_capacity = capacity

    def set_slot_west_wire_capacity(
        self, x: int, y: int, capacity: int, mirror: bool = True
    ) -> None:
        """Set the west wire capacity of a slot."""
        self.get_slot(x, y).west_wire_capacity = capacity

        left_neighbor_x = x - 1
        if mirror and left_neighbor_x >= 0:
            self.get_slot(left_neighbor_x, y).east_wire_capacity = capacity

    def get_slot_north_wire_capacity(self, x: int, y: int) -> int:
        """Get the north wire capacity of a slot."""
        return self.get_slot(x, y).north_wire_capacity

    def get_slot_south_wire_capacity(self, x: int, y: int) -> int:
        """Get the south wire capacity of a slot."""
        return self.get_slot(x, y).south_wire_capacity

    def get_slot_east_wire_capacity(self, x: int, y: int) -> int:
        """Get the east wire capacity of a slot."""
        return self.get_slot(x, y).east_wire_capacity

    def get_slot_west_wire_capacity(self, x: int, y: int) -> int:
        """Get the west wire capacity of a slot."""
        return self.get_slot(x, y).west_wire_capacity

    def get_island_pblock_range(self, coor: Coor) -> list[str]:
        """Get the pblock range of an island."""
        island_ranges = []
        for x, y in coor.get_all_slot_coors():
            slot_ranges = self.get_slot(x, y).pblock_ranges
            if slot_ranges is None:
                msg = "Slot (%d, %d) does not have a pblock range"
                raise ValueError(msg, x, y)

            island_ranges += slot_ranges

        assert all(line.startswith(("-add", "-remove")) for line in island_ranges)

        return island_ranges

    def get_pipeline_level(self, src_island: Coor, sink_island: Coor) -> int:
        """Get the pipeline level between two slots."""
        src_centroid = self.get_island_centroid(src_island)
        sink_centroid = self.get_island_centroid(sink_island)

        dist_x = abs(src_centroid["x"] - sink_centroid["x"])
        dist_y = abs(src_centroid["y"] - sink_centroid["y"])

        pp_x = dist_x / self.pp_dist
        pp_y = dist_y / self.pp_dist

        # if the source and sink have the same x coordinate, no pipeline is needed
        # otherwise at least one pipeline stage is needed for x direction

        if dist_x != 0:
            pp_x = max(pp_x, 1)

        if dist_y != 0:
            pp_y = max(pp_y, 1)

        return math.ceil(pp_x + pp_y)

    # ruff: noqa: C901, PLR0912
    def sanity_check(self) -> None:  # pylint: disable=too-many-branches
        """Sanity check the device."""
        _logger.info("Sanity checking the virtual device.")

        for x in range(self.cols):
            for y in range(self.rows):
                slot = self.get_slot(x, y)
                name = f"SLOT_X{x}Y{y}"

                if slot.area.is_empty():
                    _logger.warning("%s has an empty area.", name)

                if slot.pblock_ranges is None:
                    _logger.warning("%s does not have a pblock range.", name)

                if y < self.rows - 1:
                    if slot.north_wire_capacity == WIRE_CAPACITY_INF:
                        _logger.warning("%s has infinite north wires.", name)
                    if not slot.north_anchor_region:
                        _logger.warning("%s does not have north anchor region.", name)

                if y > 0:
                    if slot.south_wire_capacity == WIRE_CAPACITY_INF:
                        _logger.warning("%s has infinite south wire capacity.", name)
                    if not slot.south_anchor_region:
                        _logger.warning("%s does not have south anchor region.", name)

                if x < self.cols - 1:
                    if slot.east_wire_capacity == WIRE_CAPACITY_INF:
                        _logger.warning("%s has infinite east wire capacity.", name)
                    if not slot.east_anchor_region:
                        _logger.warning("%s does not have east anchor region.", name)

                if x > 0:
                    if slot.west_wire_capacity == WIRE_CAPACITY_INF:
                        _logger.warning("%s has infinite west wire capacity.", name)
                    if not slot.west_anchor_region:
                        _logger.warning("%s does not have west anchor region.", name)

        # check that the no tag is assigned to multiple slots
        used_tags = set()
        for slot in self.slots:
            for tag in slot.tags:
                if tag in used_tags:
                    msg = f"Tag {tag} is assigned to multiple slots."
                    raise ValueError(msg)
                used_tags.add(tag)

        _logger.info("Finished sanity checking.")


# ruff: noqa: PLR0913, PLR0917
def get_empty_device(
    num_row: int,
    num_col: int,
    part_num: str,
    pp_dist: int = 100,
    unit_dist_x: int = 100,
    unit_dist_y: int = 150,  # penalty for vertical routing through SLR boundary
) -> VirtualDevice:
    """Get an empty virtual device."""
    slots = []
    for x in range(num_col):
        slots.extend(
            VirtualSlot(
                area=get_empty_area(),
                x=x,
                y=y,
                centroid_x_coor=unit_dist_x * x,
                centroid_y_coor=unit_dist_y * y,
            )
            for y in range(num_row)
        )

    return VirtualDevice(
        slots=slots,
        rows=num_row,
        cols=num_col,
        pp_dist=pp_dist,
        part_num=part_num,
        board_name="",
        user_pblock_name="",
    )
