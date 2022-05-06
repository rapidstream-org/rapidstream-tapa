`default_nettype none

// first-word fall-through (FWFT) FIFO using shift register LUT
// based on HLS generated code
module fifo_srl #(
  parameter MEM_STYLE  = "shiftreg",
  parameter DATA_WIDTH = 32,
  parameter ADDR_WIDTH = 5,
  parameter DEPTH      = 32
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
  parameter REAL_DEPTH = DEPTH < 4 ? 4 : DEPTH;
  parameter REAL_ADDR_WIDTH = $clog2(REAL_DEPTH)+1;

  wire [REAL_ADDR_WIDTH - 1:0] shift_reg_addr;
  wire [DATA_WIDTH - 1:0] shift_reg_data;
  wire [DATA_WIDTH - 1:0] shift_reg_q;
  wire                    shift_reg_ce;
  reg  [REAL_ADDR_WIDTH:0]     out_ptr;
  reg                     internal_empty_n;
  reg                     internal_full_n;

  (* shreg_extract = "yes" *) reg [DATA_WIDTH-1:0] mem [0:REAL_DEPTH-1];

  assign if_empty_n = internal_empty_n;
  assign if_full_n = internal_full_n;
  assign shift_reg_data = if_din;
  assign if_dout = shift_reg_q;

  assign shift_reg_addr = out_ptr[REAL_ADDR_WIDTH] == 1'b0 ? out_ptr[REAL_ADDR_WIDTH-1:0] : {REAL_ADDR_WIDTH{1'b0}};
  assign shift_reg_ce = (if_write & if_write_ce) & internal_full_n;

  assign shift_reg_q = mem[shift_reg_addr];

  always @(posedge clk) begin
    if (reset) begin
      out_ptr <= ~{REAL_ADDR_WIDTH+1{1'b0}};
      internal_empty_n <= 1'b0;
      internal_full_n <= 1'b1;
    end else begin
      if (((if_read && if_read_ce) && internal_empty_n) &&
          (!(if_write && if_write_ce) || !internal_full_n)) begin
        out_ptr <= out_ptr - 1'b1;
        if (out_ptr == {(REAL_ADDR_WIDTH+1){1'b0}})
          internal_empty_n <= 1'b0;
        internal_full_n <= 1'b1;
      end
      else if (((if_read & if_read_ce) == 0 | internal_empty_n == 0) &&
        ((if_write & if_write_ce) == 1 & internal_full_n == 1))
      begin
        out_ptr <= out_ptr + 1'b1;
        internal_empty_n <= 1'b1;
        if (out_ptr == REAL_DEPTH - {{(REAL_ADDR_WIDTH-1){1'b0}}, 2'd2})
          internal_full_n <= 1'b0;
      end
    end
  end

  integer i;
  always @(posedge clk) begin
    if (shift_reg_ce) begin
      for (i = 0; i < REAL_DEPTH - 1; i = i + 1)
        mem[i + 1] <= mem[i];
      mem[0] <= shift_reg_data;
    end
  end

endmodule  // fifo_srl

`default_nettype wire
