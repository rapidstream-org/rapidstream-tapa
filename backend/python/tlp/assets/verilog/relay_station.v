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
  output reg                   if_full_n,
  input  wire                  if_write_ce,
  input  wire                  if_write,
  input  wire [DATA_WIDTH-1:0] if_din,

  // read
  output wire                  if_empty_n,
  input  wire                  if_read_ce,
  input  wire                  if_read,
  output wire [DATA_WIDTH-1:0] if_dout
);

  reg if_write_q;
  reg [DATA_WIDTH-1:0] if_din_q;

  (* dont_touch = "yes" *) wire                  full_n_all [LEVEL:0];
  (* dont_touch = "yes" *) wire                  write_all  [LEVEL:0];
  (* dont_touch = "yes" *) wire [DATA_WIDTH-1:0] din_all    [LEVEL:0];

  genvar i;
  for (i = 0; i < LEVEL; i = i + 1) begin : inst
    fifo #(
      .DATA_WIDTH(DATA_WIDTH),
      .ADDR_WIDTH(ADDR_WIDTH),
      .DEPTH((DEPTH - 1) / LEVEL < 2 ? 2 : (DEPTH - 1) / LEVEL)
    ) unit (
      .clk(clk),
      .reset(reset),

      // connect to fifo i+1
      .if_empty_n(write_all[i+1]),
      .if_read_ce(if_read_ce),
      .if_read   (full_n_all[i+1]),
      .if_dout   (din_all[i+1]),

      // connect to fifo i-1
      .if_full_n  (full_n_all[i]),  // almost full, grace period = 2
      .if_write_ce(if_write_ce),
      .if_write   (write_all[i]),   // delayed input
      .if_din     (din_all[i])      // delayed input
    );
  end

  // connect to interface
  always @ (posedge clk) begin
    if_full_n  <= full_n_all[0];
    if_write_q <= if_write;
    if_din_q   <= if_din;
  end

  assign din_all[0]        = if_din_q;          // input
  assign write_all[0]      = if_write_q;        // input
  assign full_n_all[LEVEL] = if_read;           // input
  assign if_empty_n        = write_all[LEVEL];  // output
  assign if_dout           = din_all[LEVEL];    // output

endmodule   // relay_station

`default_nettype wire