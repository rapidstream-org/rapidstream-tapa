`default_nettype none

// Detect burst from address stream.
module detect_burst #(
  parameter AddrWidth         = 64,
  parameter DataWidthBytesLog = 6,
  parameter WaitTimeWidth     = 4,
  parameter BurstLenWidth     = 8
) (
  input wire clk,
  input wire rst,

  input wire [WaitTimeWidth-1:0] max_wait_time,
  input wire [BurstLenWidth-1:0] max_burst_len,  // 0 disables detection

  input  wire [AddrWidth-1:0] addr_dout,
  input  wire                 addr_empty_n,
  output reg                  addr_read,

  output reg  [BurstLenWidth+AddrWidth-1:0] addr_din,
  input  wire                               addr_full_n,
  output wire                               addr_write,

  output reg  [BurstLenWidth-1:0] burst_len_0_din,
  input  wire                     burst_len_0_full_n,
  output wire                     burst_len_0_write,

  output reg  [BurstLenWidth-1:0] burst_len_1_din,
  input  wire                     burst_len_1_full_n,
  output wire                     burst_len_1_write
);

  // state
  reg [AddrWidth-1:0]     base_addr;
  reg                     base_valid;
  reg [BurstLenWidth-1:0] burst_len;
  reg [WaitTimeWidth-1:0] wait_time;

  // logic
  reg                     write_enable;
  reg [AddrWidth-1:0]     base_addr_next;
  reg                     base_valid_next;
  reg [BurstLenWidth-1:0] burst_len_next;
  reg [WaitTimeWidth-1:0] wait_time_next;

  wire [AddrWidth-1:0] curr_addr = addr_dout;
  wire [AddrWidth-1:0] next_addr =
      base_addr +
      ({{(AddrWidth-BurstLenWidth){1'b0}}, burst_len} << DataWidthBytesLog) +
      ({{(AddrWidth-1){1'b0}}, 1'b1} << DataWidthBytesLog);

  assign addr_write = write_enable;
  assign burst_len_0_write = write_enable;
  assign burst_len_1_write = write_enable;

  always @* begin
    // defaults
    addr_read = 1'b0;
    write_enable = 1'b0;
    base_addr_next = base_addr;
    base_valid_next = base_valid;
    wait_time_next = wait_time;
    burst_len_next = burst_len;
    if (!addr_full_n || !burst_len_0_full_n || !burst_len_1_full_n) begin
      // output FIFO full, do nothing
    end else if (addr_empty_n) begin
      // read new item if non-empty
      addr_read = 1'b1;
      wait_time_next = 0;
      if (!base_valid) begin
        base_addr_next = curr_addr;
        base_valid_next = 1'b1;

        write_enable = 1'b0;
        burst_len_next = burst_len;
      end else begin
        if (next_addr == curr_addr && burst_len < max_burst_len) begin
          burst_len_next = burst_len + 1;

          write_enable = 1'b0;
          base_addr_next = base_addr;
          base_valid_next = base_valid;
        end else begin
          // no more burst detected
          addr_din = {burst_len, base_addr};
          burst_len_0_din = burst_len;
          burst_len_1_din = burst_len;
          write_enable = 1'b1;
          burst_len_next = 0;
          base_addr_next = curr_addr;

          base_valid_next = base_valid;
        end
      end
    end else if (base_valid) begin
      addr_read = 1'b0;
      if (wait_time < max_wait_time) begin
        wait_time_next = wait_time + 1;

        write_enable = 1'b0;
        base_addr_next = base_addr;
        base_valid_next = base_valid;
        burst_len_next = burst_len;
      end else begin
        addr_din = {burst_len, base_addr};
        burst_len_0_din = burst_len;
        burst_len_1_din = burst_len;
        write_enable = 1'b1;
        wait_time_next = 0;
        base_valid_next = 1'b0;
        burst_len_next = 0;

        base_addr_next = base_addr;
      end
    end
  end

  always @(posedge clk) begin
    if (rst) begin
      base_addr <= {AddrWidth{1'b0}};
      base_valid <= 1'd0;
      burst_len <= {BurstLenWidth{1'b0}};
      wait_time <= {WaitTimeWidth{1'b0}};
    end else begin
      base_addr <= base_addr_next;
      base_valid <= base_valid_next;
      burst_len <= burst_len_next;
      wait_time <= wait_time_next;
    end
  end

endmodule  // detect_burst

`default_nettype wire
