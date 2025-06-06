module VecAdd_fsm(  ap_clk,
  ap_rst_n,
  ap_start,
  ap_ready,
  ap_done,
  ap_idle,
  a,
  n,
  c,
  b,
  SLOT_X0Y2_SLOT_X0Y2_0___a__q0,
  SLOT_X0Y2_SLOT_X0Y2_0___n__q0,
  SLOT_X0Y2_SLOT_X0Y2_0__ap_start,
  SLOT_X0Y2_SLOT_X0Y2_0__ap_ready,
  SLOT_X0Y2_SLOT_X0Y2_0__ap_done,
  SLOT_X0Y2_SLOT_X0Y2_0__ap_idle,
  SLOT_X2Y3_SLOT_X2Y3_0___c__q0,
  SLOT_X2Y3_SLOT_X2Y3_0___n__q0,
  SLOT_X2Y3_SLOT_X2Y3_0__ap_start,
  SLOT_X2Y3_SLOT_X2Y3_0__ap_ready,
  SLOT_X2Y3_SLOT_X2Y3_0__ap_done,
  SLOT_X2Y3_SLOT_X2Y3_0__ap_idle,
  SLOT_X3Y3_SLOT_X3Y3_0___b__q0,
  SLOT_X3Y3_SLOT_X3Y3_0___n__q0,
  SLOT_X3Y3_SLOT_X3Y3_0__ap_start,
  SLOT_X3Y3_SLOT_X3Y3_0__ap_ready,
  SLOT_X3Y3_SLOT_X3Y3_0__ap_done,
  SLOT_X3Y3_SLOT_X3Y3_0__ap_idle);
  input ap_clk;
  input ap_rst_n;
  input ap_start;
  output ap_ready;
  output ap_done;
  output ap_idle;
  wire [63:0] SLOT_X0Y2_SLOT_X0Y2_0___a__q0;
  assign SLOT_X0Y2_SLOT_X0Y2_0___a__q0 = a;
  wire [63:0] SLOT_X0Y2_SLOT_X0Y2_0___n__q0;
  assign SLOT_X0Y2_SLOT_X0Y2_0___n__q0 = n;
  wire SLOT_X0Y2_SLOT_X0Y2_0__ap_start_global__q0;
  assign SLOT_X0Y2_SLOT_X0Y2_0__ap_start_global__q0 = ap_start__q0;
  wire SLOT_X0Y2_SLOT_X0Y2_0__is_done__q0;
  assign SLOT_X0Y2_SLOT_X0Y2_0__is_done__q0 = (SLOT_X0Y2_SLOT_X0Y2_0__state == 2'b10);
  wire SLOT_X0Y2_SLOT_X0Y2_0__ap_done_global__q0;
  assign SLOT_X0Y2_SLOT_X0Y2_0__ap_done_global__q0 = ap_done__q0;

always @(posedge ap_clk) begin
  if(~ap_rst_n) begin
    SLOT_X0Y2_SLOT_X0Y2_0__state <= 2'b00;
  end else begin
    if(SLOT_X0Y2_SLOT_X0Y2_0__state == 2'b00) begin
      if(SLOT_X0Y2_SLOT_X0Y2_0__ap_start_global__q0) begin
        SLOT_X0Y2_SLOT_X0Y2_0__state <= 2'b01;
      end
    end
    if(SLOT_X0Y2_SLOT_X0Y2_0__state == 2'b01) begin
      if(SLOT_X0Y2_SLOT_X0Y2_0__ap_ready) begin
        if(SLOT_X0Y2_SLOT_X0Y2_0__ap_done) begin
          SLOT_X0Y2_SLOT_X0Y2_0__state <= 2'b10;
        end else begin
          SLOT_X0Y2_SLOT_X0Y2_0__state <= 2'b11;
        end
      end
    end
    if(SLOT_X0Y2_SLOT_X0Y2_0__state == 2'b11) begin
      if(SLOT_X0Y2_SLOT_X0Y2_0__ap_done) begin
        SLOT_X0Y2_SLOT_X0Y2_0__state <= 2'b10;
      end
    end
    if(SLOT_X0Y2_SLOT_X0Y2_0__state == 2'b10) begin
      if(SLOT_X0Y2_SLOT_X0Y2_0__ap_done_global__q0) begin
        SLOT_X0Y2_SLOT_X0Y2_0__state <= 2'b00;
      end
    end
  end
end
  assign SLOT_X0Y2_SLOT_X0Y2_0__ap_start = (SLOT_X0Y2_SLOT_X0Y2_0__state == 2'b01);
  wire SLOT_X0Y2_SLOT_X0Y2_0__ap_start;
  wire SLOT_X0Y2_SLOT_X0Y2_0__ap_ready;
  wire SLOT_X0Y2_SLOT_X0Y2_0__ap_done;
  wire SLOT_X0Y2_SLOT_X0Y2_0__ap_idle;
  reg [1:0] SLOT_X0Y2_SLOT_X0Y2_0__state;
  wire [63:0] SLOT_X2Y3_SLOT_X2Y3_0___c__q0;
  assign SLOT_X2Y3_SLOT_X2Y3_0___c__q0 = c;
  wire [63:0] SLOT_X2Y3_SLOT_X2Y3_0___n__q0;
  assign SLOT_X2Y3_SLOT_X2Y3_0___n__q0 = n;
  wire SLOT_X2Y3_SLOT_X2Y3_0__ap_start_global__q0;
  assign SLOT_X2Y3_SLOT_X2Y3_0__ap_start_global__q0 = ap_start__q0;
  wire SLOT_X2Y3_SLOT_X2Y3_0__is_done__q0;
  assign SLOT_X2Y3_SLOT_X2Y3_0__is_done__q0 = (SLOT_X2Y3_SLOT_X2Y3_0__state == 2'b10);
  wire SLOT_X2Y3_SLOT_X2Y3_0__ap_done_global__q0;
  assign SLOT_X2Y3_SLOT_X2Y3_0__ap_done_global__q0 = ap_done__q0;

always @(posedge ap_clk) begin
  if(~ap_rst_n) begin
    SLOT_X2Y3_SLOT_X2Y3_0__state <= 2'b00;
  end else begin
    if(SLOT_X2Y3_SLOT_X2Y3_0__state == 2'b00) begin
      if(SLOT_X2Y3_SLOT_X2Y3_0__ap_start_global__q0) begin
        SLOT_X2Y3_SLOT_X2Y3_0__state <= 2'b01;
      end
    end
    if(SLOT_X2Y3_SLOT_X2Y3_0__state == 2'b01) begin
      if(SLOT_X2Y3_SLOT_X2Y3_0__ap_ready) begin
        if(SLOT_X2Y3_SLOT_X2Y3_0__ap_done) begin
          SLOT_X2Y3_SLOT_X2Y3_0__state <= 2'b10;
        end else begin
          SLOT_X2Y3_SLOT_X2Y3_0__state <= 2'b11;
        end
      end
    end
    if(SLOT_X2Y3_SLOT_X2Y3_0__state == 2'b11) begin
      if(SLOT_X2Y3_SLOT_X2Y3_0__ap_done) begin
        SLOT_X2Y3_SLOT_X2Y3_0__state <= 2'b10;
      end
    end
    if(SLOT_X2Y3_SLOT_X2Y3_0__state == 2'b10) begin
      if(SLOT_X2Y3_SLOT_X2Y3_0__ap_done_global__q0) begin
        SLOT_X2Y3_SLOT_X2Y3_0__state <= 2'b00;
      end
    end
  end
end
  assign SLOT_X2Y3_SLOT_X2Y3_0__ap_start = (SLOT_X2Y3_SLOT_X2Y3_0__state == 2'b01);
  wire SLOT_X2Y3_SLOT_X2Y3_0__ap_start;
  wire SLOT_X2Y3_SLOT_X2Y3_0__ap_ready;
  wire SLOT_X2Y3_SLOT_X2Y3_0__ap_done;
  wire SLOT_X2Y3_SLOT_X2Y3_0__ap_idle;
  reg [1:0] SLOT_X2Y3_SLOT_X2Y3_0__state;
  wire [63:0] SLOT_X3Y3_SLOT_X3Y3_0___b__q0;
  assign SLOT_X3Y3_SLOT_X3Y3_0___b__q0 = b;
  wire [63:0] SLOT_X3Y3_SLOT_X3Y3_0___n__q0;
  assign SLOT_X3Y3_SLOT_X3Y3_0___n__q0 = n;
  wire SLOT_X3Y3_SLOT_X3Y3_0__ap_start_global__q0;
  assign SLOT_X3Y3_SLOT_X3Y3_0__ap_start_global__q0 = ap_start__q0;
  wire SLOT_X3Y3_SLOT_X3Y3_0__is_done__q0;
  assign SLOT_X3Y3_SLOT_X3Y3_0__is_done__q0 = (SLOT_X3Y3_SLOT_X3Y3_0__state == 2'b10);
  wire SLOT_X3Y3_SLOT_X3Y3_0__ap_done_global__q0;
  assign SLOT_X3Y3_SLOT_X3Y3_0__ap_done_global__q0 = ap_done__q0;

always @(posedge ap_clk) begin
  if(~ap_rst_n) begin
    SLOT_X3Y3_SLOT_X3Y3_0__state <= 2'b00;
  end else begin
    if(SLOT_X3Y3_SLOT_X3Y3_0__state == 2'b00) begin
      if(SLOT_X3Y3_SLOT_X3Y3_0__ap_start_global__q0) begin
        SLOT_X3Y3_SLOT_X3Y3_0__state <= 2'b01;
      end
    end
    if(SLOT_X3Y3_SLOT_X3Y3_0__state == 2'b01) begin
      if(SLOT_X3Y3_SLOT_X3Y3_0__ap_ready) begin
        if(SLOT_X3Y3_SLOT_X3Y3_0__ap_done) begin
          SLOT_X3Y3_SLOT_X3Y3_0__state <= 2'b10;
        end else begin
          SLOT_X3Y3_SLOT_X3Y3_0__state <= 2'b11;
        end
      end
    end
    if(SLOT_X3Y3_SLOT_X3Y3_0__state == 2'b11) begin
      if(SLOT_X3Y3_SLOT_X3Y3_0__ap_done) begin
        SLOT_X3Y3_SLOT_X3Y3_0__state <= 2'b10;
      end
    end
    if(SLOT_X3Y3_SLOT_X3Y3_0__state == 2'b10) begin
      if(SLOT_X3Y3_SLOT_X3Y3_0__ap_done_global__q0) begin
        SLOT_X3Y3_SLOT_X3Y3_0__state <= 2'b00;
      end
    end
  end
end
  assign SLOT_X3Y3_SLOT_X3Y3_0__ap_start = (SLOT_X3Y3_SLOT_X3Y3_0__state == 2'b01);
  wire SLOT_X3Y3_SLOT_X3Y3_0__ap_start;
  wire SLOT_X3Y3_SLOT_X3Y3_0__ap_ready;
  wire SLOT_X3Y3_SLOT_X3Y3_0__ap_done;
  wire SLOT_X3Y3_SLOT_X3Y3_0__ap_idle;
  reg [1:0] SLOT_X3Y3_SLOT_X3Y3_0__state;
  input [63:0] a;
  input [63:0] n;
  input [63:0] c;
  input [63:0] b;
  output [63:0] SLOT_X0Y2_SLOT_X0Y2_0___a__q0;
  output [63:0] SLOT_X0Y2_SLOT_X0Y2_0___n__q0;
  output SLOT_X0Y2_SLOT_X0Y2_0__ap_start;
  input SLOT_X0Y2_SLOT_X0Y2_0__ap_ready;
  input SLOT_X0Y2_SLOT_X0Y2_0__ap_done;
  input SLOT_X0Y2_SLOT_X0Y2_0__ap_idle;
  output [63:0] SLOT_X2Y3_SLOT_X2Y3_0___c__q0;
  output [63:0] SLOT_X2Y3_SLOT_X2Y3_0___n__q0;
  output SLOT_X2Y3_SLOT_X2Y3_0__ap_start;
  input SLOT_X2Y3_SLOT_X2Y3_0__ap_ready;
  input SLOT_X2Y3_SLOT_X2Y3_0__ap_done;
  input SLOT_X2Y3_SLOT_X2Y3_0__ap_idle;
  output [63:0] SLOT_X3Y3_SLOT_X3Y3_0___b__q0;
  output [63:0] SLOT_X3Y3_SLOT_X3Y3_0___n__q0;
  output SLOT_X3Y3_SLOT_X3Y3_0__ap_start;
  input SLOT_X3Y3_SLOT_X3Y3_0__ap_ready;
  input SLOT_X3Y3_SLOT_X3Y3_0__ap_done;
  input SLOT_X3Y3_SLOT_X3Y3_0__ap_idle;
// pragma RS clk port=ap_clk
// pragma RS rst port=ap_rst_n active=low
// pragma RS ap-ctrl ap_start=ap_start ap_done=ap_done ap_idle=ap_idle ap_ready=ap_ready scalar=(a|b|c|n)
// pragma RS ap-ctrl ap_start=SLOT_X0Y2_SLOT_X0Y2_0__ap_start ap_done=SLOT_X0Y2_SLOT_X0Y2_0__ap_done ap_idle=SLOT_X0Y2_SLOT_X0Y2_0__ap_idle ap_ready=SLOT_X0Y2_SLOT_X0Y2_0__ap_ready scalar=SLOT_X0Y2_SLOT_X0Y2_0___.*
// pragma RS ap-ctrl ap_start=SLOT_X2Y3_SLOT_X2Y3_0__ap_start ap_done=SLOT_X2Y3_SLOT_X2Y3_0__ap_done ap_idle=SLOT_X2Y3_SLOT_X2Y3_0__ap_idle ap_ready=SLOT_X2Y3_SLOT_X2Y3_0__ap_ready scalar=SLOT_X2Y3_SLOT_X2Y3_0___.*
// pragma RS ap-ctrl ap_start=SLOT_X3Y3_SLOT_X3Y3_0__ap_start ap_done=SLOT_X3Y3_SLOT_X3Y3_0__ap_done ap_idle=SLOT_X3Y3_SLOT_X3Y3_0__ap_idle ap_ready=SLOT_X3Y3_SLOT_X3Y3_0__ap_ready scalar=SLOT_X3Y3_SLOT_X3Y3_0___.*
  reg [1:0] tapa_state;

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
        if(SLOT_X0Y2_SLOT_X0Y2_0__is_done__q0 && SLOT_X2Y3_SLOT_X2Y3_0__is_done__q0 && SLOT_X3Y3_SLOT_X3Y3_0__is_done__q0) begin
          tapa_state <= 2'b10;
        end
      end
      2'b10: begin
        tapa_state <= 2'b00;
      end
    endcase
  end
end
  assign ap_idle = (tapa_state == 2'b00);
  assign ap_done = ap_done__q0;
  assign ap_ready = ap_done__q0;
  wire ap_start__q0;
  assign ap_start__q0 = ap_start;
  wire ap_done__q0;
  assign ap_done__q0 = (tapa_state == 2'b10); endmodule
