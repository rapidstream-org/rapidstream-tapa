
module a_axi_write_broadcastor_1_to_3 (
  ap_clk,

  s_axi_control_AWVALID_slr_0,
  s_axi_control_AWREADY_slr_0,
  s_axi_control_AWADDR_slr_0,
  s_axi_control_WVALID_slr_0,
  s_axi_control_WREADY_slr_0,
  s_axi_control_WDATA_slr_0,
  s_axi_control_WSTRB_slr_0,
  s_axi_control_AWVALID_slr_1,
  s_axi_control_AWREADY_slr_1,
  s_axi_control_AWADDR_slr_1,
  s_axi_control_WVALID_slr_1,
  s_axi_control_WREADY_slr_1,
  s_axi_control_WDATA_slr_1,
  s_axi_control_WSTRB_slr_1,
  s_axi_control_AWVALID_slr_2,
  s_axi_control_AWREADY_slr_2,
  s_axi_control_AWADDR_slr_2,
  s_axi_control_WVALID_slr_2,
  s_axi_control_WREADY_slr_2,
  s_axi_control_WDATA_slr_2,
  s_axi_control_WSTRB_slr_2,
  s_axi_control_AWVALID,
  s_axi_control_AWREADY,
  s_axi_control_AWADDR,
  s_axi_control_WVALID,
  s_axi_control_WREADY,
  s_axi_control_WDATA,
  s_axi_control_WSTRB
);
  parameter C_S_AXI_CONTROL_DATA_WIDTH = 32;
  parameter C_S_AXI_CONTROL_ADDR_WIDTH = 9;
  parameter C_S_AXI_DATA_WIDTH = 32;
  parameter C_S_AXI_CONTROL_WSTRB_WIDTH = 32 / 8;
  parameter C_S_AXI_WSTRB_WIDTH = 32 / 8;

  input ap_clk;

  input s_axi_control_AWVALID;
  output s_axi_control_AWREADY;
  input [C_S_AXI_CONTROL_ADDR_WIDTH-1:0] s_axi_control_AWADDR;
  input s_axi_control_WVALID;
  output s_axi_control_WREADY;
  input [C_S_AXI_CONTROL_DATA_WIDTH-1:0] s_axi_control_WDATA;
  input [C_S_AXI_CONTROL_WSTRB_WIDTH-1:0] s_axi_control_WSTRB;

  output s_axi_control_AWVALID_slr_0;
  input s_axi_control_AWREADY_slr_0;
  output [C_S_AXI_CONTROL_ADDR_WIDTH-1:0] s_axi_control_AWADDR_slr_0;
  output s_axi_control_WVALID_slr_0;
  input s_axi_control_WREADY_slr_0;
  output [C_S_AXI_CONTROL_DATA_WIDTH-1:0] s_axi_control_WDATA_slr_0;
  output [C_S_AXI_CONTROL_WSTRB_WIDTH-1:0] s_axi_control_WSTRB_slr_0;

  output s_axi_control_AWVALID_slr_1;
  input s_axi_control_AWREADY_slr_1;
  output [C_S_AXI_CONTROL_ADDR_WIDTH-1:0] s_axi_control_AWADDR_slr_1;
  output s_axi_control_WVALID_slr_1;
  input s_axi_control_WREADY_slr_1;
  output [C_S_AXI_CONTROL_DATA_WIDTH-1:0] s_axi_control_WDATA_slr_1;
  output [C_S_AXI_CONTROL_WSTRB_WIDTH-1:0] s_axi_control_WSTRB_slr_1;

  output s_axi_control_AWVALID_slr_2;
  input s_axi_control_AWREADY_slr_2;
  output [C_S_AXI_CONTROL_ADDR_WIDTH-1:0] s_axi_control_AWADDR_slr_2;
  output s_axi_control_WVALID_slr_2;
  input s_axi_control_WREADY_slr_2;
  output [C_S_AXI_CONTROL_DATA_WIDTH-1:0] s_axi_control_WDATA_slr_2;
  output [C_S_AXI_CONTROL_WSTRB_WIDTH-1:0] s_axi_control_WSTRB_slr_2;

  wire s_axi_control_AWVALID_slr_0_inner;
  wire s_axi_control_AWREADY_slr_0_inner;
  wire [C_S_AXI_CONTROL_ADDR_WIDTH-1:0] s_axi_control_AWADDR_slr_0_inner;
  wire s_axi_control_WVALID_slr_0_inner;
  wire s_axi_control_WREADY_slr_0_inner;
  wire [C_S_AXI_CONTROL_DATA_WIDTH-1:0] s_axi_control_WDATA_slr_0_inner;
  wire [C_S_AXI_CONTROL_WSTRB_WIDTH-1:0] s_axi_control_WSTRB_slr_0_inner;

  wire s_axi_control_AWVALID_slr_1_inner;
  wire s_axi_control_AWREADY_slr_1_inner;
  wire [C_S_AXI_CONTROL_ADDR_WIDTH-1:0] s_axi_control_AWADDR_slr_1_inner;
  wire s_axi_control_WVALID_slr_1_inner;
  wire s_axi_control_WREADY_slr_1_inner;
  wire [C_S_AXI_CONTROL_DATA_WIDTH-1:0] s_axi_control_WDATA_slr_1_inner;
  wire [C_S_AXI_CONTROL_WSTRB_WIDTH-1:0] s_axi_control_WSTRB_slr_1_inner;

  wire s_axi_control_AWVALID_slr_2_inner;
  wire s_axi_control_AWREADY_slr_2_inner;
  wire [C_S_AXI_CONTROL_ADDR_WIDTH-1:0] s_axi_control_AWADDR_slr_2_inner;
  wire s_axi_control_WVALID_slr_2_inner;
  wire s_axi_control_WREADY_slr_2_inner;
  wire [C_S_AXI_CONTROL_DATA_WIDTH-1:0] s_axi_control_WDATA_slr_2_inner;
  wire [C_S_AXI_CONTROL_WSTRB_WIDTH-1:0] s_axi_control_WSTRB_slr_2_inner;

  // broadcast the AW channel
  assign s_axi_control_AWADDR_slr_0_inner = s_axi_control_AWADDR;
  assign s_axi_control_AWADDR_slr_1_inner = s_axi_control_AWADDR;
  assign s_axi_control_AWADDR_slr_2_inner = s_axi_control_AWADDR;

  assign s_axi_control_AWVALID_slr_0_inner = s_axi_control_AWVALID;
  assign s_axi_control_AWVALID_slr_1_inner = s_axi_control_AWVALID;
  assign s_axi_control_AWVALID_slr_2_inner = s_axi_control_AWVALID;

  assign s_axi_control_AWREADY =  s_axi_control_AWREADY_slr_0_inner &
                                  s_axi_control_AWREADY_slr_1_inner &
                                  s_axi_control_AWREADY_slr_2_inner;

  // broadcast the W channel
  assign s_axi_control_WDATA_slr_0_inner = s_axi_control_WDATA;
  assign s_axi_control_WDATA_slr_1_inner = s_axi_control_WDATA;
  assign s_axi_control_WDATA_slr_2_inner = s_axi_control_WDATA;

  assign s_axi_control_WSTRB_slr_0_inner = s_axi_control_WSTRB;
  assign s_axi_control_WSTRB_slr_1_inner = s_axi_control_WSTRB;
  assign s_axi_control_WSTRB_slr_2_inner = s_axi_control_WSTRB;

  assign s_axi_control_WVALID_slr_0_inner = s_axi_control_WVALID;
  assign s_axi_control_WVALID_slr_1_inner = s_axi_control_WVALID;
  assign s_axi_control_WVALID_slr_2_inner = s_axi_control_WVALID;

  assign s_axi_control_WREADY = s_axi_control_WREADY_slr_0_inner &
                                s_axi_control_WREADY_slr_1_inner &
                                s_axi_control_WREADY_slr_2_inner;

  relay_station
  #(
    .DATA_WIDTH(C_S_AXI_CONTROL_ADDR_WIDTH),
    .DEPTH(24),
    .ADDR_WIDTH(1),
    .LEVEL( 3 )
  )
  AW_pipeline_slr_0
  (
    .clk        (ap_clk),
    .reset      (1'b0),
    .if_read_ce (1'b1),
    .if_write_ce(1'b1),

    .if_din     (s_axi_control_AWADDR_slr_0_inner),
    .if_full_n  (s_axi_control_AWREADY_slr_0_inner),
    .if_write   (s_axi_control_AWVALID_slr_0_inner),

    .if_dout    (s_axi_control_AWADDR_slr_0),
    .if_empty_n (s_axi_control_AWVALID_slr_0),
    .if_read    (s_axi_control_AWREADY_slr_0)
  );

  relay_station
  #(
    .DATA_WIDTH(C_S_AXI_CONTROL_DATA_WIDTH + C_S_AXI_CONTROL_WSTRB_WIDTH),
    .DEPTH(24),
    .ADDR_WIDTH(1),
    .LEVEL( 3 )
  )
  W_pipeline_slr_0
  (
    .clk        (ap_clk),
    .reset      (1'b0),
    .if_read_ce (1'b1),
    .if_write_ce(1'b1),

    .if_din     ({s_axi_control_WDATA_slr_0_inner, s_axi_control_WSTRB_slr_0_inner}),
    .if_full_n  (s_axi_control_WREADY_slr_0_inner),
    .if_write   (s_axi_control_WVALID_slr_0_inner),

    .if_dout    ({s_axi_control_WDATA_slr_0, s_axi_control_WSTRB_slr_0}),
    .if_empty_n (s_axi_control_WVALID_slr_0),
    .if_read    (s_axi_control_WREADY_slr_0)
  );


  relay_station
  #(
    .DATA_WIDTH(C_S_AXI_CONTROL_ADDR_WIDTH),
    .DEPTH(24),
    .ADDR_WIDTH(1),
    .LEVEL( 3 )
  )
  AW_pipeline_slr_1
  (
    .clk        (ap_clk),
    .reset      (1'b0),
    .if_read_ce (1'b1),
    .if_write_ce(1'b1),

    .if_din     (s_axi_control_AWADDR_slr_1_inner),
    .if_full_n  (s_axi_control_AWREADY_slr_1_inner),
    .if_write   (s_axi_control_AWVALID_slr_1_inner),

    .if_dout    (s_axi_control_AWADDR_slr_1),
    .if_empty_n (s_axi_control_AWVALID_slr_1),
    .if_read    (s_axi_control_AWREADY_slr_1)
  );

  relay_station
  #(
    .DATA_WIDTH(C_S_AXI_CONTROL_DATA_WIDTH + C_S_AXI_CONTROL_WSTRB_WIDTH),
    .DEPTH(24),
    .ADDR_WIDTH(1),
    .LEVEL( 3 )
  )
  W_pipeline_slr_1
  (
    .clk        (ap_clk),
    .reset      (1'b0),
    .if_read_ce (1'b1),
    .if_write_ce(1'b1),

    .if_din     ({s_axi_control_WDATA_slr_1_inner, s_axi_control_WSTRB_slr_1_inner}),
    .if_full_n  (s_axi_control_WREADY_slr_1_inner),
    .if_write   (s_axi_control_WVALID_slr_1_inner),

    .if_dout    ({s_axi_control_WDATA_slr_1, s_axi_control_WSTRB_slr_1}),
    .if_empty_n (s_axi_control_WVALID_slr_1),
    .if_read    (s_axi_control_WREADY_slr_1)
  );


  relay_station
  #(
    .DATA_WIDTH(C_S_AXI_CONTROL_ADDR_WIDTH),
    .DEPTH(24),
    .ADDR_WIDTH(1),
    .LEVEL( 3 )
  )
  AW_pipeline_slr_2
  (
    .clk        (ap_clk),
    .reset      (1'b0),
    .if_read_ce (1'b1),
    .if_write_ce(1'b1),

    .if_din     (s_axi_control_AWADDR_slr_2_inner),
    .if_full_n  (s_axi_control_AWREADY_slr_2_inner),
    .if_write   (s_axi_control_AWVALID_slr_2_inner),

    .if_dout    (s_axi_control_AWADDR_slr_2),
    .if_empty_n (s_axi_control_AWVALID_slr_2),
    .if_read    (s_axi_control_AWREADY_slr_2)
  );

  relay_station
  #(
    .DATA_WIDTH(C_S_AXI_CONTROL_DATA_WIDTH + C_S_AXI_CONTROL_WSTRB_WIDTH),
    .DEPTH(24),
    .ADDR_WIDTH(1),
    .LEVEL( 3 )
  )
  W_pipeline_slr_2
  (
    .clk        (ap_clk),
    .reset      (1'b0),
    .if_read_ce (1'b1),
    .if_write_ce(1'b1),

    .if_din     ({s_axi_control_WDATA_slr_2_inner, s_axi_control_WSTRB_slr_2_inner}),
    .if_full_n  (s_axi_control_WREADY_slr_2_inner),
    .if_write   (s_axi_control_WVALID_slr_2_inner),

    .if_dout    ({s_axi_control_WDATA_slr_2, s_axi_control_WSTRB_slr_2}),
    .if_empty_n (s_axi_control_WVALID_slr_2),
    .if_read    (s_axi_control_WREADY_slr_2)
  );


endmodule
