// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

// Custom rtl file

module Add_Upper_fsm
(
  ap_clk,
  ap_rst_n,
  ap_start,
  ap_ready,
  ap_done,
  ap_idle,
  n,
  Add_0___n__q0,
  Add_0__ap_start,
  Add_0__ap_ready,
  Add_0__ap_done,
  Add_0__ap_idle
);

  // pragma RS clk port=ap_clk
  // pragma RS rst port=ap_rst_n active=low
  // pragma RS ap-ctrl ap_start=ap_start ap_done=ap_done ap_idle=ap_idle ap_ready=ap_ready scalar=(n)
  // pragma RS ap-ctrl ap_start=Add_0__ap_start ap_done=Add_0__ap_done ap_idle=Add_0__ap_idle ap_ready=Add_0__ap_ready scalar=Add_0___.*

  input ap_clk;
  input ap_rst_n;
  input ap_start;
  output ap_ready;
  output ap_done;
  output ap_idle;
  input [63:0] n;
  output [63:0] Add_0___n__q0;
  output Add_0__ap_start;
  input Add_0__ap_ready;
  input Add_0__ap_done;
  input Add_0__ap_idle;
  wire [63:0] Add_0___n__q0;
  wire Add_0__ap_start_global__q0;
  wire Add_0__is_done__q0;
  wire Add_0__ap_done_global__q0;
  wire Add_0__ap_start;
  wire Add_0__ap_ready;
  wire Add_0__ap_done;
  wire Add_0__ap_idle;
  reg [1:0] Add_0__state;
  reg [1:0] tapa_state;
  reg [0:0] countdown;
  wire ap_start__q0;
  wire ap_done__q0;
  assign Add_0___n__q0 = n;
  assign Add_0__ap_start_global__q0 = ap_start__q0;
  assign Add_0__is_done__q0 = (Add_0__state == 2'b10);
  assign Add_0__ap_done_global__q0 = ap_done__q0;

  always @(posedge ap_clk) begin
    if(~ap_rst_n) begin
      Add_0__state <= 2'b00;
    end else begin
      if(Add_0__state == 2'b00) begin
        if(Add_0__ap_start_global__q0) begin
          Add_0__state <= 2'b01;
        end
      end
      if(Add_0__state == 2'b01) begin
        if(Add_0__ap_ready) begin
          if(Add_0__ap_done) begin
            Add_0__state <= 2'b10;
          end else begin
            Add_0__state <= 2'b11;
          end
        end
      end
      if(Add_0__state == 2'b11) begin
        if(Add_0__ap_done) begin
          Add_0__state <= 2'b10;
        end
      end
      if(Add_0__state == 2'b10) begin
        if(Add_0__ap_done_global__q0) begin
          Add_0__state <= 2'b00;
        end
      end
    end
  end

  assign Add_0__ap_start = (Add_0__state == 2'b01);

  always @(posedge ap_clk) begin
    if(~ap_rst_n) begin
      tapa_state <= 2'b00;
    end else begin
      case(tapa_state)
        2'b00: begin
          if(ap_start__q0) begin
            tapa_state <= 2'b01;
          end
        end
        2'b01: begin
          if(Add_0__is_done__q0) begin
            tapa_state <= 2'b10;
          end
        end
        2'b10: begin
          tapa_state <= 2'b00;
          countdown <= 1'd0;
        end
        2'b11: begin
          if(countdown == 1'd0) begin
            tapa_state <= 2'b00;
          end else begin
            countdown <= (countdown - 1'd1);
          end
        end
      endcase
    end
  end

  assign ap_idle = (tapa_state == 2'b00);
  assign ap_done = ap_done__q0;
  assign ap_ready = ap_done__q0;
  assign ap_start__q0 = ap_start;
  assign ap_done__q0 = (tapa_state == 2'b10);

endmodule
