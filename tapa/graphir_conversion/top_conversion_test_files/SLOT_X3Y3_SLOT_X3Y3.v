// ==============================================================
// Generated by Vitis HLS v2024.2
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// ==============================================================

(* CORE_GENERATION_INFO="SLOT_X3Y3_SLOT_X3Y3_SLOT_X3Y3_SLOT_X3Y3,hls_ip_2024_2,{HLS_INPUT_TYPE=cxx,HLS_INPUT_FLOAT=0,HLS_INPUT_FIXED=0,HLS_INPUT_PART=xcu250-figd2104-2L-e,HLS_INPUT_CLOCK=3.330000,HLS_INPUT_ARCH=others,HLS_SYN_CLOCK=1.215450,HLS_SYN_LAT=1,HLS_SYN_TPT=none,HLS_SYN_MEM=0,HLS_SYN_DSP=0,HLS_SYN_FF=2,HLS_SYN_LUT=34,HLS_VERSION=2024_2}" *)
module SLOT_X3Y3_SLOT_X3Y3 (
        ap_clk,
        ap_rst_n,
        ap_start,
        ap_done,
        ap_idle,
        ap_ready,
        b,
        n,
        a_q_VecAdd_s_dout,
        a_q_VecAdd_s_empty_n,
        a_q_VecAdd_s_read,
        a_q_VecAdd_peek,
        c_q_VecAdd_s_din,
        c_q_VecAdd_s_full_n,
        c_q_VecAdd_s_write,
        c_q_VecAdd_peek,
        mmap_Mmap2Stream_1
,
  m_axi_mmap_Mmap2Stream_1_ARADDR,
  m_axi_mmap_Mmap2Stream_1_ARBURST,
  m_axi_mmap_Mmap2Stream_1_ARCACHE,
  m_axi_mmap_Mmap2Stream_1_ARID,
  m_axi_mmap_Mmap2Stream_1_ARLEN,
  m_axi_mmap_Mmap2Stream_1_ARLOCK,
  m_axi_mmap_Mmap2Stream_1_ARPROT,
  m_axi_mmap_Mmap2Stream_1_ARQOS,
  m_axi_mmap_Mmap2Stream_1_ARREADY,
  m_axi_mmap_Mmap2Stream_1_ARSIZE,
  m_axi_mmap_Mmap2Stream_1_ARVALID,
  m_axi_mmap_Mmap2Stream_1_AWADDR,
  m_axi_mmap_Mmap2Stream_1_AWBURST,
  m_axi_mmap_Mmap2Stream_1_AWCACHE,
  m_axi_mmap_Mmap2Stream_1_AWID,
  m_axi_mmap_Mmap2Stream_1_AWLEN,
  m_axi_mmap_Mmap2Stream_1_AWLOCK,
  m_axi_mmap_Mmap2Stream_1_AWPROT,
  m_axi_mmap_Mmap2Stream_1_AWQOS,
  m_axi_mmap_Mmap2Stream_1_AWREADY,
  m_axi_mmap_Mmap2Stream_1_AWSIZE,
  m_axi_mmap_Mmap2Stream_1_AWVALID,
  m_axi_mmap_Mmap2Stream_1_BID,
  m_axi_mmap_Mmap2Stream_1_BREADY,
  m_axi_mmap_Mmap2Stream_1_BRESP,
  m_axi_mmap_Mmap2Stream_1_BVALID,
  m_axi_mmap_Mmap2Stream_1_RDATA,
  m_axi_mmap_Mmap2Stream_1_RID,
  m_axi_mmap_Mmap2Stream_1_RLAST,
  m_axi_mmap_Mmap2Stream_1_RREADY,
  m_axi_mmap_Mmap2Stream_1_RRESP,
  m_axi_mmap_Mmap2Stream_1_RVALID,
  m_axi_mmap_Mmap2Stream_1_WDATA,
  m_axi_mmap_Mmap2Stream_1_WLAST,
  m_axi_mmap_Mmap2Stream_1_WREADY,
  m_axi_mmap_Mmap2Stream_1_WSTRB,
  m_axi_mmap_Mmap2Stream_1_WVALID);
input   ap_clk;
input   ap_rst_n;
input   ap_start;
output   ap_done;
output   ap_idle;
output   ap_ready;
input  [63:0] b;
input  [63:0] n;
input  [32:0] a_q_VecAdd_s_dout;
input   a_q_VecAdd_s_empty_n;
output   a_q_VecAdd_s_read;
input  [32:0] a_q_VecAdd_peek;
output  [32:0] c_q_VecAdd_s_din;
input   c_q_VecAdd_s_full_n;
output   c_q_VecAdd_s_write;
input  [32:0] c_q_VecAdd_peek;
input  [63:0] mmap_Mmap2Stream_1;
  output [63:0] m_axi_mmap_Mmap2Stream_1_ARADDR;
  output [1:0] m_axi_mmap_Mmap2Stream_1_ARBURST;
  output [3:0] m_axi_mmap_Mmap2Stream_1_ARCACHE;
  output [0:0] m_axi_mmap_Mmap2Stream_1_ARID;
  output [7:0] m_axi_mmap_Mmap2Stream_1_ARLEN;
  output m_axi_mmap_Mmap2Stream_1_ARLOCK;
  output [2:0] m_axi_mmap_Mmap2Stream_1_ARPROT;
  output [3:0] m_axi_mmap_Mmap2Stream_1_ARQOS;
  input m_axi_mmap_Mmap2Stream_1_ARREADY;
  output [2:0] m_axi_mmap_Mmap2Stream_1_ARSIZE;
  output m_axi_mmap_Mmap2Stream_1_ARVALID;
  output [63:0] m_axi_mmap_Mmap2Stream_1_AWADDR;
  output [1:0] m_axi_mmap_Mmap2Stream_1_AWBURST;
  output [3:0] m_axi_mmap_Mmap2Stream_1_AWCACHE;
  output [0:0] m_axi_mmap_Mmap2Stream_1_AWID;
  output [7:0] m_axi_mmap_Mmap2Stream_1_AWLEN;
  output m_axi_mmap_Mmap2Stream_1_AWLOCK;
  output [2:0] m_axi_mmap_Mmap2Stream_1_AWPROT;
  output [3:0] m_axi_mmap_Mmap2Stream_1_AWQOS;
  input m_axi_mmap_Mmap2Stream_1_AWREADY;
  output [2:0] m_axi_mmap_Mmap2Stream_1_AWSIZE;
  output m_axi_mmap_Mmap2Stream_1_AWVALID;
  input [0:0] m_axi_mmap_Mmap2Stream_1_BID;
  output m_axi_mmap_Mmap2Stream_1_BREADY;
  input [1:0] m_axi_mmap_Mmap2Stream_1_BRESP;
  input m_axi_mmap_Mmap2Stream_1_BVALID;
  input [31:0] m_axi_mmap_Mmap2Stream_1_RDATA;
  input [0:0] m_axi_mmap_Mmap2Stream_1_RID;
  input m_axi_mmap_Mmap2Stream_1_RLAST;
  output m_axi_mmap_Mmap2Stream_1_RREADY;
  input [1:0] m_axi_mmap_Mmap2Stream_1_RRESP;
  input m_axi_mmap_Mmap2Stream_1_RVALID;
  output [31:0] m_axi_mmap_Mmap2Stream_1_WDATA;
  output m_axi_mmap_Mmap2Stream_1_WLAST;
  input m_axi_mmap_Mmap2Stream_1_WREADY;
  output [3:0] m_axi_mmap_Mmap2Stream_1_WSTRB;
  output m_axi_mmap_Mmap2Stream_1_WVALID;

reg    ap_block_state1;
reg    ap_ST_fsm_state1_blk;
reg    ap_ST_fsm_state2_blk;
wire    ap_ce_reg;
  wire ap_rst_n_inv;
  wire ap_done;
  wire ap_idle;
  wire ap_ready;
  wire [32:0] a_q_VecAdd__dout;
  wire a_q_VecAdd__empty_n;
  wire a_q_VecAdd__read;
  wire [32:0] b_q_VecAdd__dout;
  wire b_q_VecAdd__empty_n;
  wire b_q_VecAdd__read;
  wire [32:0] b_q_VecAdd__din;
  wire b_q_VecAdd__full_n;
  wire b_q_VecAdd__write;
  wire [32:0] c_q_VecAdd__din;
  wire c_q_VecAdd__full_n;
  wire c_q_VecAdd__write;
  wire [63:0] Add_0___n__q0;
  wire Add_0__ap_start;
  wire Add_0__ap_ready;
  wire Add_0__ap_done;
  wire Add_0__ap_idle;
  wire [63:0] Mmap2Stream_0___mmap_Mmap2Stream_1__q0;
  wire [63:0] Mmap2Stream_0___n__q0;
  wire Mmap2Stream_0__ap_start;
  wire Mmap2Stream_0__ap_ready;
  wire Mmap2Stream_0__ap_done;
  wire Mmap2Stream_0__ap_idle;
// power-on initialization
  assign ap_rst_n_inv = (~ap_rst_n);

fifo
#(
  .DATA_WIDTH(32 - 0 + 1),
  .ADDR_WIDTH(1),
  .DEPTH(2)
)
b_q_VecAdd
(
  .clk(ap_clk),
  .reset(~ap_rst_n),
  .if_dout(b_q_VecAdd__dout),
  .if_empty_n(b_q_VecAdd__empty_n),
  .if_read(b_q_VecAdd__read),
  .if_read_ce(1'b1),
  .if_din(b_q_VecAdd__din),
  .if_full_n(b_q_VecAdd__full_n),
  .if_write(b_q_VecAdd__write),
  .if_write_ce(1'b1)
);
  assign a_q_VecAdd__dout = a_q_VecAdd_s_dout;
  assign a_q_VecAdd__empty_n = a_q_VecAdd_s_empty_n;
  assign a_q_VecAdd_s_read = a_q_VecAdd__read;
  assign c_q_VecAdd_s_din = c_q_VecAdd__din;
  assign c_q_VecAdd__full_n = c_q_VecAdd_s_full_n;
  assign c_q_VecAdd_s_write = c_q_VecAdd__write;

Add
Add_0
(
  .ap_clk(ap_clk),
  .ap_rst_n(ap_rst_n),
  .ap_start(Add_0__ap_start),
  .ap_done(Add_0__ap_done),
  .ap_idle(Add_0__ap_idle),
  .ap_ready(Add_0__ap_ready),
  .a_s_dout(a_q_VecAdd__dout),
  .a_peek_dout(a_q_VecAdd__dout),
  .a_s_empty_n(a_q_VecAdd__empty_n),
  .a_peek_empty_n(a_q_VecAdd__empty_n),
  .a_s_read(a_q_VecAdd__read),
  .b_s_dout(b_q_VecAdd__dout),
  .b_peek_dout(b_q_VecAdd__dout),
  .b_s_empty_n(b_q_VecAdd__empty_n),
  .b_peek_empty_n(b_q_VecAdd__empty_n),
  .b_s_read(b_q_VecAdd__read),
  .c_s_din(c_q_VecAdd__din),
  .c_s_full_n(c_q_VecAdd__full_n),
  .c_s_write(c_q_VecAdd__write),
  .n(Add_0___n__q0)
);

Mmap2Stream
Mmap2Stream_0
(
  .ap_clk(ap_clk),
  .ap_rst_n(ap_rst_n),
  .ap_start(Mmap2Stream_0__ap_start),
  .ap_done(Mmap2Stream_0__ap_done),
  .ap_idle(Mmap2Stream_0__ap_idle),
  .ap_ready(Mmap2Stream_0__ap_ready),
  .stream_s_din(b_q_VecAdd__din),
  .stream_s_full_n(b_q_VecAdd__full_n),
  .stream_s_write(b_q_VecAdd__write),
  .m_axi_mmap_ARADDR(m_axi_mmap_Mmap2Stream_1_ARADDR),
  .m_axi_mmap_ARBURST(m_axi_mmap_Mmap2Stream_1_ARBURST),
  .m_axi_mmap_ARID(m_axi_mmap_Mmap2Stream_1_ARID),
  .m_axi_mmap_ARLEN(m_axi_mmap_Mmap2Stream_1_ARLEN),
  .m_axi_mmap_ARREADY(m_axi_mmap_Mmap2Stream_1_ARREADY),
  .m_axi_mmap_ARSIZE(m_axi_mmap_Mmap2Stream_1_ARSIZE),
  .m_axi_mmap_ARVALID(m_axi_mmap_Mmap2Stream_1_ARVALID),
  .m_axi_mmap_AWADDR(m_axi_mmap_Mmap2Stream_1_AWADDR),
  .m_axi_mmap_AWBURST(m_axi_mmap_Mmap2Stream_1_AWBURST),
  .m_axi_mmap_AWID(m_axi_mmap_Mmap2Stream_1_AWID),
  .m_axi_mmap_AWLEN(m_axi_mmap_Mmap2Stream_1_AWLEN),
  .m_axi_mmap_AWREADY(m_axi_mmap_Mmap2Stream_1_AWREADY),
  .m_axi_mmap_AWSIZE(m_axi_mmap_Mmap2Stream_1_AWSIZE),
  .m_axi_mmap_AWVALID(m_axi_mmap_Mmap2Stream_1_AWVALID),
  .m_axi_mmap_BID(m_axi_mmap_Mmap2Stream_1_BID),
  .m_axi_mmap_BREADY(m_axi_mmap_Mmap2Stream_1_BREADY),
  .m_axi_mmap_BRESP(m_axi_mmap_Mmap2Stream_1_BRESP),
  .m_axi_mmap_BVALID(m_axi_mmap_Mmap2Stream_1_BVALID),
  .m_axi_mmap_RDATA(m_axi_mmap_Mmap2Stream_1_RDATA),
  .m_axi_mmap_RID(m_axi_mmap_Mmap2Stream_1_RID),
  .m_axi_mmap_RLAST(m_axi_mmap_Mmap2Stream_1_RLAST),
  .m_axi_mmap_RREADY(m_axi_mmap_Mmap2Stream_1_RREADY),
  .m_axi_mmap_RRESP(m_axi_mmap_Mmap2Stream_1_RRESP),
  .m_axi_mmap_RVALID(m_axi_mmap_Mmap2Stream_1_RVALID),
  .m_axi_mmap_WDATA(m_axi_mmap_Mmap2Stream_1_WDATA),
  .m_axi_mmap_WLAST(m_axi_mmap_Mmap2Stream_1_WLAST),
  .m_axi_mmap_WREADY(m_axi_mmap_Mmap2Stream_1_WREADY),
  .m_axi_mmap_WSTRB(m_axi_mmap_Mmap2Stream_1_WSTRB),
  .m_axi_mmap_WVALID(m_axi_mmap_Mmap2Stream_1_WVALID),
  .m_axi_mmap_ARLOCK(m_axi_mmap_Mmap2Stream_1_ARLOCK),
  .m_axi_mmap_ARPROT(m_axi_mmap_Mmap2Stream_1_ARPROT),
  .m_axi_mmap_ARQOS(m_axi_mmap_Mmap2Stream_1_ARQOS),
  .m_axi_mmap_ARCACHE(m_axi_mmap_Mmap2Stream_1_ARCACHE),
  .m_axi_mmap_AWCACHE(m_axi_mmap_Mmap2Stream_1_AWCACHE),
  .m_axi_mmap_AWLOCK(m_axi_mmap_Mmap2Stream_1_AWLOCK),
  .m_axi_mmap_AWPROT(m_axi_mmap_Mmap2Stream_1_AWPROT),
  .m_axi_mmap_AWQOS(m_axi_mmap_Mmap2Stream_1_AWQOS),
  .mmap_offset(Mmap2Stream_0___mmap_Mmap2Stream_1__q0),
  .n(Mmap2Stream_0___n__q0)
);

SLOT_X3Y3_SLOT_X3Y3_fsm
__tapa_fsm_unit
(
  .ap_clk(ap_clk),
  .ap_rst_n(ap_rst_n),
  .ap_start(ap_start),
  .ap_done(ap_done),
  .ap_idle(ap_idle),
  .ap_ready(ap_ready),
  .n(n),
  .mmap_Mmap2Stream_1(mmap_Mmap2Stream_1),
  .Add_0___n__q0(Add_0___n__q0),
  .Add_0__ap_start(Add_0__ap_start),
  .Add_0__ap_ready(Add_0__ap_ready),
  .Add_0__ap_done(Add_0__ap_done),
  .Add_0__ap_idle(Add_0__ap_idle),
  .Mmap2Stream_0___mmap_Mmap2Stream_1__q0(Mmap2Stream_0___mmap_Mmap2Stream_1__q0),
  .Mmap2Stream_0___n__q0(Mmap2Stream_0___n__q0),
  .Mmap2Stream_0__ap_start(Mmap2Stream_0__ap_start),
  .Mmap2Stream_0__ap_ready(Mmap2Stream_0__ap_ready),
  .Mmap2Stream_0__ap_done(Mmap2Stream_0__ap_done),
  .Mmap2Stream_0__ap_idle(Mmap2Stream_0__ap_idle)
);
endmodule
