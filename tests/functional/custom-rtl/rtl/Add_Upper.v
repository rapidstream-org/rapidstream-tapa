// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

// Custom rtl file

`timescale 1 ns / 1 ps

(* CORE_GENERATION_INFO = "Add_Upper_Add_Upper,hls_ip_2024_1,{HLS_INPUT_TYPE=cxx,HLS_INPUT_FLOAT=0,HLS_INPUT_FIXED=0,HLS_INPUT_PART=xcu250-figd2104-2L-e,HLS_INPUT_CLOCK=3.333000,HLS_INPUT_ARCH=others,HLS_SYN_CLOCK=1.216545,HLS_SYN_LAT=1,HLS_SYN_TPT=none,HLS_SYN_MEM=0,HLS_SYN_DSP=0,HLS_SYN_FF=2,HLS_SYN_LUT=45,HLS_VERSION=2024_1}" *)


module Add_Upper
(
  ap_clk,
  ap_rst_n,
  ap_start,
  ap_done,
  ap_idle,
  ap_ready,
  a_s_empty_n,
  a_s_read,
  a_peek_empty_n,
  a_peek_read,
  b_s_empty_n,
  b_s_read,
  b_peek_empty_n,
  b_peek_read,
  c_full_n,
  c_write,
  n,
  a_s_dout,
  a_s_dout_eot,
  a_peek_dout,
  a_peek_dout_eot,
  b_s_dout,
  b_s_dout_eot,
  b_peek_dout,
  b_peek_dout_eot,
  c_din,
  c_din_eot
);

  input ap_clk;
  input ap_rst_n;
  input ap_start;
  output ap_done;
  output ap_idle;
  output ap_ready;
  input a_s_empty_n;
  output a_s_read;
  input a_peek_empty_n;
  output a_peek_read;
  input b_s_empty_n;
  output b_s_read;
  input b_peek_empty_n;
  output b_peek_read;
  input c_full_n;
  output c_write;
  input [63:0] n;
  input [32-1:0] a_s_dout;
  input a_s_dout_eot;
  input [32-1:0] a_peek_dout;
  input a_peek_dout_eot;
  input [32-1:0] b_s_dout;
  input b_s_dout_eot;
  input [32-1:0] b_peek_dout;
  input b_peek_dout_eot;
  output [32-1:0] c_din;
  output c_din_eot;
  reg ap_block_state2;
  reg ap_block_state1;
  reg c_write_local;
  reg a_s_read_local;
  reg b_s_read_local;
  reg ap_ST_fsm_state1_blk;
  reg ap_ST_fsm_state2_blk;
  wire ap_ce_reg;
  wire [32-1:0] a__dout;
  wire a__dout_eot;
  wire a__empty_n;
  wire a__read;
  wire [32-1:0] b__dout;
  wire b__dout_eot;
  wire b__empty_n;
  wire b__read;
  wire [32-1:0] c__din;
  wire c__din_eot;
  wire c__full_n;
  wire c__write;
  wire [63:0] Add_0___n__q0;
  wire Add_0__ap_start;
  wire Add_0__ap_ready;
  wire Add_0__ap_done;
  wire Add_0__ap_idle;
  wire ap_rst_n_inv;
  wire ap_done;
  wire ap_idle;
  wire ap_ready;
  assign ap_rst_n_inv = (~ap_rst_n);
  assign a__dout = a_s_dout;
  assign a__empty_n = a_s_empty_n;
  assign a_s_read = a__read;
  assign a__dout_eot = a_s_dout_eot;
  assign b__dout = b_s_dout;
  assign b__empty_n = b_s_empty_n;
  assign b_s_read = b__read;
  assign b__dout_eot = b_s_dout_eot;
  assign c_din = c__din;
  assign c__full_n = c_full_n;
  assign c_write = c__write;
  assign c_din_eot = c__din_eot;

  Add
  Add_0
  (
    .ap_clk(ap_clk),
    .ap_rst_n(ap_rst_n),
    .ap_start(Add_0__ap_start),
    .ap_done(Add_0__ap_done),
    .ap_idle(Add_0__ap_idle),
    .ap_ready(Add_0__ap_ready),
    .a_s_dout({ a__dout_eot, a__dout }),
    .a_peek_dout({ a__dout_eot, a__dout }),
    .a_s_empty_n(a__empty_n),
    .a_peek_empty_n(a__empty_n),
    .a_s_read(a__read),
    .b_s_dout({ b__dout_eot, b__dout }),
    .b_peek_dout({ b__dout_eot, b__dout }),
    .b_s_empty_n(b__empty_n),
    .b_peek_empty_n(b__empty_n),
    .b_s_read(b__read),
    .c_din({ c__din_eot, c__din }),
    .c_full_n(c__full_n),
    .c_write(c__write),
    .n(Add_0___n__q0)
  );


  Add_Upper_fsm
  __tapa_fsm_unit
  (
    .ap_clk(ap_clk),
    .ap_rst_n(ap_rst_n),
    .ap_start(ap_start),
    .ap_done(ap_done),
    .ap_idle(ap_idle),
    .ap_ready(ap_ready),
    .n(n),
    .Add_0___n__q0(Add_0___n__q0),
    .Add_0__ap_start(Add_0__ap_start),
    .Add_0__ap_ready(Add_0__ap_ready),
    .Add_0__ap_done(Add_0__ap_done),
    .Add_0__ap_idle(Add_0__ap_idle)
  );


endmodule
