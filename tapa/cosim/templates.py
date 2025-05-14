__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
import os
import sys
from collections.abc import Sequence

from tapa.cosim.common import AXI, MAX_AXI_BRAM_ADDR_WIDTH, Arg

_logger = logging.getLogger().getChild(__name__)


def get_axi_ram_inst(axi_obj: AXI) -> str:
    # FIXME: test if using addr_width = 64 will cause problem in simulation
    return f"""
  parameter AXI_RAM_{axi_obj.name.upper()}_DATA_WIDTH = {axi_obj.data_width};
  parameter AXI_RAM_{axi_obj.name.upper()}_ADDR_WIDTH = {MAX_AXI_BRAM_ADDR_WIDTH};
  parameter AXI_RAM_{axi_obj.name.upper()}_STRB_WIDTH =
      AXI_RAM_{axi_obj.name.upper()}_DATA_WIDTH/8;
  parameter AXI_RAM_{axi_obj.name.upper()}_ID_WIDTH = 8;
  parameter AXI_RAM_{axi_obj.name.upper()}_PIPELINE_OUTPUT = 0;

  wire [AXI_RAM_{axi_obj.name.upper()}_ID_WIDTH-1:0]     axi_{axi_obj.name}_awid;
  wire [AXI_RAM_{axi_obj.name.upper()}_ADDR_WIDTH-1:0]   axi_{axi_obj.name}_awaddr;
  wire [7:0]                                          axi_{axi_obj.name}_awlen;
  wire [2:0]                                          axi_{axi_obj.name}_awsize;
  wire [1:0]                                          axi_{axi_obj.name}_awburst;
  wire                                                axi_{axi_obj.name}_awlock;
  wire [3:0]                                          axi_{axi_obj.name}_awcache;
  wire [2:0]                                          axi_{axi_obj.name}_awprot;
  wire                                                axi_{axi_obj.name}_awvalid;
  wire                                                axi_{axi_obj.name}_awready;
  wire [AXI_RAM_{axi_obj.name.upper()}_DATA_WIDTH-1:0]   axi_{axi_obj.name}_wdata;
  wire [AXI_RAM_{axi_obj.name.upper()}_STRB_WIDTH-1:0]   axi_{axi_obj.name}_wstrb;
  wire                                                axi_{axi_obj.name}_wlast;
  wire                                                axi_{axi_obj.name}_wvalid;
  wire                                                axi_{axi_obj.name}_wready;
  wire [AXI_RAM_{axi_obj.name.upper()}_ID_WIDTH-1:0]     axi_{axi_obj.name}_bid;
  wire [1:0]                                          axi_{axi_obj.name}_bresp;
  wire                                                axi_{axi_obj.name}_bvalid;
  wire                                                axi_{axi_obj.name}_bready;
  wire [AXI_RAM_{axi_obj.name.upper()}_ID_WIDTH-1:0]     axi_{axi_obj.name}_arid;
  wire [AXI_RAM_{axi_obj.name.upper()}_ADDR_WIDTH-1:0]   axi_{axi_obj.name}_araddr;
  wire [7:0]                                          axi_{axi_obj.name}_arlen;
  wire [2:0]                                          axi_{axi_obj.name}_arsize;
  wire [1:0]                                          axi_{axi_obj.name}_arburst;
  wire                                                axi_{axi_obj.name}_arlock;
  wire [3:0]                                          axi_{axi_obj.name}_arcache;
  wire [2:0]                                          axi_{axi_obj.name}_arprot;
  wire                                                axi_{axi_obj.name}_arvalid;
  wire                                                axi_{axi_obj.name}_arready;
  wire [AXI_RAM_{axi_obj.name.upper()}_ID_WIDTH-1:0]     axi_{axi_obj.name}_rid;
  wire [AXI_RAM_{axi_obj.name.upper()}_DATA_WIDTH-1:0]   axi_{axi_obj.name}_rdata;
  wire [1:0]                                          axi_{axi_obj.name}_rresp;
  wire                                                axi_{axi_obj.name}_rlast;
  wire                                                axi_{axi_obj.name}_rvalid;
  wire                                                axi_{axi_obj.name}_rready;

  // write out the contents into binary file
  reg        axi_ram_{axi_obj.name}_dump_mem = 0;
  wire [3:0] axi_{axi_obj.name}_awqos; // place holder
  wire [3:0] axi_{axi_obj.name}_arqos;

  axi_ram_{axi_obj.name} #
  (
    .DATA_WIDTH       (AXI_RAM_{axi_obj.name.upper()}_DATA_WIDTH),
    .ADDR_WIDTH       (AXI_RAM_{axi_obj.name.upper()}_ADDR_WIDTH),
    .STRB_WIDTH       (AXI_RAM_{axi_obj.name.upper()}_DATA_WIDTH/8),
    .ID_WIDTH         (AXI_RAM_{axi_obj.name.upper()}_ID_WIDTH),
    .PIPELINE_OUTPUT  (AXI_RAM_{axi_obj.name.upper()}_PIPELINE_OUTPUT)
  ) axi_ram_{axi_obj.name}_unit
  (
      .clk           (ap_clk),
      .rst           (~ap_rst_n),

      .s_axi_awid    (axi_{axi_obj.name}_awid    ),
      .s_axi_awaddr  (axi_{axi_obj.name}_awaddr  ),
      .s_axi_awlen   (axi_{axi_obj.name}_awlen   ),
      .s_axi_awsize  (axi_{axi_obj.name}_awsize  ),
      .s_axi_awburst (axi_{axi_obj.name}_awburst ),
      .s_axi_awlock  (axi_{axi_obj.name}_awlock  ),
      .s_axi_awcache (axi_{axi_obj.name}_awcache ),
      .s_axi_awprot  (axi_{axi_obj.name}_awprot  ),
      .s_axi_awvalid (axi_{axi_obj.name}_awvalid ),
      .s_axi_awready (axi_{axi_obj.name}_awready ),
      .s_axi_wdata   (axi_{axi_obj.name}_wdata   ),
      .s_axi_wstrb   (axi_{axi_obj.name}_wstrb   ),
      .s_axi_wlast   (axi_{axi_obj.name}_wlast   ),
      .s_axi_wvalid  (axi_{axi_obj.name}_wvalid  ),
      .s_axi_wready  (axi_{axi_obj.name}_wready  ),
      .s_axi_bid     (axi_{axi_obj.name}_bid     ),
      .s_axi_bresp   (axi_{axi_obj.name}_bresp   ),
      .s_axi_bvalid  (axi_{axi_obj.name}_bvalid  ),
      .s_axi_bready  (axi_{axi_obj.name}_bready  ),
      .s_axi_arid    (axi_{axi_obj.name}_arid    ),
      .s_axi_araddr  (axi_{axi_obj.name}_araddr  ),
      .s_axi_arlen   (axi_{axi_obj.name}_arlen   ),
      .s_axi_arsize  (axi_{axi_obj.name}_arsize  ),
      .s_axi_arburst (axi_{axi_obj.name}_arburst ),
      .s_axi_arlock  (axi_{axi_obj.name}_arlock  ),
      .s_axi_arcache (axi_{axi_obj.name}_arcache ),
      .s_axi_arprot  (axi_{axi_obj.name}_arprot  ),
      .s_axi_arvalid (axi_{axi_obj.name}_arvalid ),
      .s_axi_arready (axi_{axi_obj.name}_arready ),
      .s_axi_rid     (axi_{axi_obj.name}_rid     ),
      .s_axi_rdata   (axi_{axi_obj.name}_rdata   ),
      .s_axi_rresp   (axi_{axi_obj.name}_rresp   ),
      .s_axi_rlast   (axi_{axi_obj.name}_rlast   ),
      .s_axi_rvalid  (axi_{axi_obj.name}_rvalid  ),
      .s_axi_rready  (axi_{axi_obj.name}_rready  ),

      .dump_mem      (axi_ram_{axi_obj.name}_dump_mem)
  );

"""


def get_s_axi_control() -> str:
    return """
  parameter C_S_AXI_CONTROL_DATA_WIDTH = 32;
  parameter C_S_AXI_CONTROL_ADDR_WIDTH = 64;
  parameter C_S_AXI_DATA_WIDTH = 32;
  parameter C_S_AXI_CONTROL_WSTRB_WIDTH = 32 / 8;
  parameter C_S_AXI_WSTRB_WIDTH = 32 / 8;

  wire                                    s_axi_control_awvalid;
  wire                                    s_axi_control_awready;
  wire [C_S_AXI_CONTROL_ADDR_WIDTH-1:0]   s_axi_control_awaddr;
  wire                                    s_axi_control_wvalid;
  wire                                    s_axi_control_wready;
  wire [C_S_AXI_CONTROL_DATA_WIDTH-1:0]   s_axi_control_wdata;
  wire [C_S_AXI_CONTROL_WSTRB_WIDTH-1:0]  s_axi_control_wstrb;
  reg                                     s_axi_control_arvalid = 0;
  wire                                    s_axi_control_arready;
  wire [C_S_AXI_CONTROL_ADDR_WIDTH-1:0]   s_axi_control_araddr;
  wire                                    s_axi_control_rvalid;
  wire                                    s_axi_control_rready;
  wire [C_S_AXI_CONTROL_DATA_WIDTH-1:0]   s_axi_control_rdata;
  wire [1:0]                              s_axi_control_rresp;
  wire                                    s_axi_control_bvalid;
  wire                                    s_axi_control_bready;
  wire [1:0]                              s_axi_control_bresp;

  // use a large FIFO to buffer the command to s_axi_control
  // in this way we don't need to worry about flow control
  reg [63:0] s_axi_aw_din = 0;
  reg        s_axi_aw_write = 0;

  fifo_srl #(
    .DATA_WIDTH(64),
    .ADDR_WIDTH(6),
    .DEPTH(64)
  ) s_axi_aw(
    .clk(ap_clk),
    .reset(~ap_rst_n),

    // write
    .if_full_n   (),
    .if_write    (s_axi_aw_write),
    .if_din      (s_axi_aw_din  ),

    // read
    .if_empty_n  (s_axi_control_awvalid),
    .if_read     (s_axi_control_awready),
    .if_dout     (s_axi_control_awaddr),

    .if_write_ce (1'b1),
    .if_read_ce  (1'b1)

  );

  reg [511:0] s_axi_w_din = 0;
  reg         s_axi_w_write = 0;

  fifo_srl #(
    .DATA_WIDTH(32),
    .ADDR_WIDTH(6),
    .DEPTH(64)
  ) s_axi_w(
    .clk(ap_clk),
    .reset(~ap_rst_n),

    // write
    .if_full_n   (),
    .if_write    (s_axi_w_write),
    .if_din      (s_axi_w_din  ),

    // read
    .if_empty_n  (s_axi_control_wvalid),
    .if_read     (s_axi_control_wready),
    .if_dout     (s_axi_control_wdata),

    .if_write_ce (1'b1),
    .if_read_ce  (1'b1)

  );
"""


def get_axis(args: Sequence[Arg]) -> str:
    lines = [get_stream_typedef(args)]
    axis_args = [arg for arg in args if arg.is_stream]
    for arg in axis_args:
        lines.append(
            f"""
  // data ports connected to DUT
  packed_uint{arg.port.data_width}_t axis_{arg.name}_tdata;
  logic axis_{arg.name}_tlast;

  // data ports connected to testbench
  unpacked_uint{arg.port.data_width + 1}_t axis_{arg.name}_tdata_unpacked;
  unpacked_uint{arg.port.data_width + 1}_t axis_{arg.name}_tdata_unpacked_next;

  logic axis_{arg.name}_tvalid = 0;
  logic axis_{arg.name}_tready = 0;
  logic axis_{arg.name}_tvalid_next = 0;
  logic axis_{arg.name}_tready_next = 0;
"""
        )
    return "\n".join(lines)


def get_fifo(args: Sequence[Arg]) -> str:
    lines = [get_stream_typedef(args)]
    fifo_args = [arg for arg in args if arg.is_stream]
    for arg in fifo_args:
        lines.append(
            f"""
  // data ports connected to DUT
  packed_uint{arg.port.data_width + 1}_t fifo_{arg.name}_data;

  // data ports connected to testbench
  unpacked_uint{arg.port.data_width + 1}_t fifo_{arg.name}_data_unpacked;
  unpacked_uint{arg.port.data_width + 1}_t fifo_{arg.name}_data_unpacked_next;

  logic fifo_{arg.name}_valid = 0;
  logic fifo_{arg.name}_ready = 0;
  logic fifo_{arg.name}_valid_next = 0;
  logic fifo_{arg.name}_ready_next = 0;
"""
        )
    return "\n".join(lines)


def get_stream_typedef(args: Sequence[Arg]) -> str:
    stream_args = [arg for arg in args if arg.is_stream]

    # create type alias for widths used for axis
    widths = set()
    for arg in stream_args:
        widths |= {
            arg.port.data_width,
            arg.port.data_width + 1,  # for eot
        }

    lines = []
    # list comprehension is only more readable when short
    # ruff: noqa: PERF401
    for width in widths:
        lines.append(
            f"""
  typedef logic unpacked_uint{width}_t[{width - 1}:0];
  typedef logic [{width - 1}:0] packed_uint{width}_t;
"""
        )

    return "\n".join(lines)


def get_vitis_dut(top_name: str, args: Sequence[Arg]) -> str:
    dut = f"""
  {top_name} dut (
    .s_axi_control_AWVALID (s_axi_control_awvalid),
    .s_axi_control_AWREADY (s_axi_control_awready),
    .s_axi_control_AWADDR  (s_axi_control_awaddr ),

    .s_axi_control_WVALID  (s_axi_control_wvalid ),
    .s_axi_control_WREADY  (s_axi_control_wready ),
    .s_axi_control_WDATA   (s_axi_control_wdata  ),
    .s_axi_control_WSTRB   ({{64{{1'b1}} }}      ),

    // keep polling the control registers
    .s_axi_control_ARVALID (s_axi_control_arvalid),
    .s_axi_control_ARREADY (s_axi_control_arready),
    .s_axi_control_ARADDR  ('h00 ),

    .s_axi_control_RVALID  (s_axi_control_rvalid ),
    .s_axi_control_RREADY  (1 ),
    .s_axi_control_RDATA   (s_axi_control_rdata  ),
    .s_axi_control_RRESP   (                     ),

    .s_axi_control_BVALID  (s_axi_control_bvalid ),
    .s_axi_control_BREADY  (1'b1                 ),
    .s_axi_control_BRESP   (s_axi_control_bresp  ),
"""

    for arg in args:
        if arg.is_mmap:
            dut += f"""
    .m_axi_{arg.name}_ARADDR  (axi_{arg.name}_araddr ),
    .m_axi_{arg.name}_ARBURST (axi_{arg.name}_arburst),
    .m_axi_{arg.name}_ARCACHE (axi_{arg.name}_arcache),
    .m_axi_{arg.name}_ARID    (axi_{arg.name}_arid   ),
    .m_axi_{arg.name}_ARLEN   (axi_{arg.name}_arlen  ),
    .m_axi_{arg.name}_ARLOCK  (axi_{arg.name}_arlock ),
    .m_axi_{arg.name}_ARPROT  (axi_{arg.name}_arprot ),
    .m_axi_{arg.name}_ARQOS   (axi_{arg.name}_arqos  ),
    .m_axi_{arg.name}_ARREADY (axi_{arg.name}_arready),
    .m_axi_{arg.name}_ARSIZE  (axi_{arg.name}_arsize ),
    .m_axi_{arg.name}_ARVALID (axi_{arg.name}_arvalid),
    .m_axi_{arg.name}_AWADDR  (axi_{arg.name}_awaddr ),
    .m_axi_{arg.name}_AWBURST (axi_{arg.name}_awburst),
    .m_axi_{arg.name}_AWCACHE (axi_{arg.name}_awcache),
    .m_axi_{arg.name}_AWID    (axi_{arg.name}_awid   ),
    .m_axi_{arg.name}_AWLEN   (axi_{arg.name}_awlen  ),
    .m_axi_{arg.name}_AWLOCK  (axi_{arg.name}_awlock ),
    .m_axi_{arg.name}_AWPROT  (axi_{arg.name}_awprot ),
    .m_axi_{arg.name}_AWQOS   (axi_{arg.name}_awqos  ),
    .m_axi_{arg.name}_AWREADY (axi_{arg.name}_awready),
    .m_axi_{arg.name}_AWSIZE  (axi_{arg.name}_awsize ),
    .m_axi_{arg.name}_AWVALID (axi_{arg.name}_awvalid),
    .m_axi_{arg.name}_BID     (axi_{arg.name}_bid    ),
    .m_axi_{arg.name}_BREADY  (axi_{arg.name}_bready ),
    .m_axi_{arg.name}_BRESP   (axi_{arg.name}_bresp  ),
    .m_axi_{arg.name}_BVALID  (axi_{arg.name}_bvalid ),
    .m_axi_{arg.name}_RDATA   (axi_{arg.name}_rdata  ),
    .m_axi_{arg.name}_RID     (axi_{arg.name}_rid    ),
    .m_axi_{arg.name}_RLAST   (axi_{arg.name}_rlast  ),
    .m_axi_{arg.name}_RREADY  (axi_{arg.name}_rready ),
    .m_axi_{arg.name}_RRESP   (axi_{arg.name}_rresp  ),
    .m_axi_{arg.name}_RVALID  (axi_{arg.name}_rvalid ),
    .m_axi_{arg.name}_WDATA   (axi_{arg.name}_wdata  ),
    .m_axi_{arg.name}_WLAST   (axi_{arg.name}_wlast  ),
    .m_axi_{arg.name}_WREADY  (axi_{arg.name}_wready ),
    .m_axi_{arg.name}_WSTRB   (axi_{arg.name}_wstrb  ),
    .m_axi_{arg.name}_WVALID  (axi_{arg.name}_wvalid ),
"""
        if arg.is_stream:
            dut += f"""
    .{arg.name}_TDATA  (axis_{arg.name}_tdata ),
    .{arg.name}_TVALID (axis_{arg.name}_tvalid),
    .{arg.name}_TREADY (axis_{arg.name}_tready),
    .{arg.name}_TLAST  (axis_{arg.name}_tlast ),
"""

    dut += """
    .ap_clk          (ap_clk       ),
    .ap_rst_n        (ap_rst_n     ),
    .interrupt       (             )
  );
  """

    return dut


def get_hls_dut(
    top_name: str,
    top_is_leaf_task: bool,
    args: Sequence[Arg],
    scalar_to_val: dict[str, str],
) -> str:
    dut = f"\n  {top_name} dut (\n"

    for arg in args:
        if arg.is_mmap:
            msg = "mmap is not supported in HLS"
            raise NotImplementedError(msg)

        if arg.is_stream and arg.port.is_istream:
            dut += f"""
    .{arg.name}_s_dout(fifo_{arg.name}_data),
    .{arg.name}_s_empty_n(fifo_{arg.name}_valid),
    .{arg.name}_s_read(fifo_{arg.name}_ready),
"""
            if top_is_leaf_task:
                dut += f"""
    .{arg.name}_peek_dout(fifo_{arg.name}_data),
    .{arg.name}_peek_empty_n(fifo_{arg.name}_valid),
"""

        if arg.is_stream and arg.port.is_ostream:
            dut += f"""
    .{arg.name}_s_din(fifo_{arg.name}_data),
    .{arg.name}_s_full_n(fifo_{arg.name}_ready),
    .{arg.name}_s_write(fifo_{arg.name}_valid),
"""

        if arg.is_scalar:
            dut += f"""
    .{arg.name}({scalar_to_val.get(arg.name, 0)} & REG_MASK_32_BIT),\n
"""

    dut += """
    .ap_clk(ap_clk),
    .ap_rst_n(ap_rst_n),
    .ap_start(ap_start),
    .ap_done(ap_done),
    .ap_ready(ap_ready),
    .ap_idle(ap_idle)
  );
"""

    return dut


def get_vitis_test_signals(
    arg_to_reg_addrs: dict[str, str],
    scalar_arg_to_val: dict[str, str],
    args: list[Arg],
) -> str:
    axis_dpi_calls = []
    axis_assignments = []
    for arg in args:
        if not arg.is_stream:
            continue
        if arg.port.is_istream:
            axis_dpi_calls.append(
                f"""
    tapa::istream(
        axis_{arg.name}_tdata_unpacked_next,
        axis_{arg.name}_tvalid_next,
        axis_{arg.name}_tready,
        "{arg.name}"
    );

    axis_{arg.name}_tdata_unpacked <= axis_{arg.name}_tdata_unpacked_next;
    axis_{arg.name}_tvalid <= axis_{arg.name}_tvalid_next;
"""
            )
            axis_assignments.append(
                f"""
    assign {{axis_{arg.name}_tlast, axis_{arg.name}_tdata}} =
        packed_uint{arg.port.data_width + 1}_t'(axis_{arg.name}_tdata_unpacked);
"""
            )
        elif arg.port.is_ostream:
            axis_dpi_calls.append(
                f"""
    tapa::ostream(
        axis_{arg.name}_tdata_unpacked,
        axis_{arg.name}_tready_next,
        axis_{arg.name}_tvalid,
        "{arg.name}"
    );

    axis_{arg.name}_tready <= axis_{arg.name}_tready_next;
"""
            )
            axis_assignments.append(
                f"""
    assign axis_{arg.name}_tdata_unpacked =
        unpacked_uint{arg.port.data_width + 1}_t'
            ({{axis_{arg.name}_tlast, axis_{arg.name}_tdata}});
"""
            )
        else:
            _logger.fatal("unexpected arg.port.mode: %s", arg.port.mode)

    newline = "\n"
    test = f"""
  parameter HALF_CLOCK_PERIOD = 2;
  parameter CLOCK_PERIOD = HALF_CLOCK_PERIOD * 2;

  // clock
  always begin
      ap_clk = 1'b0;
      #HALF_CLOCK_PERIOD;
      ap_clk = 1'b1;
      #HALF_CLOCK_PERIOD;
  end

  // axis signals
  always @(posedge ap_clk) begin
    if (kernel_started) begin
      {newline.join(axis_dpi_calls)}
    end
  end

{newline.join(axis_assignments)}

  initial begin
    // reset the DUT
    ap_rst_n = 1'b0;
    #(CLOCK_PERIOD*1000);

    // enable the DUT
    ap_rst_n = 1'b1;
    #(CLOCK_PERIOD*100);

"""

    for arg, addrs in arg_to_reg_addrs.items():
        val = scalar_arg_to_val.get(arg, 0)
        test += (
            f"    s_axi_aw_write <= 1; s_axi_aw_din <= {addrs[0]}; "
            "s_axi_w_write <= 1; "
            f"s_axi_w_din <= {val} & REG_MASK_32_BIT; @(posedge ap_clk);\n"
        )
        if len(addrs) == 2:  # noqa: PLR2004
            test += (
                f"    s_axi_aw_write <= 1; s_axi_aw_din <= {addrs[1]}; "
                "s_axi_w_write <= 1; "
                f"s_axi_w_din <= ({val} >> 32) & REG_MASK_32_BIT; "
                f"@(posedge ap_clk);\n"
            )

    test += """
    // start the DUT
    s_axi_aw_write <= 1;
    s_axi_aw_din <= 'h00;
    s_axi_w_write <= 1;
    s_axi_w_din <= 1;
    @(posedge ap_clk);

    // stop writing control signal
    s_axi_aw_write <= 0;
    s_axi_w_write <= 0;
    @(posedge ap_clk);

    kernel_started <= 1;
    @(posedge ap_clk);

    // start polling ap_done
    #(CLOCK_PERIOD*1000);
    s_axi_control_arvalid <= 1'b1 & s_axi_control_arready;
    @(posedge ap_clk);

  end
"""

    dump_signals = "\n".join(
        f"          axi_ram_{arg.name}_dump_mem <= 1;" for arg in args if arg.is_mmap
    )
    test += f"""
  // polling on ap_done
  always @(posedge ap_clk) begin
    if (~ap_rst_n) begin
    end
    else begin
      if (s_axi_control_rvalid) begin
        if (s_axi_control_rdata[1]) begin
{dump_signals}
          #(CLOCK_PERIOD*100)
          $finish;
        end
      end
    end
  end
"""

    return test


def get_hls_test_signals(args: list[Arg]) -> str:
    fifo_dpi_calls = []
    fifo_assignments = []

    # build signals for FIFOs
    for arg in args:
        if not arg.is_stream:
            continue
        if arg.port.is_istream:
            fifo_dpi_calls.append(f"""
    tapa::istream(
        fifo_{arg.name}_data_unpacked_next,
        fifo_{arg.name}_valid_next,
        fifo_{arg.name}_ready,
        "{arg.name}"
    );

    fifo_{arg.name}_data_unpacked <= fifo_{arg.name}_data_unpacked_next;
    fifo_{arg.name}_valid <= fifo_{arg.name}_valid_next;
""")
            fifo_assignments.append(f"""
    assign fifo_{arg.name}_data =
        packed_uint{arg.port.data_width + 1}_t'(fifo_{arg.name}_data_unpacked);
""")
        elif arg.port.is_ostream:
            fifo_dpi_calls.append(f"""
    tapa::ostream(
        fifo_{arg.name}_data_unpacked,
        fifo_{arg.name}_ready_next,
        fifo_{arg.name}_valid,
        "{arg.name}"
    );

    fifo_{arg.name}_ready <= fifo_{arg.name}_ready_next;
""")
            fifo_assignments.append(f"""
    assign fifo_{arg.name}_data_unpacked =
        unpacked_uint{arg.port.data_width + 1}_t'(fifo_{arg.name}_data);
""")
        else:
            msg = f"unexpected arg.port.mode: {arg.port.mode}"
            raise ValueError(msg)

    newline = "\n"
    return f"""
  parameter HALF_CLOCK_PERIOD = 2;
  parameter CLOCK_PERIOD = HALF_CLOCK_PERIOD * 2;

  // clock
  always begin
      ap_clk = 1'b0;
      #HALF_CLOCK_PERIOD;
      ap_clk = 1'b1;
      #HALF_CLOCK_PERIOD;
  end

  // fifo signals
  always @(posedge ap_clk) begin
    if (kernel_started) begin
      {newline.join(fifo_dpi_calls)}
    end
  end

{newline.join(fifo_assignments)}

  initial begin
    // reset the DUT
    ap_rst_n = 1'b0;
    #(CLOCK_PERIOD*1000);

    // enable the DUT
    ap_rst_n = 1'b1;
    #(CLOCK_PERIOD*100);

    // start the DUT
    ap_start <= 1'b1;
    @(posedge ap_clk);

    kernel_started <= 1;
    @(posedge ap_clk);

    fork
      begin
        wait(ap_ready);
        ap_start <= 1'b0;
        @(posedge ap_clk);
      end
      begin
        wait(ap_done);
      end
      begin
        // In some Vitis versions, there is a bug where ap_done is asserted
        // before the operation is done. Preliminary investigations show that
        // ap_done is asserted as soon as the last state is reached, which
        // does not necessarily mean that the operation is done. Instead,
        // wait for ap_idle to be asserted (which is state_1 & !ap_start)
        // seems to be a workaround. We are not sure yet, if there is a
        // possibility that ap_idle has the same bug. If so, there is no
        // reasonable workaround to identify the end of the operation.
        wait(ap_idle);
      end
    join

    #(CLOCK_PERIOD*100);
    $finish;
  end
"""


def get_begin() -> str:
    return """
`timescale 1 ns / 1 ps

package tapa;
  import "DPI-C" function void istream(
    output logic  dout[],
    output logic  empty_n,
    input  logic  read,
    input  string id
  );
  import "DPI-C" function void ostream(
    input  logic  din[],
    output logic  full_n,
    input  logic  write,
    input  string id
  );
endpackage

module test();

  reg ap_clk = 0;
  reg ap_rst_n = 0;
  reg ap_start = 0;
  wire ap_done;
  wire ap_ready;
  wire ap_idle;

  reg kernel_started = 0;

  wire [31:0] REG_MASK_32_BIT = {{32{{1\'b1}}}};

"""


def get_end() -> str:
    return """

endmodule
"""


def get_axi_ram_module(axi: AXI, input_data_path: str, c_array_size: int) -> str:
    """Generate the AXI RAM module for cosimulation."""
    if axi.data_width / 8 * c_array_size > 2**MAX_AXI_BRAM_ADDR_WIDTH:
        _logger.error(
            "The current cosim data size is larger than the template "
            "threshold (32-bit address). Please reduce cosim data size."
        )
        sys.exit(1)

    if input_data_path:
        assert os.path.exists(input_data_path)

    return f"""
/*

Copyright (c) 2018 Alex Forencich

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

// Language: Verilog 2001

`resetall
`timescale 1ns / 1ps
`default_nettype none

/*
 * AXI4 RAM
 */
module axi_ram_{axi.name} #
(
    // Width of data bus in bits
    parameter DATA_WIDTH = {axi.data_width},
    // Width of address bus in bits
    parameter ADDR_WIDTH = {MAX_AXI_BRAM_ADDR_WIDTH},
    // Width of wstrb (width of data bus in words)
    parameter STRB_WIDTH = (DATA_WIDTH/8),
    // Width of ID signal
    parameter ID_WIDTH = 8,
    // Extra pipeline register on output
    parameter PIPELINE_OUTPUT = 0
)
(
    input  wire                   dump_mem, // write out the contents into binary file
    input  wire                   clk,
    input  wire                   rst,

    input  wire [ID_WIDTH-1:0]    s_axi_awid,
    input  wire [ADDR_WIDTH-1:0]  s_axi_awaddr,
    input  wire [7:0]             s_axi_awlen,
    input  wire [2:0]             s_axi_awsize,
    input  wire [1:0]             s_axi_awburst,
    input  wire                   s_axi_awlock,
    input  wire [3:0]             s_axi_awcache,
    input  wire [2:0]             s_axi_awprot,
    input  wire                   s_axi_awvalid,
    output wire                   s_axi_awready,
    input  wire [DATA_WIDTH-1:0]  s_axi_wdata,
    input  wire [STRB_WIDTH-1:0]  s_axi_wstrb,
    input  wire                   s_axi_wlast,
    input  wire                   s_axi_wvalid,
    output wire                   s_axi_wready,
    output wire [ID_WIDTH-1:0]    s_axi_bid,
    output wire [1:0]             s_axi_bresp,
    output wire                   s_axi_bvalid,
    input  wire                   s_axi_bready,
    input  wire [ID_WIDTH-1:0]    s_axi_arid,
    input  wire [ADDR_WIDTH-1:0]  s_axi_araddr,
    input  wire [7:0]             s_axi_arlen,
    input  wire [2:0]             s_axi_arsize,
    input  wire [1:0]             s_axi_arburst,
    input  wire                   s_axi_arlock,
    input  wire [3:0]             s_axi_arcache,
    input  wire [2:0]             s_axi_arprot,
    input  wire                   s_axi_arvalid,
    output wire                   s_axi_arready,
    output wire [ID_WIDTH-1:0]    s_axi_rid,
    output wire [DATA_WIDTH-1:0]  s_axi_rdata,
    output wire [1:0]             s_axi_rresp,
    output wire                   s_axi_rlast,
    output wire                   s_axi_rvalid,
    input  wire                   s_axi_rready
);

parameter VALID_ADDR_WIDTH = ADDR_WIDTH - $clog2(STRB_WIDTH);
parameter WORD_WIDTH = STRB_WIDTH;
parameter WORD_SIZE = DATA_WIDTH/WORD_WIDTH;

//////////////////////////////////////////////////////////////////////

reg [DATA_WIDTH-1:0] mem[(2**VALID_ADDR_WIDTH)-1:0];
integer fp;
integer read_size;
reg [7:0] temp;

integer i_rd, j_rd;
initial begin
  fp = $fopen("{input_data_path}", "rb");
  for (i_rd = 0; i_rd < {c_array_size} ; i_rd = i_rd + 1) begin
    for (j_rd = 0; j_rd < DATA_WIDTH / 8; j_rd = j_rd + 1) begin
      $fread(temp, fp);
      mem[i_rd][j_rd*8 +: 8] = temp;
    end
  end
end

integer i_wr, j_wr;
always @* begin
  if (dump_mem) begin
    fp = $fopen("{input_data_path.replace(".bin", "_out.bin")}", "wb");
    for (i_wr = 0; i_wr < {c_array_size}; i_wr = i_wr + 1) begin
      for (j_wr = 0; j_wr < DATA_WIDTH / 8; j_wr = j_wr + 1) begin
        $fwrite(fp, "%c", mem[i_wr][j_wr * 8 +: 8] );
      end
    end
  end
end

//////////////////////////////////////////////////////////////////////

// bus width assertions
initial begin
    if (WORD_SIZE * STRB_WIDTH != DATA_WIDTH) begin
        $error("Error: AXI data width not evenly divisible (instance %m)");
        $finish;
    end

    if (2**$clog2(WORD_WIDTH) != WORD_WIDTH) begin
        $error("Error: AXI word width must be even power of two (instance %m)");
        $finish;
    end
end

localparam [0:0]
    READ_STATE_IDLE = 1'd0,
    READ_STATE_BURST = 1'd1;

reg [0:0] read_state_reg = READ_STATE_IDLE, read_state_next;

localparam [1:0]
    WRITE_STATE_IDLE = 2'd0,
    WRITE_STATE_BURST = 2'd1,
    WRITE_STATE_RESP = 2'd2;

reg [1:0] write_state_reg = WRITE_STATE_IDLE, write_state_next;

reg mem_wr_en;
reg mem_rd_en;

reg [ID_WIDTH-1:0] read_id_reg = {{ID_WIDTH{{1'b0}}}}, read_id_next;
reg [ADDR_WIDTH-1:0] read_addr_reg = {{ADDR_WIDTH{{1'b0}}}}, read_addr_next;
reg [7:0] read_count_reg = 8'd0, read_count_next;
reg [2:0] read_size_reg = 3'd0, read_size_next;
reg [1:0] read_burst_reg = 2'd0, read_burst_next;
reg [ID_WIDTH-1:0] write_id_reg = {{ID_WIDTH{{1'b0}}}}, write_id_next;
reg [ADDR_WIDTH-1:0] write_addr_reg = {{ADDR_WIDTH{{1'b0}}}}, write_addr_next;
reg [7:0] write_count_reg = 8'd0, write_count_next;
reg [2:0] write_size_reg = 3'd0, write_size_next;
reg [1:0] write_burst_reg = 2'd0, write_burst_next;

reg s_axi_awready_reg = 1'b0, s_axi_awready_next;
reg s_axi_wready_reg = 1'b0, s_axi_wready_next;
reg [ID_WIDTH-1:0] s_axi_bid_reg = {{ID_WIDTH{{1'b0}}}}, s_axi_bid_next;
reg s_axi_bvalid_reg = 1'b0, s_axi_bvalid_next;
reg s_axi_arready_reg = 1'b0, s_axi_arready_next;
reg [ID_WIDTH-1:0] s_axi_rid_reg = {{ID_WIDTH{{1'b0}}}}, s_axi_rid_next;
reg [DATA_WIDTH-1:0] s_axi_rdata_reg = {{DATA_WIDTH{{1'b0}}}}, s_axi_rdata_next;
reg s_axi_rlast_reg = 1'b0, s_axi_rlast_next;
reg s_axi_rvalid_reg = 1'b0, s_axi_rvalid_next;
reg [ID_WIDTH-1:0] s_axi_rid_pipe_reg = {{ID_WIDTH{{1'b0}}}};
reg [DATA_WIDTH-1:0] s_axi_rdata_pipe_reg = {{DATA_WIDTH{{1'b0}}}};
reg s_axi_rlast_pipe_reg = 1'b0;
reg s_axi_rvalid_pipe_reg = 1'b0;

// (* RAM_STYLE="BLOCK" *)

wire [VALID_ADDR_WIDTH-1:0] s_axi_awaddr_valid =
    s_axi_awaddr >> (ADDR_WIDTH - VALID_ADDR_WIDTH);
wire [VALID_ADDR_WIDTH-1:0] s_axi_araddr_valid =
    s_axi_araddr >> (ADDR_WIDTH - VALID_ADDR_WIDTH);
wire [VALID_ADDR_WIDTH-1:0] read_addr_valid =
    read_addr_reg >> (ADDR_WIDTH - VALID_ADDR_WIDTH);
wire [VALID_ADDR_WIDTH-1:0] write_addr_valid =
    write_addr_reg >> (ADDR_WIDTH - VALID_ADDR_WIDTH);

assign s_axi_awready = s_axi_awready_reg;
assign s_axi_wready = s_axi_wready_reg;
assign s_axi_bid = s_axi_bid_reg;
assign s_axi_bresp = 2'b00;
assign s_axi_bvalid = s_axi_bvalid_reg;
assign s_axi_arready = s_axi_arready_reg;
assign s_axi_rid = PIPELINE_OUTPUT ? s_axi_rid_pipe_reg : s_axi_rid_reg;
assign s_axi_rdata = PIPELINE_OUTPUT ? s_axi_rdata_pipe_reg : s_axi_rdata_reg;
assign s_axi_rresp = 2'b00;
assign s_axi_rlast = PIPELINE_OUTPUT ? s_axi_rlast_pipe_reg : s_axi_rlast_reg;
assign s_axi_rvalid = PIPELINE_OUTPUT ? s_axi_rvalid_pipe_reg : s_axi_rvalid_reg;


always @* begin
    write_state_next = WRITE_STATE_IDLE;

    mem_wr_en = 1'b0;

    write_id_next = write_id_reg;
    write_addr_next = write_addr_reg;
    write_count_next = write_count_reg;
    write_size_next = write_size_reg;
    write_burst_next = write_burst_reg;

    s_axi_awready_next = 1'b0;
    s_axi_wready_next = 1'b0;
    s_axi_bid_next = s_axi_bid_reg;
    s_axi_bvalid_next = s_axi_bvalid_reg && !s_axi_bready;

    case (write_state_reg)
        WRITE_STATE_IDLE: begin
            s_axi_awready_next = 1'b1;

            if (s_axi_awready && s_axi_awvalid) begin
                write_id_next = s_axi_awid;
                write_addr_next = s_axi_awaddr;
                write_count_next = s_axi_awlen;
                write_size_next = s_axi_awsize < $clog2(STRB_WIDTH)
                    ? s_axi_awsize
                    : $clog2(STRB_WIDTH);
                write_burst_next = s_axi_awburst;

                s_axi_awready_next = 1'b0;
                s_axi_wready_next = 1'b1;
                write_state_next = WRITE_STATE_BURST;
            end else begin
                write_state_next = WRITE_STATE_IDLE;
            end
        end
        WRITE_STATE_BURST: begin
            s_axi_wready_next = 1'b1;

            if (s_axi_wready && s_axi_wvalid) begin
                mem_wr_en = 1'b1;
                if (write_burst_reg != 2'b00) begin
                    write_addr_next = write_addr_reg + (1 << write_size_reg);
                end
                write_count_next = write_count_reg - 1;
                if (write_count_reg > 0) begin
                    write_state_next = WRITE_STATE_BURST;
                end else begin
                    s_axi_wready_next = 1'b0;
                    if (s_axi_bready || !s_axi_bvalid) begin
                        s_axi_bid_next = write_id_reg;
                        s_axi_bvalid_next = 1'b1;
                        s_axi_awready_next = 1'b1;
                        write_state_next = WRITE_STATE_IDLE;
                    end else begin
                        write_state_next = WRITE_STATE_RESP;
                    end
                end
            end else begin
                write_state_next = WRITE_STATE_BURST;
            end
        end
        WRITE_STATE_RESP: begin
            if (s_axi_bready || !s_axi_bvalid) begin
                s_axi_bid_next = write_id_reg;
                s_axi_bvalid_next = 1'b1;
                s_axi_awready_next = 1'b1;
                write_state_next = WRITE_STATE_IDLE;
            end else begin
                write_state_next = WRITE_STATE_RESP;
            end
        end
    endcase
end

integer i;
always @(posedge clk) begin
    write_state_reg <= write_state_next;

    write_id_reg <= write_id_next;
    write_addr_reg <= write_addr_next;
    write_count_reg <= write_count_next;
    write_size_reg <= write_size_next;
    write_burst_reg <= write_burst_next;

    s_axi_awready_reg <= s_axi_awready_next;
    s_axi_wready_reg <= s_axi_wready_next;
    s_axi_bid_reg <= s_axi_bid_next;
    s_axi_bvalid_reg <= s_axi_bvalid_next;

    for (i = 0; i < WORD_WIDTH; i = i + 1) begin
        if (mem_wr_en & s_axi_wstrb[i]) begin
            mem[write_addr_valid][WORD_SIZE*i +: WORD_SIZE] <=
                s_axi_wdata[WORD_SIZE*i +: WORD_SIZE];
        end
    end

    if (rst) begin
        write_state_reg <= WRITE_STATE_IDLE;

        s_axi_awready_reg <= 1'b0;
        s_axi_wready_reg <= 1'b0;
        s_axi_bvalid_reg <= 1'b0;
    end
end

always @* begin
    read_state_next = READ_STATE_IDLE;

    mem_rd_en = 1'b0;

    s_axi_rid_next = s_axi_rid_reg;
    s_axi_rlast_next = s_axi_rlast_reg;
    s_axi_rvalid_next =
      s_axi_rvalid_reg &&
      !(s_axi_rready || (PIPELINE_OUTPUT && !s_axi_rvalid_pipe_reg));

    read_id_next = read_id_reg;
    read_addr_next = read_addr_reg;
    read_count_next = read_count_reg;
    read_size_next = read_size_reg;
    read_burst_next = read_burst_reg;

    s_axi_arready_next = 1'b0;

    case (read_state_reg)
        READ_STATE_IDLE: begin
            s_axi_arready_next = 1'b1;

            if (s_axi_arready && s_axi_arvalid) begin
                read_id_next = s_axi_arid;
                read_addr_next = s_axi_araddr;
                read_count_next = s_axi_arlen;
                read_size_next = s_axi_arsize < $clog2(STRB_WIDTH)
                                    ? s_axi_arsize
                                    : $clog2(STRB_WIDTH);
                read_burst_next = s_axi_arburst;

                s_axi_arready_next = 1'b0;
                read_state_next = READ_STATE_BURST;
            end else begin
                read_state_next = READ_STATE_IDLE;
            end
        end
        READ_STATE_BURST: begin
            if (s_axi_rready ||
                (PIPELINE_OUTPUT && !s_axi_rvalid_pipe_reg) ||
                !s_axi_rvalid_reg) begin
                mem_rd_en = 1'b1;
                s_axi_rvalid_next = 1'b1;
                s_axi_rid_next = read_id_reg;
                s_axi_rlast_next = read_count_reg == 0;
                if (read_burst_reg != 2'b00) begin
                    read_addr_next = read_addr_reg + (1 << read_size_reg);
                end
                read_count_next = read_count_reg - 1;
                if (read_count_reg > 0) begin
                    read_state_next = READ_STATE_BURST;
                end else begin
                    s_axi_arready_next = 1'b1;
                    read_state_next = READ_STATE_IDLE;
                end
            end else begin
                read_state_next = READ_STATE_BURST;
            end
        end
    endcase
end

always @(posedge clk) begin
    read_state_reg <= read_state_next;

    read_id_reg <= read_id_next;
    read_addr_reg <= read_addr_next;
    read_count_reg <= read_count_next;
    read_size_reg <= read_size_next;
    read_burst_reg <= read_burst_next;

    s_axi_arready_reg <= s_axi_arready_next;
    s_axi_rid_reg <= s_axi_rid_next;
    s_axi_rlast_reg <= s_axi_rlast_next;
    s_axi_rvalid_reg <= s_axi_rvalid_next;

    if (mem_rd_en) begin
        s_axi_rdata_reg <= mem[read_addr_valid];
    end

    if (!s_axi_rvalid_pipe_reg || s_axi_rready) begin
        s_axi_rid_pipe_reg <= s_axi_rid_reg;
        s_axi_rdata_pipe_reg <= s_axi_rdata_reg;
        s_axi_rlast_pipe_reg <= s_axi_rlast_reg;
        s_axi_rvalid_pipe_reg <= s_axi_rvalid_reg;
    end

    if (rst) begin
        read_state_reg <= READ_STATE_IDLE;

        s_axi_arready_reg <= 1'b0;
        s_axi_rvalid_reg <= 1'b0;
        s_axi_rvalid_pipe_reg <= 1'b0;
    end
end

endmodule

"""


def get_srl_fifo_template() -> str:
    return """`default_nettype none

// first-word fall-through (FWFT) FIFO using shift register LUT
// based on HLS generated code
module fifo_srl #(
  parameter MEM_STYLE  = "shiftreg",
  parameter DATA_WIDTH = 512,
  parameter ADDR_WIDTH = 64,
  parameter DEPTH      = 32
) (
  input wire clk,
  input wire reset,

  // write
  output wire                  if_full_n,
  input  wire                  if_write_ce,
  input  wire                  if_write,
  input  wire [DATA_WIDTH-1:0] if_din,

  // read
  output wire                  if_empty_n,
  input  wire                  if_read_ce,
  input  wire                  if_read,
  output wire [DATA_WIDTH-1:0] if_dout
);

  wire [ADDR_WIDTH - 1:0] shift_reg_addr;
  wire [DATA_WIDTH - 1:0] shift_reg_data;
  wire [DATA_WIDTH - 1:0] shift_reg_q;
  wire                    shift_reg_ce;
  reg  [ADDR_WIDTH:0]     out_ptr;
  reg                     internal_empty_n;
  reg                     internal_full_n;

  reg [DATA_WIDTH-1:0] mem [0:DEPTH-1];

  assign if_empty_n = internal_empty_n;
  assign if_full_n = internal_full_n;
  assign shift_reg_data = if_din;
  assign if_dout = shift_reg_q;

  assign shift_reg_addr = out_ptr[ADDR_WIDTH] == 1'b0 ? out_ptr[ADDR_WIDTH-1:0]
                                                      : {ADDR_WIDTH{1'b0}};
  assign shift_reg_ce = (if_write & if_write_ce) & internal_full_n;

  assign shift_reg_q = mem[shift_reg_addr];

  always @(posedge clk) begin
    if (reset) begin
      out_ptr <= ~{ADDR_WIDTH+1{1'b0}};
      internal_empty_n <= 1'b0;
      internal_full_n <= 1'b1;
    end else begin
      if (((if_read && if_read_ce) && internal_empty_n) &&
          (!(if_write && if_write_ce) || !internal_full_n)) begin
        out_ptr <= out_ptr - 1'b1;
        if (out_ptr == {(ADDR_WIDTH+1){1'b0}})
          internal_empty_n <= 1'b0;
        internal_full_n <= 1'b1;
      end
      else if (((if_read & if_read_ce) == 0 | internal_empty_n == 0) &&
        ((if_write & if_write_ce) == 1 & internal_full_n == 1))
      begin
        out_ptr <= out_ptr + 1'b1;
        internal_empty_n <= 1'b1;
        if (out_ptr == DEPTH - {{(ADDR_WIDTH-1){1'b0}}, 2'd2})
          internal_full_n <= 1'b0;
      end
    end
  end

  integer i;
  always @(posedge clk) begin
    if (shift_reg_ce) begin
      for (i = 0; i < DEPTH - 1; i = i + 1)
        mem[i + 1] <= mem[i];
      mem[0] <= shift_reg_data;
    end
  end

endmodule  // fifo_srl

`default_nettype wire
"""
