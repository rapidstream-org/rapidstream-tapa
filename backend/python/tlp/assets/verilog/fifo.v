`default_nettype none

// first-word fall-through (FWFT) FIFO
// if its capacity > THRESHOLD bits, it uses block RAM, otherwise it will uses
// registers
module fifo #(
  parameter DATA_WIDTH = 32,
  parameter ADDR_WIDTH = 5,
  parameter DEPTH      = 32,
  parameter THRESHOLD  = 4096
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

  fifo_bram #(
    .MEM_STYLE (DATA_WIDTH * DEPTH > THRESHOLD ? "block" : "registers"),
    .DATA_WIDTH(DATA_WIDTH),
    .ADDR_WIDTH(ADDR_WIDTH),
    .DEPTH     (DEPTH)
  ) unit (
    .clk      (clk),
    .reset    (reset),

    .if_full_n  (if_full_n),
    .if_write_ce(if_write_ce),
    .if_write   (if_write),
    .if_din     (if_din),

    .if_empty_n(if_empty_n),
    .if_read_ce(if_read_ce),
    .if_read   (if_read),
    .if_dout   (if_dout)
  );

endmodule  // fifo

`default_nettype wire