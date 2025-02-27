"""Common items for AutoBridge."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging

from pydantic import BaseModel, ConfigDict

_logger = logging.getLogger(__name__)

RESOURCES = (
    "FF",
    "LUT",
    "BRAM_18K",
    "DSP",
    "URAM",
)


def get_coor(
    down_left_x: int, down_left_y: int, up_right_x: int, up_right_y: int
) -> "Coor":
    """Get a Coor object."""
    return Coor(
        down_left_x=down_left_x,
        down_left_y=down_left_y,
        up_right_x=up_right_x,
        up_right_y=up_right_y,
    )


class Coor(BaseModel):
    """Describe the coordinates of a square region."""

    down_left_x: int
    down_left_y: int
    up_right_x: int
    up_right_y: int

    model_config = ConfigDict(frozen=True)

    def get_name_as_slot(self) -> str:
        """Get the name as a slot."""
        return (
            f"SLOT_X{self.down_left_x}Y{self.down_left_y}"
            f"_TO_SLOT_X{self.up_right_x}Y{self.up_right_y}"
        )

    def __hash__(self) -> int:
        """Get the hash."""
        return hash(self._key())

    def _key(self) -> tuple[int, int, int, int]:
        """Get the key for coor."""
        return (self.down_left_x, self.down_left_y, self.up_right_x, self.up_right_y)

    def get_val(self) -> dict[str, int]:
        """Get the four coordinate number."""
        return self.__dict__

    def is_south_neighbor_of(self, other: "Coor") -> bool:
        """Check if self is on the south side of other."""
        return self.up_right_y + 1 == other.down_left_y and max(
            self.down_left_x, other.down_left_x
        ) <= min(self.up_right_x, other.up_right_x)

    def is_north_neighbor_of(self, other: "Coor") -> bool:
        """Check if self is on the north side of other."""
        return self.down_left_y == other.up_right_y + 1 and max(
            self.down_left_x, other.down_left_x
        ) <= min(self.up_right_x, other.up_right_x)

    def is_east_neighbor_of(self, other: "Coor") -> bool:
        """Check if self is on the east side of other."""
        return self.down_left_x == other.up_right_x + 1 and max(
            self.down_left_y, other.down_left_y
        ) <= min(self.up_right_y, other.up_right_y)

    def is_west_neighbor_of(self, other: "Coor") -> bool:
        """Check if self is on the west side of other."""
        return self.up_right_x + 1 == other.down_left_x and max(
            self.down_left_y, other.down_left_y
        ) <= min(self.up_right_y, other.up_right_y)

    def is_neighbor(self, other: "Coor") -> bool:
        """Check if self is a neighbor of other."""
        return any(
            [
                self.is_north_neighbor_of(other),
                self.is_south_neighbor_of(other),
                self.is_west_neighbor_of(other),
                self.is_east_neighbor_of(other),
            ]
        )

    def get_width(self) -> int:
        """Get the width."""
        return abs(self.up_right_x - self.down_left_x) + 1

    def get_height(self) -> int:
        """Get the height."""
        return abs(self.up_right_y - self.down_left_y) + 1

    def has_shared_boundary(self, other: "Coor") -> bool:
        """Check if two Coor share a common border."""
        return (
            (
                (
                    self.down_left_x <= other.down_left_x
                    and self.up_right_x >= other.up_right_x
                )
                or (
                    self.down_left_y <= other.down_left_y
                    and self.up_right_y >= other.up_right_y
                )
            )
            if self.is_neighbor(other)
            else False
        )

    def get_dict(self) -> dict[str, int]:
        """Get dict representation of the coordinate."""
        return {
            "down_left_x": self.down_left_x,
            "down_left_y": self.down_left_y,
            "up_right_x": self.up_right_x,
            "up_right_y": self.up_right_y,
        }

    def get_all_slot_coors(self) -> list[tuple[int, int]]:
        """Get all slot coordinates."""
        return [
            (x, y)
            for x in range(self.down_left_x, self.up_right_x + 1)
            for y in range(self.down_left_y, self.up_right_y + 1)
        ]

    def is_inside(self, other: "Coor") -> bool:
        """Check if the current coor is inside another.

        Examples:
        >>> node = get_coor(1, 1, 3, 3)
        >>> node.is_inside(get_coor(1, 1, 3, 3))
        True

        >>> node.is_inside(get_coor(0, 0, 4, 4))
        True

        >>> node.is_inside(get_coor(2, 2, 4, 4))
        False
        """
        return (
            self.down_left_x >= other.down_left_x
            and self.down_left_y >= other.down_left_y
            and self.up_right_x <= other.up_right_x
            and self.up_right_y <= other.up_right_y
        )

    def has_overlap(self, other: "Coor") -> bool:
        """Check if the current coor overlaps with another.

        Example:
        >>> node = get_coor(1, 1, 3, 3)
        >>> node.has_overlap(get_coor(1, 1, 3, 3))
        True
        >>> node.has_overlap(get_coor(4, 1, 6, 3))
        False
        >>> node.has_overlap(get_coor(1, 4, 3, 6))
        False
        >>> node.has_overlap(get_coor(4, 4, 6, 6))
        False
        >>> node.has_overlap(get_coor(2, 2, 3, 3))
        True
        >>> node.has_overlap(get_coor(1, 1, 2, 2))
        True
        >>> node.has_overlap(get_coor(2, 2, 4, 4))
        True
        >>> node.has_overlap(get_coor(2, 2, 2, 2))
        True
        >>> node.has_overlap(get_coor(0, 0, 2, 2))
        True
        >>> node.has_overlap(get_coor(0, 0, 0, 0))
        False
        >>> node.has_overlap(get_coor(0, 0, 1, 1))
        True
        """
        # other slot is on the right of the current node
        if other.down_left_x > self.up_right_x:
            return False

        # other slot is on the left of the current node
        if other.up_right_x < self.down_left_x:
            return False

        # other slot is above the current node
        if other.down_left_y > self.up_right_y:
            return False

        # other slot is below the current node
        return not other.up_right_y < self.down_left_y

    def is_perfectly_covered_by(self, child_coors: list["Coor"]) -> bool:
        """Check if current coor is perfectly covered by child coors.

        Examples:
        >>> node = get_coor(1, 1, 3, 3)
        >>> node.is_perfectly_covered_by([get_coor(1, 1, 3, 3)])
        True
        >>> node.is_perfectly_covered_by([get_coor(1, 1, 2, 2)])
        False
        >>> node.is_perfectly_covered_by([get_coor(1, 1, 2, 3)])
        False

        Fully covered but with overlap

        >>> node.is_perfectly_covered_by([get_coor(1, 1, 2, 3), get_coor(2, 1, 3, 3)])
        False

        Fully covered without overlap

        >>> node.is_perfectly_covered_by([get_coor(1, 1, 2, 3), get_coor(3, 1, 3, 3)])
        True
        """
        visited = {
            x: dict.fromkeys(range(self.down_left_y, self.up_right_y + 1), False)
            for x in range(self.down_left_x, self.up_right_x + 1)
        }

        for child in child_coors:
            for x in range(child.down_left_x, child.up_right_x + 1):
                for y in range(child.down_left_y, child.up_right_y + 1):
                    if visited[x][y]:
                        _logger.debug("overlap at (%d, %d)", x, y)
                        return False

                    visited[x][y] = True

        return all(all(visited[x][y] for y in visited[x]) for x in visited)

    def covers(self, other: "Coor") -> bool:
        """The current coor covers the other (input) coor.

        Examples:
        >>> node = get_coor(1, 1, 2, 2)
        >>> node.covers(get_coor(1, 1, 2, 2))
        True
        >>> node.covers(get_coor(1, 1, 3, 3))
        False
        >>> node.covers(get_coor(2, 2, 2, 3))
        False
        >>> node.covers(get_coor(0, 0, 1, 1))
        False
        >>> node.covers(get_coor(1, 2, 2, 2))
        True
        >>> node.covers(get_coor(2, 1, 2, 2))
        True
        >>> node.covers(get_coor(1, 1, 1, 1))
        True
        """
        return (
            self.down_left_x <= other.down_left_x
            and self.down_left_y <= other.down_left_y
            and self.up_right_x >= other.up_right_x
            and self.up_right_y >= other.up_right_y
        )

    def calculate_overlap(self, other: "Coor") -> int:
        """Calculate the overlap area of two rectangles.

        Args:
            other: coordinates of another rectangle

        Returns:
            The area of overlap between the two rectangles

        Examples:
        >>> node = Coor(down_left_x=0, down_left_y=0, up_right_x=5, up_right_y=5)
        >>> node.calculate_overlap(
        ...     Coor(down_left_x=3, down_left_y=3, up_right_x=8, up_right_y=8)
        ... )
        4
        >>> node = Coor(down_left_x=0, down_left_y=0, up_right_x=1, up_right_y=1)
        >>> node.calculate_overlap(
        ...     Coor(down_left_x=2, down_left_y=2, up_right_x=3, up_right_y=3)
        ... )
        0
        >>> node = Coor(down_left_x=0, down_left_y=0, up_right_x=5, up_right_y=5)
        >>> node.calculate_overlap(
        ...     Coor(down_left_x=5, down_left_y=5, up_right_x=10, up_right_y=10)
        ... )
        0
        >>> node = Coor(down_left_x=0, down_left_y=0, up_right_x=10, up_right_y=10)
        >>> node.calculate_overlap(
        ...     Coor(down_left_x=0, down_left_y=0, up_right_x=10, up_right_y=10)
        ... )
        100
        """
        # Find the coordinates of the overlapping rectangle
        left = max(self.down_left_x, other.down_left_x)
        bottom = max(self.down_left_y, other.down_left_y)
        right = min(self.up_right_x, other.up_right_x)
        top = min(self.up_right_y, other.up_right_y)

        # Calculate width and height of the overlapping rectangle
        width = max(0, right - left)
        height = max(0, top - bottom)

        # Calculate and return the area
        return width * height

    def point_covers(self, x: int, y: int) -> bool:
        """Check if the point is covered by the Coor.

        Examples:
        >>> node = get_coor(1, 1, 3, 3)
        >>> node.point_covers(1, 1)
        True
        >>> node.point_covers(3, 3)
        True
        >>> node.point_covers(4, 4)
        False
        """
        return (
            self.down_left_x <= x <= self.up_right_x
            and self.down_left_y <= y <= self.up_right_y
        )
