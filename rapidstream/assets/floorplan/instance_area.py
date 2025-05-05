"""Data structure to represent the area of a module instance."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from pydantic import BaseModel, ConfigDict


class InstanceArea(BaseModel):
    """Resource usage of a module instance."""

    model_config = ConfigDict(frozen=True)

    ff: int
    lut: int
    dsp: int
    bram_18k: int
    uram: int

    def to_dict(self) -> dict[str, int]:
        """Return a dict representation of the area."""
        return {
            "FF": self.ff,
            "LUT": self.lut,
            "DSP": self.dsp,
            "BRAM_18K": self.bram_18k,
            "URAM": self.uram,
        }

    def multiply_by_factor(self, factor: float) -> "InstanceArea":
        """Multiply the area by a factor."""
        return InstanceArea(
            ff=int(self.ff * factor),
            lut=int(self.lut * factor),
            dsp=int(self.dsp * factor),
            bram_18k=int(self.bram_18k * factor),
            uram=int(self.uram * factor),
        )

    def to_string(self) -> str:
        """Return a string representation of the area."""
        return self.model_dump_json()

    @staticmethod
    def get_zero_area() -> "InstanceArea":
        """Return an instance area with zero area."""
        return InstanceArea(ff=0, lut=0, dsp=0, bram_18k=0, uram=0)

    @staticmethod
    def get_instance_area_from_dict(area: dict[str, int]) -> "InstanceArea":
        """Return an instance area from a dict."""
        ff = int(area["FF"])
        lut = int(area["LUT"])

        if "DSP" in area:
            dsp = int(area["DSP"])
        elif "DSP48E" in area:
            dsp = int(area["DSP48E"])
        else:
            msg = "Cannot find DSP area"
            raise ValueError(msg)

        if "BRAM_18K" in area:
            bram_18k = int(area["BRAM_18K"])
        elif "BRAM_36K" in area:
            bram_18k = int(area["BRAM_36K"]) * 2
        elif "BRAM" in area:
            bram_18k = int(area["BRAM"])
        else:
            msg = "Cannot find BRAM area"
            raise ValueError(msg)

        uram = int(area["URAM"])

        return InstanceArea(ff=ff, lut=lut, dsp=dsp, bram_18k=bram_18k, uram=uram)
