`default_nettype none

module generate_last #(
  parameter BurstLenWidth = 8
) (
  input wire clk,
  input wire rst,

  input  wire [BurstLenWidth-1:0] burst_len_dout,
  input  wire                     burst_len_empty_n,
  output reg                      burst_len_read,

  output reg  last_din,
  input  wire last_full_n,
  output reg  last_write
);

  // state
  reg                     busy;
  reg [BurstLenWidth-1:0] count;

  // logic
  reg                     busy_next;
  reg [BurstLenWidth-1:0] count_next;

  always @* begin
    busy_next = busy;
    count_next = count;
    burst_len_read = 1'b0;
    last_write = 1'b0;
    last_din = 1'bx;

    if (last_full_n) begin
      if (busy == 1'b0) begin
        if (burst_len_empty_n) begin
          // read from burst_len
          burst_len_read = 1'b1;
          count_next = burst_len_dout;

          // write to last
          last_write = 1'b1;
          last_din = ~|count_next;

          // change busy
          if (|count_next) busy_next = 1'b1;
        end
      end else begin
        count_next = count - 1'b1;

        // write to last
        last_write = 1'b1;
        last_din = ~|count_next;

        // change busy
        if (~|count_next) busy_next = 1'b0;
      end
    end
  end

  always @(posedge clk) begin
    if (rst) begin
      busy <= 1'b0;
      count <= {BurstLenWidth{1'b0}};
    end else begin
      busy <= busy_next;
      count <= count_next;
    end
  end

endmodule  // generate_last

`default_nettype wire
