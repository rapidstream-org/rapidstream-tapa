`default_nettype none

module async_mmap #(
  parameter BufferSize        = 64,
  parameter BufferSizeLog     = 6,
  parameter AddrWidth         = 64,
  parameter DataWidth         = 512,
  parameter DataWidthBytesLog = 6,  // must equal log2(DataWidth/8)
  parameter WaitTimeWidth     = 4,
  parameter BurstLenWidth     = 8
) (
  input wire clk,
  input wire rst, // active high

  // for burst inference
  input wire [WaitTimeWidth-1:0] max_wait_time,
  input wire [BurstLenWidth-1:0] max_burst_len,

  // axi write addr channel
  output wire                 m_axi_AWVALID,
  input  wire                 m_axi_AWREADY,
  output wire [AddrWidth-1:0] m_axi_AWADDR,
  output wire [0:0]           m_axi_AWID,
  output wire [7:0]           m_axi_AWLEN,
  output wire [2:0]           m_axi_AWSIZE,
  output wire [1:0]           m_axi_AWBURST,
  output wire [0:0]           m_axi_AWLOCK,
  output wire [3:0]           m_axi_AWCACHE,
  output wire [2:0]           m_axi_AWPROT,
  output wire [3:0]           m_axi_AWQOS,

  // axi write data channel
  output wire                   m_axi_WVALID,
  input  wire                   m_axi_WREADY,
  output wire [DataWidth-1:0]   m_axi_WDATA,
  output wire [DataWidth/8-1:0] m_axi_WSTRB,
  output wire                   m_axi_WLAST,

  // axi write acknowledge channel
  input  wire       m_axi_BVALID,
  output wire       m_axi_BREADY,
  input  wire [1:0] m_axi_BRESP,
  input  wire [0:0] m_axi_BID,

  // axi read addr channel
  output wire                 m_axi_ARVALID,
  input  wire                 m_axi_ARREADY,
  output wire [AddrWidth-1:0] m_axi_ARADDR,
  output wire [0:0]           m_axi_ARID,
  output wire [7:0]           m_axi_ARLEN,
  output wire [2:0]           m_axi_ARSIZE,
  output wire [1:0]           m_axi_ARBURST,
  output wire [0:0]           m_axi_ARLOCK,
  output wire [3:0]           m_axi_ARCACHE,
  output wire [2:0]           m_axi_ARPROT,
  output wire [3:0]           m_axi_ARQOS,

  // axi read response channel
  input  wire                 m_axi_RVALID,
  output wire                 m_axi_RREADY,
  input  wire [DataWidth-1:0] m_axi_RDATA,
  input  wire                 m_axi_RLAST,
  input  wire [0:0]           m_axi_RID,
  input  wire [1:0]           m_axi_RRESP,

  // push read addr here
  input  wire [AddrWidth-1:0] read_addr_din,
  input  wire                 read_addr_write,
  output wire                 read_addr_full_n,

  // pop read resp here
  output wire [DataWidth-1:0] read_data_dout,
  input  wire                 read_data_read,
  output wire                 read_data_empty_n,

  // push write addr and data here
  input  wire [AddrWidth-1:0] write_addr_din,
  input  wire                 write_addr_write,
  output wire                 write_addr_full_n,
  input  wire [DataWidth-1:0] write_data_din,
  input  wire                 write_data_write,
  output wire                 write_data_full_n,

  // pop write resp here
  output wire [7:0] write_resp_dout,
  input  wire       write_resp_read,
  output wire       write_resp_empty_n
);

  // add flow control between the read_addr channel and the read_data channel
  // if the read_data channel is close to full, stop issuing more read requests
  // buffer size set by FlowControlBufferDepth
  parameter FlowControlBufferDepth = 64;
  wire read_data_fifo_almost_full_n;
  wire m_axi_ARVALID_internal;
  wire m_axi_ARREADY_internal;
  assign m_axi_ARVALID = m_axi_ARVALID_internal & read_data_fifo_almost_full_n;
  assign m_axi_ARREADY_internal = m_axi_ARREADY & read_data_fifo_almost_full_n;
  
  // write addr buffer, from user to burst detector
  wire [AddrWidth-1:0] write_addr_dout;
  wire                 write_addr_empty_n;
  wire                 write_addr_read;

  fifo #(
    .DATA_WIDTH(AddrWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize)
  ) write_addr (
    .clk  (clk),
    .reset(rst),

    // from user
    .if_full_n  (write_addr_full_n),
    .if_write_ce(1'b1),
    .if_write   (write_addr_write),
    .if_din     (write_addr_din),

    // to burst detector
    .if_empty_n(write_addr_empty_n),
    .if_read_ce(1'b1),
    .if_read   (write_addr_read),
    .if_dout   (write_addr_dout)
  );

  // burst write addr buffer, from burst detector to axi
  wire [BurstLenWidth+AddrWidth-1:0] burst_write_addr_din;
  wire                               burst_write_addr_full_n;
  wire                               burst_write_addr_write;
  wire [AddrWidth-1:0]               burst_write_addr_dout_addr;
  wire [BurstLenWidth-1:0]           burst_write_addr_dout_burst_len;
  wire                               burst_write_addr_empty_n;
  wire                               burst_write_addr_read;

  wire  [BurstLenWidth-1:0] burst_write_len_din;
  wire                      burst_write_len_full_n;
  wire                      burst_write_len_write;
  wire  [BurstLenWidth-1:0] burst_write_len_dout;
  wire                      burst_write_len_empty_n;
  wire                      burst_write_len_read;

  wire [BurstLenWidth-1:0] write_req_din;
  wire                     write_req_write;
  wire                     write_req_full_n;
  wire [BurstLenWidth-1:0] write_req_dout;
  wire                     write_req_read;
  wire                     write_req_empty_n;

  wire burst_write_last_din;
  wire burst_write_last_full_n;
  wire burst_write_last_write;
  wire burst_write_last_dout;
  wire burst_write_last_empty_n;

  detect_burst #(
    .AddrWidth        (AddrWidth),
    .DataWidthBytesLog(DataWidthBytesLog),
    .WaitTimeWidth    (WaitTimeWidth),
    .BurstLenWidth    (BurstLenWidth)
  ) detect_burst_write (
    .clk(clk),
    .rst(rst),

    .max_wait_time(max_wait_time),
    .max_burst_len(max_burst_len),

    // input: individual addresses
    .addr_dout   (write_addr_dout),
    .addr_empty_n(write_addr_empty_n),
    .addr_read   (write_addr_read),

    // output: inferred burst addresses
    .addr_din   (burst_write_addr_din),
    .addr_full_n(burst_write_addr_full_n),
    .addr_write (burst_write_addr_write),

    // output: used to generate the "last" signals
    .burst_len_0_din   (burst_write_len_din),
    .burst_len_0_full_n(burst_write_len_full_n),
    .burst_len_0_write (burst_write_len_write),

    // output: used to generate write responses
    .burst_len_1_din   (write_req_din),
    .burst_len_1_full_n(write_req_full_n),
    .burst_len_1_write (write_req_write)
  );

  fifo #(
    .DATA_WIDTH(BurstLenWidth + AddrWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize)
  ) burst_write_addr (
    .clk  (clk),
    .reset(rst),

    // from burst detector
    .if_full_n  (burst_write_addr_full_n),
    .if_write_ce(1'b1),
    .if_write   (burst_write_addr_write),
    .if_din     (burst_write_addr_din),

    // to axi
    .if_empty_n(burst_write_addr_empty_n),
    .if_read_ce(1'b1),
    .if_read   (burst_write_addr_read),
    .if_dout   ({burst_write_addr_dout_burst_len, burst_write_addr_dout_addr})
  );

  fifo #(
    .DATA_WIDTH(BurstLenWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize)
  ) burst_write_len (
    .clk  (clk),
    .reset(rst),

    // from burst detector
    .if_full_n  (burst_write_len_full_n),
    .if_write_ce(1'b1),
    .if_write   (burst_write_len_write),
    .if_din     (burst_write_len_din),

    // to last generator
    .if_empty_n(burst_write_len_empty_n),
    .if_read_ce(1'b1),
    .if_read   (burst_write_len_read),
    .if_dout   (burst_write_len_dout)
  );

  // generate last signal for W channel
  generate_last #(
    .BurstLenWidth(BurstLenWidth)
  ) generate_last_unit(
    .clk(clk),
    .rst(rst),

    .burst_len_dout   (burst_write_len_dout),
    .burst_len_empty_n(burst_write_len_empty_n),
    .burst_len_read   (burst_write_len_read),

    .last_din   (burst_write_last_din),
    .last_full_n(burst_write_last_full_n),
    .last_write (burst_write_last_write)
  );

  // write req buffer that remembers the burst length of each write transaction
  fifo #(
    .DATA_WIDTH(BurstLenWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize)
  ) write_req (
    .clk  (clk),
    .reset(rst),

    // from burst detector
    .if_full_n  (write_req_full_n),
    .if_write_ce(1'b1),
    .if_write   (write_req_write),
    .if_din     (write_req_din),

    // to write resp buffer
    .if_empty_n(write_req_empty_n),
    .if_read_ce(1'b1),
    .if_read   (write_req_read),
    .if_dout   (write_req_dout)
  );

  // this fifo should be synchronized with the wr_data fifo
  fifo #(
    .DATA_WIDTH(1),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize)
  ) burst_write_last (
    .clk  (clk),
    .reset(rst),

    // from burst detector
    .if_full_n  (burst_write_last_full_n),
    .if_write_ce(1'b1),
    .if_write   (burst_write_last_write),
    .if_din     (burst_write_last_din),

    // to axi
    .if_empty_n(burst_write_last_empty_n),
    .if_read_ce(1'b1),
    // deal with when last-fifo is non-empty while data-fifo is empty
    .if_read   (m_axi_WREADY && write_data_empty_n),
    .if_dout   (burst_write_last_dout)
  );

  // write data buffer
  wire [DataWidth-1:0] write_data_dout;
  wire                 write_data_empty_n;
  fifo #(
    .DATA_WIDTH(DataWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize)
  ) write_data (
    .clk  (clk),
    .reset(rst),

    // from user
    .if_full_n  (write_data_full_n),
    .if_write_ce(1'b1),
    .if_write   (write_data_write),
    .if_din     (write_data_din),

    // to axi
    .if_empty_n(write_data_empty_n),
    .if_read_ce(1'b1),
    // deal with when data-fifo is non empty but last-fifo is empty
    .if_read   (m_axi_WREADY && burst_write_last_empty_n),
    .if_dout   (write_data_dout)
  );

  // write resp buffer
  wire [BurstLenWidth-1:0] write_resp_din   = write_req_dout;
  wire                     write_resp_write = m_axi_BVALID && write_req_empty_n;
  wire                     write_resp_full_n;
  fifo #(
    .DATA_WIDTH(BurstLenWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize)
  ) write_resp (
    .clk  (clk),
    .reset(rst),

    // from write req buffer and axi
    .if_full_n  (write_resp_full_n),
    .if_write_ce(1'b1),
    .if_write   (write_resp_write),
    .if_din     (write_resp_din),

    // to user
    .if_empty_n(write_resp_empty_n),
    .if_read_ce(1'b1),
    .if_read   (write_resp_read),
    .if_dout   (write_resp_dout)
  );

  // AW channel
  assign burst_write_addr_read = m_axi_AWREADY;
  assign m_axi_AWVALID  = burst_write_addr_empty_n;
  assign m_axi_AWADDR   = burst_write_addr_dout_addr;
  assign m_axi_AWID     = 0;
  assign m_axi_AWLEN    = burst_write_addr_dout_burst_len;
  assign m_axi_AWSIZE   = DataWidthBytesLog;
  assign m_axi_AWBURST  = 1;        // INCR mode
  assign m_axi_AWLOCK   = 0;        // Xilinx only supports 0
  assign m_axi_AWCACHE  = 4'b0011;  // Xilinx only supports 4'b0011
  assign m_axi_AWPROT   = 0;
  assign m_axi_AWQOS    = 0;

  // W channel
  assign m_axi_WVALID = write_data_empty_n && burst_write_last_empty_n;
  assign m_axi_WDATA  = write_data_dout;
  assign m_axi_WSTRB  = {(DataWidth/8){1'b1}};  // assume every bit is valid
  assign m_axi_WLAST  = burst_write_last_dout;

  // B channel
  assign m_axi_BREADY   = write_resp_full_n && write_req_empty_n;
  assign write_req_read = write_resp_full_n && m_axi_BVALID;

  // read addr buffer, from user to burst detector
  wire [AddrWidth-1:0] read_addr_dout;
  wire                 read_addr_empty_n;
  wire                 read_addr_read;

  fifo #(
    .DATA_WIDTH(AddrWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize)
  ) read_addr (
    .clk  (clk),
    .reset(rst),

    // from user
    .if_full_n  (read_addr_full_n),
    .if_write_ce(1'b1),
    .if_write   (read_addr_write),
    .if_din     (read_addr_din),

    // to axi
    .if_empty_n(read_addr_empty_n),
    .if_read_ce(1'b1),
    .if_read   (read_addr_read),
    .if_dout   (read_addr_dout)
  );

  wire [BurstLenWidth+AddrWidth-1:0] burst_read_addr_din;
  wire                               burst_read_addr_full_n;
  wire                               burst_read_addr_write;
  wire [AddrWidth-1:0]               burst_read_addr_dout_addr;
  wire [BurstLenWidth-1:0]           burst_read_addr_dout_burst_len;
  wire                               burst_read_addr_empty_n;
  wire                               burst_read_addr_read;

  fifo #(
    .DATA_WIDTH(BurstLenWidth + AddrWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize)
  ) burst_read_addr (
    .clk  (clk),
    .reset(rst),

    // from burst detector
    .if_full_n  (burst_read_addr_full_n),
    .if_write_ce(1'b1),
    .if_write   (burst_read_addr_write),
    .if_din     (burst_read_addr_din),

    // to axi
    .if_empty_n(burst_read_addr_empty_n),
    .if_read_ce(1'b1),
    .if_read   (burst_read_addr_read),
    .if_dout   ({burst_read_addr_dout_burst_len, burst_read_addr_dout_addr})
  );

  detect_burst #(
    .AddrWidth        (AddrWidth),
    .DataWidthBytesLog(DataWidthBytesLog),
    .WaitTimeWidth    (WaitTimeWidth),
    .BurstLenWidth    (BurstLenWidth)
  ) detect_burst_read (
    .clk(clk),
    .rst(rst),

    .max_wait_time(max_wait_time),
    .max_burst_len(max_burst_len),

    // input: individual addresses
    .addr_dout   (read_addr_dout),
    .addr_empty_n(read_addr_empty_n),
    .addr_read   (read_addr_read),

    // output: inferred burst addresses
    .addr_din   (burst_read_addr_din),
    .addr_full_n(burst_read_addr_full_n),
    .addr_write (burst_read_addr_write),

    // output: used to generate the "last" signals, unused
    .burst_len_0_din   (),
    .burst_len_0_full_n(1'b1),
    .burst_len_0_write (),

    // output: used to generate write responses, unused
    .burst_len_1_din   (),
    .burst_len_1_full_n(1'b1),
    .burst_len_1_write ()
  );

  // read resp buffer
  wire [DataWidth-1:0] read_data_din;
  wire                 read_data_write;
  wire                 read_data_full_n;
  fifo_srl_with_almost_full #(
    .DATA_WIDTH(DataWidth),
    .BASE_ADDR_WIDTH(BufferSizeLog),
    .BASE_DEPTH     (BufferSize),
    .GRACE_PERIOD (FlowControlBufferDepth)
  ) read_data (
    .clk  (clk),
    .reset(rst),

    // from axi
    .if_full_n  (read_data_full_n),
    .if_almost_full_n (read_data_fifo_almost_full_n),
    .if_write_ce(1'b1),
    .if_write   (read_data_write),
    .if_din     (read_data_din),

    // to user
    .if_empty_n(read_data_empty_n),
    .if_read_ce(1'b1),
    .if_read   (read_data_read),
    .if_dout   (read_data_dout)
  );

  // AR channel
  assign burst_read_addr_read = m_axi_ARREADY_internal;
  assign m_axi_ARVALID_internal        = burst_read_addr_empty_n;
  assign m_axi_ARADDR         = burst_read_addr_dout_addr;
  assign m_axi_ARID           = 0;
  assign m_axi_ARLEN          = burst_read_addr_dout_burst_len;
  assign m_axi_ARSIZE         = DataWidthBytesLog;
  assign m_axi_ARBURST        = 1;        // INCR mode
  assign m_axi_ARLOCK         = 0;        // Xilinx only supports 0
  assign m_axi_ARCACHE        = 4'b0011;  // Xilinx only supports 4'b0011
  assign m_axi_ARPROT         = 0;
  assign m_axi_ARQOS          = 0;

  // R channel
  assign m_axi_RREADY    = read_data_full_n;
  assign read_data_write = m_axi_RVALID;
  assign read_data_din   = m_axi_RDATA;

  // unused input signals
  wire _unused = &{1'b0,
    m_axi_BRESP,
    m_axi_BID,
    m_axi_RLAST,
    m_axi_RID,
    m_axi_RRESP,
    1'b0};

endmodule  // async_mmap

`default_nettype wire

///////////////////////////////////////////////////////////

module fifo_srl_with_almost_full (
  input wire clk,
  input wire reset,

  // write
  output wire                  if_full_n,
  output wire                  if_almost_full_n,
  input  wire                  if_write_ce,
  input  wire                  if_write,
  input  wire [DATA_WIDTH-1:0] if_din,

  // read
  output wire                  if_empty_n,
  input  wire                  if_read_ce,
  input  wire                  if_read,
  output wire [DATA_WIDTH-1:0] if_dout
);

parameter MEM_STYLE   = "shiftreg";
parameter DATA_WIDTH  = 32'd32;
parameter BASE_ADDR_WIDTH  = 32'd4;
parameter BASE_DEPTH       = 5'd16;

/*******************************************/
parameter GRACE_PERIOD = 32;
parameter DEPTH = GRACE_PERIOD + BASE_DEPTH;
parameter ADDR_WIDTH  = $clog2(DEPTH);
/*******************************************/

wire[ADDR_WIDTH - 1:0] shiftReg_addr ;
wire[DATA_WIDTH - 1:0] shiftReg_data, shiftReg_q;
wire                     shiftReg_ce;
reg[ADDR_WIDTH:0] mOutPtr = ~{(ADDR_WIDTH+1){1'b0}};
reg internal_empty_n = 0, internal_full_n = 1;

assign if_empty_n = internal_empty_n;

/*******************************************/
assign if_full_n = internal_full_n;
wire almost_full = mOutPtr >= DEPTH - 1 - GRACE_PERIOD && mOutPtr != ~{ADDR_WIDTH+1{1'b0}};
assign if_almost_full_n = ~almost_full;
/*******************************************/

assign shiftReg_data = if_din;
assign if_dout = shiftReg_q;

always @ (posedge clk) begin
    if (reset == 1'b1)
    begin
        mOutPtr <= ~{ADDR_WIDTH+1{1'b0}};
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
            if (mOutPtr == DEPTH - 5'd2)
                internal_full_n <= 1'b0;
        end
    end
end

assign shiftReg_addr = mOutPtr[ADDR_WIDTH] == 1'b0 ? mOutPtr[ADDR_WIDTH-1:0]:{ADDR_WIDTH{1'b0}};
assign shiftReg_ce = (if_write & if_write_ce) & internal_full_n;

fifo_srl_almost_full_internal
#(
    .DATA_WIDTH(DATA_WIDTH),
    .ADDR_WIDTH(ADDR_WIDTH),
    .DEPTH(DEPTH))
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

reg[DATA_WIDTH-1:0] SRL_SIG [0:DEPTH-1];
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
