"""Tapa graphir templates."""

from pathlib import Path

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""


def get_fifo_template() -> str:
    """Get the FIFO template."""
    fifo_v_path = Path(__file__).parent.parent / "assets" / "verilog" / "fifo.v"
    with open(fifo_v_path, encoding="utf-8") as f:
        return f.read()


FIFO_TEMPLATE = get_fifo_template()


RESET_INVERTER_TEMPLATE = """
// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

module reset_inverter (
  input wire clk,
  input wire rst_n,
  output wire rst
);

  assign rst = ~rst_n;

endmodule
"""
