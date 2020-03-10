`default_nettype none

// first-word fall-through (FWFT) FIFO that is friendly for floorplanning
module relay_station #(
    parameter DATA_WIDTH = 32,
    parameter ADDR_WIDTH = 5,
    parameter DEPTH      = 2,
    parameter LEVEL      = 2
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

  (* dont_touch = "yes" *) wire                  full_n  [LEVEL:0];
  (* dont_touch = "yes" *) wire                  empty_n [LEVEL:0];
  (* dont_touch = "yes" *) wire [DATA_WIDTH-1:0] data    [LEVEL:0];

  localparam kDepthPerUnit = (DEPTH - 1) / LEVEL + 1;

  genvar i;
  for (i = 0; i < LEVEL; i = i + 1) begin : inst
    fifo #(
      .DATA_WIDTH(DATA_WIDTH),
      .ADDR_WIDTH(ADDR_WIDTH),
      .DEPTH(kDepthPerUnit < 2 ? 2 : kDepthPerUnit)
    ) unit (
      .clk(clk),
      .reset(reset),

      // connect to fifo[i+1]
      .if_empty_n(empty_n[i+1]),
      .if_read_ce(if_read_ce),
      .if_read   (full_n[i+1]),
      .if_dout   (data[i+1]),

      // connect to fifo[i-1]
      .if_full_n  (full_n[i]),
      .if_write_ce(if_write_ce),
      .if_write   (empty_n[i]),
      .if_din     (data[i])
    );
  end

  // write
  assign if_full_n  = full_n[0];  // output
  assign empty_n[0] = if_write;   // input
  assign data[0]    = if_din;     // input

  // read
  assign if_empty_n    = empty_n[LEVEL];  // output
  assign full_n[LEVEL] = if_read;         // input
  assign if_dout       = data[LEVEL];     // output

endmodule   // relay_station

`default_nettype wire