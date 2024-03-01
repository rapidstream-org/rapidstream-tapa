`default_nettype none

// first-word fall-through (FWFT) FIFO that is friendly for floorplanning
module relay_station #(
    parameter DATA_WIDTH = 32,
    parameter ADDR_WIDTH = 5,
    parameter DEPTH      = 2,
    parameter LEVEL      = 2,
    parameter CONNECT    = 1   // add api to disconnect the relay station
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

  wire                  full_n  [LEVEL:0];
  wire                  empty_n [LEVEL:0];
  wire [DATA_WIDTH-1:0] data    [LEVEL:0];

  // both full_n and write are registered, thus one level of relay_station cause
  // two additional latency for the almost full fifo
  parameter GRACE_PERIOD = LEVEL * 2;
  parameter REAL_DEPTH = GRACE_PERIOD + DEPTH + 4;
  parameter REAL_ADDR_WIDTH  = $clog2(REAL_DEPTH);

  genvar i;
  generate
  if (CONNECT > 0) begin
    if (LEVEL > 0) begin

      for (i = 0; i < LEVEL; i = i + 1) begin : inst
        if (i < LEVEL - 1) begin
          fifo_reg #(
            .DATA_WIDTH(DATA_WIDTH)
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

        end else begin
          (* keep = "true" *) fifo_almost_full #(
            .DATA_WIDTH(DATA_WIDTH),
            .ADDR_WIDTH(REAL_ADDR_WIDTH),
            .DEPTH(REAL_DEPTH),
            .GRACE_PERIOD(GRACE_PERIOD)
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
      end

      // write
      assign if_full_n  = full_n[0];  // output
      assign empty_n[0] = if_write & full_n[0];   // input
      assign data[0]    = if_din;     // input

      // read
      assign if_empty_n    = empty_n[LEVEL];  // output
      assign full_n[LEVEL] = if_read;         // input
      assign if_dout       = data[LEVEL];     // output

    end

    // LEVEL == 0
    else begin
      assign if_full_n  = if_read;  // output
      assign if_empty_n    = if_write;  // output
      assign if_dout       = if_din;     // output
    end
  end

  // disconnect the relay station
  else begin
    assign if_full_n  = 0;  // output
    assign if_empty_n = 0;  // output
    // leave the dout port dangling to facilitate pruning
    // assign if_dout    = 0;     // output
  end
  endgenerate

endmodule   // relay_station

/////////////////////////////////////////////////////////////

module fifo_reg #(
  parameter DATA_WIDTH = 32
) (
  input wire clk,
  input wire reset,

  // write
  (* keep = "true" *)
  output reg                   if_full_n,
  input  wire                  if_write_ce,
  input  wire                  if_write,
  input  wire [DATA_WIDTH-1:0] if_din,

  // read
  (* keep = "true" *)
  output reg                   if_empty_n,
  input  wire                  if_read_ce,
  input  wire                  if_read,
  (* keep = "true" *)
  output reg  [DATA_WIDTH-1:0] if_dout
);

  always @ (posedge clk) begin
    if_dout <= if_din;
    if_empty_n <= if_write;
    if_full_n <= if_read;
  end

endmodule

/////////////////////////////////////////////////////////////////

// first-word fall-through (FWFT) FIFO
module fifo_almost_full #(
  parameter DATA_WIDTH = 32,
  parameter ADDR_WIDTH = 5,
  parameter DEPTH      = 32,
  parameter GRACE_PERIOD = 2
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

generate
  if (DATA_WIDTH >= 36 && DEPTH >= 4096) begin : uram
    fifo_bram_almost_full #(
      .MEM_STYLE   ("ultra"),
      .DATA_WIDTH  (DATA_WIDTH),
      .ADDR_WIDTH  (ADDR_WIDTH),
      .DEPTH       (DEPTH),
      .GRACE_PERIOD(GRACE_PERIOD)
    ) unit (
      .clk  (clk),
      .reset(reset),

      .if_full_n  (if_full_n),
      .if_write_ce(if_write_ce),
      .if_write   (if_write),
      .if_din     (if_din),

      .if_empty_n(if_empty_n),
      .if_read_ce(if_read_ce),
      .if_read   (if_read),
      .if_dout   (if_dout)
    );
  end else if (DEPTH >= 128) begin : bram
    fifo_bram_almost_full #(
      .MEM_STYLE ("block"),
      .DATA_WIDTH(DATA_WIDTH),
      .ADDR_WIDTH(ADDR_WIDTH),
      .DEPTH     (DEPTH),
      .GRACE_PERIOD(GRACE_PERIOD) /*********/
    ) unit (
      .clk  (clk),
      .reset(reset),

      .if_full_n  (if_full_n),
      .if_write_ce(if_write_ce),
      .if_write   (if_write),
      .if_din     (if_din),

      .if_empty_n(if_empty_n),
      .if_read_ce(if_read_ce),
      .if_read   (if_read),
      .if_dout   (if_dout)
    );
  end else begin : srl
    fifo_srl_almost_full #(
      .DATA_WIDTH(DATA_WIDTH),
      .ADDR_WIDTH(ADDR_WIDTH),
      .DEPTH     (DEPTH),
      .GRACE_PERIOD(GRACE_PERIOD) /*********/
    ) unit (
      .clk  (clk),
      .reset(reset),

      .if_full_n  (if_full_n),
      .if_write_ce(if_write_ce),
      .if_write   (if_write),
      .if_din     (if_din),

      .if_empty_n(if_empty_n),
      .if_read_ce(if_read_ce),
      .if_read   (if_read),
      .if_dout   (if_dout)
    );
  end
endgenerate

endmodule  // fifo

/////////////////////////////////////////////////////////////////

module fifo_srl_almost_full #(
  parameter MEM_STYLE    = "shiftreg",
  parameter DATA_WIDTH   = 32,
  parameter ADDR_WIDTH   = 4,
  parameter DEPTH        = 16,
  parameter GRACE_PERIOD = 2
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

wire[REAL_ADDR_WIDTH - 1:0] shiftReg_addr ;
wire[DATA_WIDTH - 1:0] shiftReg_data, shiftReg_q;
wire                     shiftReg_ce;
reg[REAL_ADDR_WIDTH:0] mOutPtr = ~{(REAL_ADDR_WIDTH+1){1'b0}};
reg internal_empty_n = 0, internal_full_n = 1;

assign if_empty_n = internal_empty_n;

/*******************************************/
// assign if_full_n = internal_full_n;
reg almost_full_q;
wire almost_full = mOutPtr >= REAL_DEPTH - 1 - GRACE_PERIOD - 1 && mOutPtr != ~{REAL_ADDR_WIDTH+1{1'b0}};
always @ (posedge clk) almost_full_q <= almost_full;
assign if_full_n = ~almost_full_q;
/*******************************************/

assign shiftReg_data = if_din;
assign if_dout = shiftReg_q;

always @ (posedge clk) begin
    if (reset == 1'b1)
    begin
        mOutPtr <= ~{REAL_ADDR_WIDTH+1{1'b0}};
        internal_empty_n <= 1'b0;
        internal_full_n <= 1'b1;
    end
    else begin
        if (((if_read & if_read_ce) == 1 & internal_empty_n == 1) &&
            ((if_write & if_write_ce) == 0 | internal_full_n == 0))
        begin
            mOutPtr <= mOutPtr - 5'd1;
            if (mOutPtr == 0)
                internal_empty_n <= 1'b0;
            internal_full_n <= 1'b1;
        end
        else if (((if_read & if_read_ce) == 0 | internal_empty_n == 0) &&
            ((if_write & if_write_ce) == 1 & internal_full_n == 1))
        begin
            mOutPtr <= mOutPtr + 5'd1;
            internal_empty_n <= 1'b1;
            if (mOutPtr == REAL_DEPTH - 5'd2)
                internal_full_n <= 1'b0;
        end
    end
end

assign shiftReg_addr = mOutPtr[REAL_ADDR_WIDTH] == 1'b0 ? mOutPtr[REAL_ADDR_WIDTH-1:0]:{REAL_ADDR_WIDTH{1'b0}};
assign shiftReg_ce = (if_write & if_write_ce) & internal_full_n;

fifo_srl_almost_full_internal
#(
    .DATA_WIDTH(DATA_WIDTH),
    .ADDR_WIDTH(REAL_ADDR_WIDTH),
    .DEPTH(REAL_DEPTH))
U_fifo_w32_d16_A_ram (
    .clk(clk),
    .data(shiftReg_data),
    .ce(shiftReg_ce),
    .a(shiftReg_addr),
    .q(shiftReg_q));

endmodule

module fifo_srl_almost_full_internal (
  input  wire                  clk,
  input  wire [DATA_WIDTH-1:0] data,
  input  wire                  ce,
  input  wire [ADDR_WIDTH-1:0] a,
  output wire [DATA_WIDTH-1:0] q
);

parameter DATA_WIDTH = 32'd32;
parameter ADDR_WIDTH = 32'd4;
parameter DEPTH = 5'd16;

(* shreg_extract = "yes" *) reg[DATA_WIDTH-1:0] SRL_SIG [0:DEPTH-1];
integer i;

always @ (posedge clk)
    begin
        if (ce)
        begin
            for (i=0;i<DEPTH-1;i=i+1)
                SRL_SIG[i+1] <= SRL_SIG[i];
            SRL_SIG[0] <= data;
        end
    end

assign q = SRL_SIG[a];

endmodule

///////////////////////////////////////////////////////////

// first-word fall-through (FWFT) FIFO using block RAM
// based on HLS generated code
module fifo_bram_almost_full #(
  parameter MEM_STYLE  = "auto",
  parameter DATA_WIDTH = 32,
  parameter ADDR_WIDTH = 5,
  parameter DEPTH      = 32,
  parameter GRACE_PERIOD = 2
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

(* ram_style = MEM_STYLE *)
reg  [DATA_WIDTH-1:0] mem[0:DEPTH-1];
reg  [DATA_WIDTH-1:0] q_buf;
reg  [ADDR_WIDTH-1:0] waddr;
reg  [ADDR_WIDTH-1:0] raddr;
wire [ADDR_WIDTH-1:0] wnext;
wire [ADDR_WIDTH-1:0] rnext;
wire                  push;
wire                  pop;
reg  [ADDR_WIDTH-1:0] used;
reg                   full_n;
reg                   empty_n;
reg  [DATA_WIDTH-1:0] q_tmp;
reg                   show_ahead;
reg  [DATA_WIDTH-1:0] dout_buf;
reg                   dout_valid;

localparam DepthM1 = DEPTH[ADDR_WIDTH-1:0] - 1'd1;

/**************************************/
wire almost_full = (used >= DEPTH - 1 - GRACE_PERIOD);
//assign if_full_n  = full_n;
assign if_full_n  = ~almost_full;
/**************************************/

assign if_empty_n = dout_valid;
assign if_dout    = dout_buf;
assign push       = full_n & if_write_ce & if_write;
assign pop        = empty_n & if_read_ce & (~dout_valid | if_read);
assign wnext      = !push              ? waddr              :
                    (waddr == DepthM1) ? {ADDR_WIDTH{1'b0}} : waddr + 1'd1;
assign rnext      = !pop               ? raddr              :
                    (raddr == DepthM1) ? {ADDR_WIDTH{1'b0}} : raddr + 1'd1;



// waddr
always @(posedge clk) begin
  if (reset)
    waddr <= {ADDR_WIDTH{1'b0}};
  else
    waddr <= wnext;
end

// raddr
always @(posedge clk) begin
  if (reset)
    raddr <= {ADDR_WIDTH{1'b0}};
  else
    raddr <= rnext;
end

// used
always @(posedge clk) begin
  if (reset)
    used <= {ADDR_WIDTH{1'b0}};
  else if (push && !pop)
    used <= used + 1'b1;
  else if (!push && pop)
    used <= used - 1'b1;
end

// full_n
always @(posedge clk) begin
  if (reset)
    full_n <= 1'b1;
  else if (push && !pop)
    full_n <= (used != DepthM1);
  else if (!push && pop)
    full_n <= 1'b1;
end

// empty_n
always @(posedge clk) begin
  if (reset)
    empty_n <= 1'b0;
  else if (push && !pop)
    empty_n <= 1'b1;
  else if (!push && pop)
    empty_n <= (used != {{(ADDR_WIDTH-1){1'b0}},1'b1});
end

// mem
always @(posedge clk) begin
  if (push)
    mem[waddr] <= if_din;
end

// q_buf
always @(posedge clk) begin
  q_buf <= mem[rnext];
end

// q_tmp
always @(posedge clk) begin
  if (reset)
    q_tmp <= {DATA_WIDTH{1'b0}};
  else if (push)
    q_tmp <= if_din;
end

// show_ahead
always @(posedge clk) begin
  if (reset)
    show_ahead <= 1'b0;
  else if (push && used == {{(ADDR_WIDTH-1){1'b0}},pop})
    show_ahead <= 1'b1;
  else
    show_ahead <= 1'b0;
end

// dout_buf
always @(posedge clk) begin
  if (reset)
    dout_buf <= {DATA_WIDTH{1'b0}};
  else if (pop)
    dout_buf <= show_ahead? q_tmp : q_buf;
end

// dout_valid
always @(posedge clk) begin
  if (reset)
    dout_valid <= 1'b0;
  else if (pop)
    dout_valid <= 1'b1;
  else if (if_read_ce & if_read)
    dout_valid <= 1'b0;
end

endmodule  // fifo_bram

`default_nettype wire
