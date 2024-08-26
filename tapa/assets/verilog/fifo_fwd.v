// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

`default_nettype none

// first-word fall-through (FWFT) FIFO with latency=0 and depth=1
module fifo_fwd #(
  parameter MEM_STYLE  = "",  // ignored
  parameter DATA_WIDTH = 32,
  parameter ADDR_WIDTH = 0,   // ignored
  parameter DEPTH      = 1    // ignored
) (
  input wire clk,
  input wire reset,

  // write
  output wire                  if_full_n,
  input  wire                  if_write_ce,
  input  wire                  if_write,
  input  wire [DATA_WIDTH-1:0] if_din,

  // read
  output wire                  if_empty_n,
  input  wire                  if_read_ce,
  input  wire                  if_read,
  output wire [DATA_WIDTH-1:0] if_dout
);
  reg [DATA_WIDTH-1:0] mem;
  reg                  is_mem_valid;

  // If mem is valid, the FIFO is not empty and is full; if read, mem becomes
  // invalid. If mem is not valid, the FIFO is not empty if and only if written
  // and is not full; if written but not read, mem becomes valid.

  assign if_empty_n = is_mem_valid || (if_write && if_write_ce);
  assign if_full_n  = !is_mem_valid;
  assign if_dout    = is_mem_valid ? mem : if_din;

  always @(posedge clk) begin
    if (reset) begin
      is_mem_valid <= 1'b0;
    end else begin
      if (is_mem_valid) begin
        if (if_read && if_read_ce) begin
          is_mem_valid <= 1'b0;
        end
      end else begin
        if (if_write && if_write_ce && !(if_read && if_read_ce)) begin
          is_mem_valid <= 1'b1;
        end
      end

      if (if_write && if_write_ce) begin
        mem <= if_din;
      end
    end
  end

endmodule  // fifo_fwd

`default_nettype wire
