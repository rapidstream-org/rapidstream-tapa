// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

module axi_pipeline #(
  parameter
    C_M_AXI_ID_WIDTH        = 8,
    C_M_AXI_ADDR_WIDTH      = 32,
    C_M_AXI_DATA_WIDTH      = 512,
    C_M_AXI_WSTRB_WIDTH     = (512 / 8),

    PIPELINE_LEVEL          = 3,
    EnableReadChannel       = 1,
    EnableWriteChannel      = 1
)
(
  input                                          ap_clk,

  // pipeline in
  input                                          in_AWVALID,
  output                                         in_AWREADY,
  input   [C_M_AXI_ADDR_WIDTH - 1:0]             in_AWADDR,
  input   [1:0]                                  in_AWBURST,
  input   [7:0]                                  in_AWLEN,
  input   [2:0]                                  in_AWSIZE,
  input   [C_M_AXI_ID_WIDTH - 1:0]               in_AWID,

  input                                          in_WVALID,
  output                                         in_WREADY,
  input   [C_M_AXI_DATA_WIDTH - 1:0]             in_WDATA,
  input   [C_M_AXI_WSTRB_WIDTH - 1:0]            in_WSTRB,
  input                                          in_WLAST,

  output                                         in_BVALID,
  input                                          in_BREADY,
  output   [1:0]                                 in_BRESP,
  output   [C_M_AXI_ID_WIDTH - 1:0]              in_BID,

  input                                          in_ARVALID,
  output                                         in_ARREADY,
  input   [C_M_AXI_ADDR_WIDTH - 1:0]             in_ARADDR,
  input   [1:0]                                  in_ARBURST,
  input   [7:0]                                  in_ARLEN,
  input   [2:0]                                  in_ARSIZE,
  input   [C_M_AXI_ID_WIDTH - 1:0]               in_ARID,

  output                                         in_RVALID,
  input                                          in_RREADY,
  output   [C_M_AXI_DATA_WIDTH - 1:0]            in_RDATA,
  output                                         in_RLAST,
  output   [C_M_AXI_ID_WIDTH - 1:0]              in_RID,
  output   [1:0]                                 in_RRESP,

  // pipeline out
  output                                         out_AWVALID,
  input                                          out_AWREADY,
  output   [C_M_AXI_ADDR_WIDTH - 1:0]            out_AWADDR,
  output   [1:0]                                 out_AWBURST,
  output   [7:0]                                 out_AWLEN,
  output   [2:0]                                 out_AWSIZE,
  output   [C_M_AXI_ID_WIDTH - 1:0]              out_AWID,

  output                                         out_WVALID,
  input                                          out_WREADY,
  output   [C_M_AXI_DATA_WIDTH - 1:0]            out_WDATA,
  output   [C_M_AXI_WSTRB_WIDTH - 1:0]           out_WSTRB,
  output                                         out_WLAST,

  input                                          out_BVALID,
  output                                         out_BREADY,
  input   [1:0]                                  out_BRESP,
  input   [C_M_AXI_ID_WIDTH - 1:0]               out_BID,

  output                                         out_ARVALID,
  input                                          out_ARREADY,
  output   [C_M_AXI_ADDR_WIDTH - 1:0]            out_ARADDR,
  output   [1:0]                                 out_ARBURST,
  output   [7:0]                                 out_ARLEN,
  output   [2:0]                                 out_ARSIZE,
  output   [C_M_AXI_ID_WIDTH - 1:0]              out_ARID,

  input                                          out_RVALID,
  output                                         out_RREADY,
  input   [C_M_AXI_DATA_WIDTH - 1:0]             out_RDATA,
  input                                          out_RLAST,
  input   [C_M_AXI_ID_WIDTH - 1:0]               out_RID,
  input   [1:0]                                  out_RRESP
);

  relay_station
  #(
    .DATA_WIDTH     ( C_M_AXI_ADDR_WIDTH + C_M_AXI_ID_WIDTH + 8 + 3 + 2           ),
    .DEPTH          ( 2                                                           ),
    .ADDR_WIDTH     ( 1                                                           ),
    .LEVEL          ( PIPELINE_LEVEL                                              ),
    .CONNECT        ( EnableWriteChannel )
  )
  AW_pipeline
  (
    .clk            ( ap_clk                                                      ),
    .reset          ( 1'b0                                                        ),
    .if_read_ce     ( 1'b1                                                        ),
    .if_write_ce    ( 1'b1                                                        ),

    .if_din         ( {in_AWADDR,  in_AWID,  in_AWLEN,  in_AWSIZE,  in_AWBURST}   ),
    .if_full_n      ( in_AWREADY                                                  ),
    .if_write       ( in_AWVALID                                                  ),

    .if_dout        ( {out_AWADDR, out_AWID, out_AWLEN, out_AWSIZE, out_AWBURST}  ),
    .if_empty_n     ( out_AWVALID                                                 ),
    .if_read        ( out_AWREADY                                                 )
  );

  relay_station
  #(
    .DATA_WIDTH(
      C_M_AXI_DATA_WIDTH + C_M_AXI_WSTRB_WIDTH + 1
    ),
    .DEPTH(2),
    .ADDR_WIDTH(1),
    .LEVEL( PIPELINE_LEVEL ),
    .CONNECT        ( EnableWriteChannel )
  )
  W_pipeline
  (
    .clk        (ap_clk),
    .reset      ( 1'b0                  ),
    .if_read_ce (1'b1),
    .if_write_ce(1'b1),

    .if_din     ({in_WDATA,  in_WSTRB,  in_WLAST}),
    .if_full_n  ( in_WREADY),
    .if_write   ( in_WVALID),

    .if_dout    ({out_WDATA, out_WSTRB, out_WLAST}),
    .if_empty_n (out_WVALID),
    .if_read    (out_WREADY)
  );

  relay_station
  #(
    .DATA_WIDTH(
      C_M_AXI_ADDR_WIDTH + C_M_AXI_ID_WIDTH + 8 + 3 + 2
    ),
    .DEPTH(2),
    .ADDR_WIDTH(1),
    .LEVEL( PIPELINE_LEVEL ),
    .CONNECT( EnableReadChannel )
  )
  AR_pipeline
  (
    .clk        (ap_clk),
    .reset      (1'b0),
    .if_read_ce (1'b1),
    .if_write_ce(1'b1),

    .if_din     ({ in_ARADDR,  in_ARID,  in_ARLEN,  in_ARSIZE,  in_ARBURST}),
    .if_full_n  ( in_ARREADY),
    .if_write   ( in_ARVALID),

    .if_dout    ({out_ARADDR, out_ARID, out_ARLEN, out_ARSIZE, out_ARBURST}),
    .if_empty_n (out_ARVALID),
    .if_read    (out_ARREADY)
  );

  relay_station
  #(
    .DATA_WIDTH(
      C_M_AXI_DATA_WIDTH + 1 + C_M_AXI_ID_WIDTH + 2
    ),
    .DEPTH(2),
    .ADDR_WIDTH(1),
    .LEVEL( PIPELINE_LEVEL ),
    .CONNECT( EnableReadChannel )
  )
  R_pipeline
  (
    .clk        (ap_clk),
    .reset      (1'b0),
    .if_read_ce (1'b1),
    .if_write_ce(1'b1),

    .if_din     ({out_RDATA, out_RLAST, out_RID, out_RRESP}),
    .if_full_n  ( out_RREADY),
    .if_write   ( out_RVALID),

    .if_dout    ({in_RDATA,  in_RLAST,  in_RID, in_RRESP}),
    .if_empty_n (in_RVALID),
    .if_read    (in_RREADY)
  );

  relay_station
  #(
    .DATA_WIDTH(
      2 + C_M_AXI_ID_WIDTH
    ),
    .DEPTH(2),
    .ADDR_WIDTH(1),
    .LEVEL( PIPELINE_LEVEL ),
    .CONNECT( EnableWriteChannel )
  )
  B_pipeline
  (
    .clk        (ap_clk),
    .reset      (1'b0),
    .if_read_ce (1'b1),
    .if_write_ce(1'b1),

    .if_din     ({out_BRESP,  out_BID}),
    .if_full_n  (out_BREADY),
    .if_write   (out_BVALID),

    .if_dout    ({ in_BRESP,  in_BID}),
    .if_empty_n (in_BVALID),
    .if_read    (in_BREADY)
  );

endmodule
