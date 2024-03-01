`default_nettype none

module async_mmap #(
  parameter BufferSize        = 32,
  parameter BufferSizeLog     = 5,
  parameter AddrWidth         = 64,
  parameter AxiSideAddrWidth  = 64,
  parameter DataWidth         = 512,
  parameter DataWidthBytesLog = 6,  // must equal log2(DataWidth/8)
  parameter WaitTimeWidth     = 4,
  parameter BurstLenWidth     = 8,
  // implement the FIFOs for the read channel
  // if set to 0: disconnect the data link
  parameter EnableReadChannel = 1,
  parameter EnableWriteChannel= 1,
  // for burst inference
  parameter MaxWaitTime       = 3,
  parameter MaxBurstLen       = 15
) (
  (* RS_CLK *) input wire clk,
  (* RS_RST *) input wire rst, // active high

  // base address for the memory region
  (* RS_FF = "offset" *) input wire [63:0] offset,

  // axi write addr channel
  (* RS_HS = "aw.valid" *) output wire                        m_axi_AWVALID,
  (* RS_HS = "aw.ready" *) input  wire                        m_axi_AWREADY,
  (* RS_HS = "aw.data" *)  output wire [AxiSideAddrWidth-1:0] m_axi_AWADDR,
  (* RS_HS = "aw.data" *)  output wire [0:0]                  m_axi_AWID,
  (* RS_HS = "aw.data" *)  output wire [7:0]                  m_axi_AWLEN,
  (* RS_HS = "aw.data" *)  output wire [2:0]                  m_axi_AWSIZE,
  (* RS_HS = "aw.data" *)  output wire [1:0]                  m_axi_AWBURST,
  (* RS_HS = "aw.data" *)  output wire [0:0]                  m_axi_AWLOCK,
  (* RS_HS = "aw.data" *)  output wire [3:0]                  m_axi_AWCACHE,
  (* RS_HS = "aw.data" *)  output wire [2:0]                  m_axi_AWPROT,
  (* RS_HS = "aw.data" *)  output wire [3:0]                  m_axi_AWQOS,

  // axi write data channel
  (* RS_HS = "w.valid" *) output wire                   m_axi_WVALID,
  (* RS_HS = "w.ready" *) input  wire                   m_axi_WREADY,
  (* RS_HS = "w.data" *)  output wire [DataWidth-1:0]   m_axi_WDATA,
  (* RS_HS = "w.data" *)  output wire [DataWidth/8-1:0] m_axi_WSTRB,
  (* RS_HS = "w.data" *)  output wire                   m_axi_WLAST,

  // axi write acknowledge channel
  (* RS_HS = "b.valid" *) input  wire       m_axi_BVALID,
  (* RS_HS = "b.ready" *) output wire       m_axi_BREADY,
  (* RS_HS = "b.data" *)  input  wire [1:0] m_axi_BRESP,
  (* RS_HS = "b.data" *)  input  wire [0:0] m_axi_BID,

  // axi read addr channel
  (* RS_HS = "ar.valid" *) output wire                        m_axi_ARVALID,
  (* RS_HS = "ar.ready" *) input  wire                        m_axi_ARREADY,
  (* RS_HS = "ar.data" *)  output wire [AxiSideAddrWidth-1:0] m_axi_ARADDR,
  (* RS_HS = "ar.data" *)  output wire [0:0]                  m_axi_ARID,
  (* RS_HS = "ar.data" *)  output wire [7:0]                  m_axi_ARLEN,
  (* RS_HS = "ar.data" *)  output wire [2:0]                  m_axi_ARSIZE,
  (* RS_HS = "ar.data" *)  output wire [1:0]                  m_axi_ARBURST,
  (* RS_HS = "ar.data" *)  output wire [0:0]                  m_axi_ARLOCK,
  (* RS_HS = "ar.data" *)  output wire [3:0]                  m_axi_ARCACHE,
  (* RS_HS = "ar.data" *)  output wire [2:0]                  m_axi_ARPROT,
  (* RS_HS = "ar.data" *)  output wire [3:0]                  m_axi_ARQOS,

  // axi read response channel
  (* RS_HS = "r.valid" *) input  wire                 m_axi_RVALID,
  (* RS_HS = "r.ready" *) output wire                 m_axi_RREADY,
  (* RS_HS = "r.data" *)  input  wire [DataWidth-1:0] m_axi_RDATA,
  (* RS_HS = "r.data" *)  input  wire                 m_axi_RLAST,
  (* RS_HS = "r.data" *)  input  wire [0:0]           m_axi_RID,
  (* RS_HS = "r.data" *)  input  wire [1:0]           m_axi_RRESP,


  // push read addr here
  (* RS_HS = "read_addr.data" *)  input  wire [AddrWidth-1:0] read_addr_din,
  (* RS_HS = "read_addr.valid" *) input  wire                 read_addr_write,
  (* RS_HS = "read_addr.ready" *) output wire                 read_addr_full_n,

  // pop read resp here
  (* RS_HS = "read_data.data" *)  output wire [DataWidth-1:0] read_data_dout,
  (* RS_HS = "read_data.ready" *) input  wire                 read_data_read,
  (* RS_HS = "read_data.valid" *) output wire                 read_data_empty_n,

  // push write addr and data here
  (* RS_HS = "write_addr.data" *)  input  wire [AddrWidth-1:0] write_addr_din,
  (* RS_HS = "write_addr.valid" *) input  wire                 write_addr_write,
  (* RS_HS = "write_addr.ready" *) output wire                 write_addr_full_n,
  (* RS_HS = "write_data.data" *)  input  wire [DataWidth-1:0] write_data_din,
  (* RS_HS = "write_data.valid" *) input  wire                 write_data_write,
  (* RS_HS = "write_data.ready" *) output wire                 write_data_full_n,

  // pop write resp here
  (* RS_HS = "write_resp.data" *)  output wire [7:0] write_resp_dout,
  (* RS_HS = "write_resp.ready" *) input  wire       write_resp_read,
  (* RS_HS = "write_resp.valid" *) output wire       write_resp_empty_n
);

  // write addr buffer, from user to burst detector
  wire [AddrWidth-1:0] write_addr_dout;
  wire                 write_addr_empty_n;
  wire                 write_addr_read;

  relay_station #(
    .DATA_WIDTH(AddrWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize),
    .LEVEL     (1),
    .CONNECT   (EnableWriteChannel)
  ) write_addr (
    .clk  (clk),
    .reset(rst),

    // from user
    .if_full_n  (write_addr_full_n),
    .if_write_ce(1'b1),
    .if_write   (write_addr_write),
    .if_din     (offset + (write_addr_din << $clog2(DataWidth/8))),

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

    .max_wait_time(MaxWaitTime[WaitTimeWidth-1:0]),
    .max_burst_len(MaxBurstLen[BurstLenWidth-1:0]),

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

  relay_station #(
    .DATA_WIDTH(BurstLenWidth + AddrWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize),
    .LEVEL     (1),
    .CONNECT   (EnableWriteChannel)
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

  relay_station #(
    .DATA_WIDTH(BurstLenWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize),
    .LEVEL     (1),
    .CONNECT   (EnableWriteChannel)
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
  relay_station #(
    .DATA_WIDTH(BurstLenWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize),
    .LEVEL     (1),
    .CONNECT   (EnableWriteChannel)
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

  // write data buffer
  wire [DataWidth-1:0] write_data_dout;
  wire                 write_data_empty_n;
  relay_station #(
    .DATA_WIDTH(DataWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize),
    .LEVEL     (1),
    .CONNECT   (EnableWriteChannel)
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
    // deal with when data-relay_station is non empty but last-relay_station is empty
    .if_read   (m_axi_WREADY && burst_write_last_empty_n),
    .if_dout   (write_data_dout)
  );

  // this relay_station should be synchronized with the wr_data relay_station
  relay_station #(
    .DATA_WIDTH(1),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize),
    .LEVEL     (1),
    .CONNECT   (EnableWriteChannel)
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
    // deal with when last-relay_station is non-empty while data-relay_station is empty
    .if_read   (m_axi_WREADY && write_data_empty_n),
    .if_dout   (burst_write_last_dout)
  );

  // write resp buffer
  wire [BurstLenWidth-1:0] write_resp_din   = write_req_dout;
  wire                     write_resp_write = m_axi_BVALID && write_req_empty_n;
  wire                     write_resp_full_n;
  relay_station #(
    .DATA_WIDTH(BurstLenWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize),
    .LEVEL     (1),
    .CONNECT   (EnableWriteChannel)
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
  assign m_axi_AWADDR   = {{(AxiSideAddrWidth - AddrWidth){1'b0}}, burst_write_addr_dout_addr};
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

  relay_station #(
    .DATA_WIDTH(AddrWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize),
    .LEVEL     (1),
    .CONNECT   (EnableReadChannel)
  ) read_addr (
    .clk  (clk),
    .reset(rst),

    // from user
    .if_full_n  (read_addr_full_n),
    .if_write_ce(1'b1),
    .if_write   (read_addr_write),
    .if_din     (offset + (read_addr_din << $clog2(DataWidth/8))),

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

  relay_station #(
    .DATA_WIDTH(BurstLenWidth + AddrWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize),
    .LEVEL     (1),
    .CONNECT   (EnableReadChannel)
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

    .max_wait_time(MaxWaitTime[WaitTimeWidth-1:0]),
    .max_burst_len(MaxBurstLen[BurstLenWidth-1:0]),

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
  relay_station #(
    .DATA_WIDTH(DataWidth),
    .ADDR_WIDTH(BufferSizeLog),
    .DEPTH     (BufferSize),
    .LEVEL     (1),
    .CONNECT   (EnableReadChannel)
  ) read_data (
    .clk  (clk),
    .reset(rst),

    // from axi
    .if_full_n  (read_data_full_n),
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
  assign burst_read_addr_read = m_axi_ARREADY;
  assign m_axi_ARVALID        = burst_read_addr_empty_n;
  assign m_axi_ARADDR         = {{(AxiSideAddrWidth - AddrWidth){1'b0}}, burst_read_addr_dout_addr};
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
