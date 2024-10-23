`timescale 1ns / 1ps

// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

module VecAdd_tb();
  parameter CLK_PERIOD = 10;

  reg ap_clk = 0, ap_rst_n = 1, ap_start = 0;
  wire ap_done, ap_idle, ap_ready;

  reg [32:0] a_s_dout = 0, b_s_dout = 0;
  reg a_s_empty_n = 0, b_s_empty_n = 0;
  wire a_s_read, b_s_read;

  wire [32:0] c_din;
  reg c_full_n = 0;
  wire c_write;

  reg [63:0] n = 5;

  VecAdd uut (
    .ap_clk(ap_clk),
    .ap_rst_n(ap_rst_n),
    .ap_start(ap_start),
    .ap_done(ap_done),
    .ap_idle(ap_idle),
    .ap_ready(ap_ready),
    .a_s_dout(a_s_dout),
    .a_s_empty_n(a_s_empty_n),
    .a_s_read(a_s_read),
    .b_s_dout(b_s_dout),
    .b_s_empty_n(b_s_empty_n),
    .b_s_read(b_s_read),
    .c_din(c_din),
    .c_full_n(c_full_n),
    .c_write(c_write),
    // TODO: Hide peek ports from upper level tasks
    .a_peek_dout(a_s_dout),
    .a_peek_empty_n(),
    .a_peek_read(),
    .b_peek_dout(b_s_dout),
    .b_peek_empty_n(),
    .b_peek_read(),
    .n(n)
  );

  always #(CLK_PERIOD/2) ap_clk = ~ap_clk;

  initial begin
    // Wait for Vivado to finish the simulation setup
    #1000;

    // Reset and start UUT
    #(CLK_PERIOD*10) ap_rst_n = 0; #(CLK_PERIOD*10) ap_rst_n = 1; #(CLK_PERIOD*10);
    @(posedge ap_clk);

    ap_start = 1;
    @(posedge ap_clk);

    fork
      stream_a_data();
      stream_b_data();
      read_c_data();
      begin
        wait(ap_ready);
        ap_start = 0;
      end
    join

    $display("PASSED");
    $finish;
  end

  task stream_a_data;
    integer i;
    begin
      // Write numbers to stream A
      for (i = 0; i < 5; i = i + 1) begin
        a_s_empty_n = 1;
        a_s_dout = {1'b0, $shortrealtobits(0.0 + i)};
        @(posedge ap_clk);
        while (!a_s_read) @(posedge ap_clk);
      end

      // Write a close token
      a_s_empty_n = 1;
      a_s_dout = {1'b1, 0};
      @(posedge ap_clk);
      while (!a_s_read) @(posedge ap_clk);

      a_s_empty_n = 0;
    end
  endtask

  task stream_b_data;
    integer i;
    begin
      // Write numbers to stream B
      for (i = 0; i < 5; i = i + 1) begin
        b_s_empty_n = 1;
        b_s_dout = {1'b0, $shortrealtobits(1.0 + i)};
        @(posedge ap_clk);
        while (!b_s_read) @(posedge ap_clk);
      end

      // Write a close token
      b_s_empty_n = 1;
      b_s_dout = {1'b1, 0};
      @(posedge ap_clk);
      while (!b_s_read) @(posedge ap_clk);

      b_s_empty_n = 0;
    end
  endtask

  task read_c_data;
    shortreal real_data;
    begin
      c_full_n = 1;

      for (int i = 0; i < 5; i++) begin
        while (!c_write) @(posedge ap_clk);
        real_data = $bitstoshortreal(c_din[31:0]);
        $display("c_din[%0d] = %f", i, real_data);
        if (real_data != 1.0 + 2 * i)
          $fatal(1, "Error: c_din[%0d] = %f, expected %f", i, real_data, 1.0 + 2 * i);
        @(posedge ap_clk);
      end

      while (!c_write) @(posedge ap_clk);
      if (c_din[32] !== 1'b1)
        $fatal(1, "Error: close token not set on last data");
      @(posedge ap_clk);

      c_full_n = 0;
    end
  endtask

endmodule
