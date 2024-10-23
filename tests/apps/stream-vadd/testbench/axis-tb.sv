`timescale 1ns / 1ps

// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

module VecAdd_tb();
  parameter CLK_PERIOD = 10;

  reg ap_clk = 0, ap_rst_n = 1;
  wire interrupt;

  reg s_axi_control_AWVALID = 0, s_axi_control_WVALID = 0, s_axi_control_ARVALID = 0, s_axi_control_RREADY = 0, s_axi_control_BREADY = 0;
  wire s_axi_control_AWREADY, s_axi_control_WREADY, s_axi_control_ARREADY, s_axi_control_RVALID, s_axi_control_BVALID;
  reg [4:0] s_axi_control_AWADDR, s_axi_control_ARADDR;
  wire [1:0] s_axi_control_RRESP, s_axi_control_BRESP;
  wire [31:0] s_axi_control_RDATA;
  reg [31:0] s_axi_control_WDATA;
  reg [3:0] s_axi_control_WSTRB;

  reg a_TVALID = 0, b_TVALID = 0, c_TREADY = 0, a_TLAST, b_TLAST;
  wire a_TREADY, b_TREADY, c_TVALID, c_TLAST;
  reg [31:0] a_TDATA, b_TDATA;
  reg [3:0] a_TKEEP, b_TKEEP;
  wire [3:0] c_TKEEP;
  wire [31:0] c_TDATA;

  VecAdd uut (.*);

  always #(CLK_PERIOD/2) ap_clk = ~ap_clk;

  initial begin
    // Wait for Vivado to finish the simulation setup
    #1000;

    // Reset UUT
    #(CLK_PERIOD*10) ap_rst_n = 0; #(CLK_PERIOD*10) ap_rst_n = 1; #(CLK_PERIOD*10);

    // Write control registers, n = 5 and set ap_start
    write_reg(5'h10, 32'h00000005);
    write_reg(5'h14, 32'h00000000);
    write_reg(5'h00, 32'h00000001);

    fork
      stream_a_data();
      stream_b_data();
      read_c_data();
    join

    wait_for_done();

    $display("PASSED");
    $finish;
  end

  task write_reg(input [4:0] addr, input [31:0] data);
    begin
      s_axi_control_AWVALID = 1; s_axi_control_AWADDR = addr;
      s_axi_control_WVALID = 1; s_axi_control_WDATA = data; s_axi_control_WSTRB = 4'hF;
      s_axi_control_BREADY = 1;
      @(posedge ap_clk);
      while (!s_axi_control_AWREADY) @(posedge ap_clk);
      while (!s_axi_control_WREADY) @(posedge ap_clk);
      s_axi_control_AWVALID = 0; s_axi_control_WVALID = 0;
      while (!s_axi_control_BVALID) @(posedge ap_clk);
      s_axi_control_BREADY = 0;
    end
  endtask

  task read_reg(input [4:0] addr, output [31:0] data);
    begin
      s_axi_control_ARVALID = 1; s_axi_control_ARADDR = addr; s_axi_control_RREADY = 1;
      @(posedge ap_clk);
      while (!s_axi_control_ARREADY) @(posedge ap_clk);
      s_axi_control_ARVALID = 0;
      while (!s_axi_control_RVALID) @(posedge ap_clk);
      data = s_axi_control_RDATA; s_axi_control_RREADY = 0;
    end
  endtask

  task stream_a_data;
    integer i;
    begin
      for (i = 0; i < 5; i = i + 1) begin
        a_TVALID = 1;
        a_TDATA = $shortrealtobits(0.0 + i);
        a_TKEEP = 4'hF;
        a_TLAST = 0;
        @(posedge ap_clk);
        while (!a_TREADY) @(posedge ap_clk);
      end

      a_TVALID = 1;
      a_TDATA = 0;
      a_TKEEP = 4'hF;
      a_TLAST = 1;
      @(posedge ap_clk);
      while (!a_TREADY) @(posedge ap_clk);

      a_TVALID = 0;
    end
  endtask

  task stream_b_data;
    integer i;
    begin
      for (i = 0; i < 5; i = i + 1) begin
        b_TVALID = 1;
        b_TDATA = $shortrealtobits(1.0 + i);
        b_TKEEP = 4'hF;
        b_TLAST = 0;
        @(posedge ap_clk);
        while (!b_TREADY) @(posedge ap_clk);
      end

      b_TVALID = 1;
      b_TDATA = 0;
      b_TKEEP = 4'hF;
      b_TLAST = 1;
      @(posedge ap_clk);
      while (!b_TREADY) @(posedge ap_clk);

      b_TVALID = 0;
    end
  endtask

  task read_c_data;
    shortreal real_data;
    begin
      c_TREADY = 1;
      for (int i = 0; i < 5; i++) begin
        while (!c_TVALID) @(posedge ap_clk);
        real_data = $bitstoshortreal(c_TDATA);
        $display("c_TDATA[%0d] = %f", i, real_data);
        if (real_data != 1.0 + 2 * i)
          $fatal(1, "Error: c_TDATA[%0d] = %f, expected %f", i, real_data, 1.0 + 2 * i);
        @(posedge ap_clk);
      end

      while (!c_TVALID) @(posedge ap_clk);
      if (c_TLAST !== 1'b1)
        $fatal(1, "Error: c_TLAST not set on last data");
      @(posedge ap_clk);

      c_TREADY = 0;
    end
  endtask

  task wait_for_done;
    reg [31:0] status;
    do begin
      read_reg(5'h00, status);
      @(posedge ap_clk);
    end while (status[1] == 1'b0);
  endtask

endmodule
