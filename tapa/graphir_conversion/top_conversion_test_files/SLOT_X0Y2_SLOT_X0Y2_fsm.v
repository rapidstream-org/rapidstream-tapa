module SLOT_X0Y2_SLOT_X0Y2_fsm(  ap_clk,
  ap_rst_n,
  ap_start,
  ap_ready,
  ap_done,
  ap_idle,
  mmap_Mmap2Stream_0,
  n,
  Mmap2Stream_0___mmap_Mmap2Stream_0__q0,
  Mmap2Stream_0___n__q0,
  Mmap2Stream_0__ap_start,
  Mmap2Stream_0__ap_ready,
  Mmap2Stream_0__ap_done,
  Mmap2Stream_0__ap_idle);
  input ap_clk;
  input ap_rst_n;
  input ap_start;
  output ap_ready;
  output ap_done;
  output ap_idle;
  wire [63:0] Mmap2Stream_0___mmap_Mmap2Stream_0__q0;
  assign Mmap2Stream_0___mmap_Mmap2Stream_0__q0 = mmap_Mmap2Stream_0;
  wire [63:0] Mmap2Stream_0___n__q0;
  assign Mmap2Stream_0___n__q0 = n;
  wire Mmap2Stream_0__ap_start_global__q0;
  assign Mmap2Stream_0__ap_start_global__q0 = ap_start__q0;
  wire Mmap2Stream_0__is_done__q0;
  assign Mmap2Stream_0__is_done__q0 = (Mmap2Stream_0__state == 2'b10);
  wire Mmap2Stream_0__ap_done_global__q0;
  assign Mmap2Stream_0__ap_done_global__q0 = ap_done__q0;

always @(posedge ap_clk) begin
  if(~ap_rst_n) begin
    Mmap2Stream_0__state <= 2'b00;
  end else begin
    if(Mmap2Stream_0__state == 2'b00) begin
      if(Mmap2Stream_0__ap_start_global__q0) begin
        Mmap2Stream_0__state <= 2'b01;
      end
    end
    if(Mmap2Stream_0__state == 2'b01) begin
      if(Mmap2Stream_0__ap_ready) begin
        if(Mmap2Stream_0__ap_done) begin
          Mmap2Stream_0__state <= 2'b10;
        end else begin
          Mmap2Stream_0__state <= 2'b11;
        end
      end
    end
    if(Mmap2Stream_0__state == 2'b11) begin
      if(Mmap2Stream_0__ap_done) begin
        Mmap2Stream_0__state <= 2'b10;
      end
    end
    if(Mmap2Stream_0__state == 2'b10) begin
      if(Mmap2Stream_0__ap_done_global__q0) begin
        Mmap2Stream_0__state <= 2'b00;
      end
    end
  end
end
  assign Mmap2Stream_0__ap_start = (Mmap2Stream_0__state == 2'b01);
  wire Mmap2Stream_0__ap_start;
  wire Mmap2Stream_0__ap_ready;
  wire Mmap2Stream_0__ap_done;
  wire Mmap2Stream_0__ap_idle;
  reg [1:0] Mmap2Stream_0__state;
  input [63:0] mmap_Mmap2Stream_0;
  input [63:0] n;
  output [63:0] Mmap2Stream_0___mmap_Mmap2Stream_0__q0;
  output [63:0] Mmap2Stream_0___n__q0;
  output Mmap2Stream_0__ap_start;
  input Mmap2Stream_0__ap_ready;
  input Mmap2Stream_0__ap_done;
  input Mmap2Stream_0__ap_idle;
// pragma RS clk port=ap_clk
// pragma RS rst port=ap_rst_n active=low
// pragma RS ap-ctrl ap_start=ap_start ap_done=ap_done ap_idle=ap_idle ap_ready=ap_ready scalar=(n|mmap_Mmap2Stream_0)
// pragma RS ap-ctrl ap_start=Mmap2Stream_0__ap_start ap_done=Mmap2Stream_0__ap_done ap_idle=Mmap2Stream_0__ap_idle ap_ready=Mmap2Stream_0__ap_ready scalar=Mmap2Stream_0___.*
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
        if(Mmap2Stream_0__is_done__q0) begin
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
