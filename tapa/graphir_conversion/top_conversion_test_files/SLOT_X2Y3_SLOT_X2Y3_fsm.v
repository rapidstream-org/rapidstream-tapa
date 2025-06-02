module SLOT_X2Y3_SLOT_X2Y3_fsm(  ap_clk,
  ap_rst_n,
  ap_start,
  ap_ready,
  ap_done,
  ap_idle,
  mmap_Stream2Mmap_0,
  n,
  Stream2Mmap_0___mmap_Stream2Mmap_0__q0,
  Stream2Mmap_0___n__q0,
  Stream2Mmap_0__ap_start,
  Stream2Mmap_0__ap_ready,
  Stream2Mmap_0__ap_done,
  Stream2Mmap_0__ap_idle);
  input ap_clk;
  input ap_rst_n;
  input ap_start;
  output ap_ready;
  output ap_done;
  output ap_idle;
  wire [63:0] Stream2Mmap_0___mmap_Stream2Mmap_0__q0;
  assign Stream2Mmap_0___mmap_Stream2Mmap_0__q0 = mmap_Stream2Mmap_0;
  wire [63:0] Stream2Mmap_0___n__q0;
  assign Stream2Mmap_0___n__q0 = n;
  wire Stream2Mmap_0__ap_start_global__q0;
  assign Stream2Mmap_0__ap_start_global__q0 = ap_start__q0;
  wire Stream2Mmap_0__is_done__q0;
  assign Stream2Mmap_0__is_done__q0 = (Stream2Mmap_0__state == 2'b10);
  wire Stream2Mmap_0__ap_done_global__q0;
  assign Stream2Mmap_0__ap_done_global__q0 = ap_done__q0;

always @(posedge ap_clk) begin
  if(~ap_rst_n) begin
    Stream2Mmap_0__state <= 2'b00;
  end else begin
    if(Stream2Mmap_0__state == 2'b00) begin
      if(Stream2Mmap_0__ap_start_global__q0) begin
        Stream2Mmap_0__state <= 2'b01;
      end
    end
    if(Stream2Mmap_0__state == 2'b01) begin
      if(Stream2Mmap_0__ap_ready) begin
        if(Stream2Mmap_0__ap_done) begin
          Stream2Mmap_0__state <= 2'b10;
        end else begin
          Stream2Mmap_0__state <= 2'b11;
        end
      end
    end
    if(Stream2Mmap_0__state == 2'b11) begin
      if(Stream2Mmap_0__ap_done) begin
        Stream2Mmap_0__state <= 2'b10;
      end
    end
    if(Stream2Mmap_0__state == 2'b10) begin
      if(Stream2Mmap_0__ap_done_global__q0) begin
        Stream2Mmap_0__state <= 2'b00;
      end
    end
  end
end
  assign Stream2Mmap_0__ap_start = (Stream2Mmap_0__state == 2'b01);
  wire Stream2Mmap_0__ap_start;
  wire Stream2Mmap_0__ap_ready;
  wire Stream2Mmap_0__ap_done;
  wire Stream2Mmap_0__ap_idle;
  reg [1:0] Stream2Mmap_0__state;
  input [63:0] mmap_Stream2Mmap_0;
  input [63:0] n;
  output [63:0] Stream2Mmap_0___mmap_Stream2Mmap_0__q0;
  output [63:0] Stream2Mmap_0___n__q0;
  output Stream2Mmap_0__ap_start;
  input Stream2Mmap_0__ap_ready;
  input Stream2Mmap_0__ap_done;
  input Stream2Mmap_0__ap_idle;
// pragma RS clk port=ap_clk
// pragma RS rst port=ap_rst_n active=low
// pragma RS ap-ctrl ap_start=ap_start ap_done=ap_done ap_idle=ap_idle ap_ready=ap_ready scalar=(n|mmap_Stream2Mmap_0)
// pragma RS ap-ctrl ap_start=Stream2Mmap_0__ap_start ap_done=Stream2Mmap_0__ap_done ap_idle=Stream2Mmap_0__ap_idle ap_ready=Stream2Mmap_0__ap_ready scalar=Stream2Mmap_0___.*
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
        if(Stream2Mmap_0__is_done__q0) begin
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
