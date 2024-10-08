"""Generates an AXI crossbar wrapper with the specified number of ports."""

# ruff: noqa: E501

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from jinja2 import Template


def generate(
    ports: int | list[int] | tuple[int, int] = 4, name: str | None = None
) -> str:
    """Generates an AXI crossbar wrapper with the specified number of ports."""
    if type(ports) is int:
        m = n = ports
    elif isinstance(ports, tuple | list) and len(ports) == 1:
        m = n = ports[0]
    elif isinstance(ports, tuple | list) and len(ports) == 2:  # noqa: PLR2004
        m, n = ports
    else:
        msg = "Invalid number of ports"
        raise ValueError(msg)

    if name is None:
        name = f"axi_crossbar_wrap_{m}x{n}"

    cm = (m - 1).bit_length()
    cn = (n - 1).bit_length()

    t = Template(
        """/*

Copyright (c) 2020 Alex Forencich

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
 * AXI4 {{m}}x{{n}} crossbar (wrapper)
 */
module {{name}} #
(
    // Width of data bus in bits
    parameter DATA_WIDTH = 32,
    // Width of address bus in bits
    parameter ADDR_WIDTH = 32,
    // Width of wstrb (width of data bus in words)
    parameter STRB_WIDTH = (DATA_WIDTH/8),
    // Input ID field width (from AXI masters)
    parameter S_ID_WIDTH = 8,
    // Output ID field width (towards AXI slaves)
    // Additional bits required for response routing
    parameter M_ID_WIDTH = S_ID_WIDTH+$clog2(S_COUNT),
    // Propagate awuser signal
    parameter AWUSER_ENABLE = 0,
    // Width of awuser signal
    parameter AWUSER_WIDTH = 1,
    // Propagate wuser signal
    parameter WUSER_ENABLE = 0,
    // Width of wuser signal
    parameter WUSER_WIDTH = 1,
    // Propagate buser signal
    parameter BUSER_ENABLE = 0,
    // Width of buser signal
    parameter BUSER_WIDTH = 1,
    // Propagate aruser signal
    parameter ARUSER_ENABLE = 0,
    // Width of aruser signal
    parameter ARUSER_WIDTH = 1,
    // Propagate ruser signal
    parameter RUSER_ENABLE = 0,
    // Width of ruser signal
    parameter RUSER_WIDTH = 1,
{%- for p in range(m) %}
    // Number of concurrent unique IDs
    parameter S{{'%02d'%p}}_THREADS = 2,
    // Number of concurrent operations
    parameter S{{'%02d'%p}}_ACCEPT = 16,
{%- endfor %}
    // Number of regions per master interface
    parameter M_REGIONS = 1,
{%- for p in range(n) %}
    // Master interface base addresses
    // M_REGIONS concatenated fields of ADDR_WIDTH bits
    parameter M{{'%02d'%p}}_BASE_ADDR = 0,
    // Master interface address widths
    // M_REGIONS concatenated fields of 32 bits
    parameter M{{'%02d'%p}}_ADDR_WIDTH = {M_REGIONS{32'd24}},
    // Read connections between interfaces
    // S_COUNT bits
    parameter M{{'%02d'%p}}_CONNECT_READ = {{m}}'b{% for p in range(m) %}1{% endfor %},
    // Write connections between interfaces
    // S_COUNT bits
    parameter M{{'%02d'%p}}_CONNECT_WRITE = {{m}}'b{% for p in range(m) %}1{% endfor %},
    // Number of concurrent operations for each master interface
    parameter M{{'%02d'%p}}_ISSUE = 4,
    // Secure master (fail operations based on awprot/arprot)
    parameter M{{'%02d'%p}}_SECURE = 0,
{%- endfor %}
{%- for p in range(m) %}
    // Slave interface AW channel register type (input)
    // 0 to bypass, 1 for simple buffer, 2 for skid buffer
    parameter S{{'%02d'%p}}_AW_REG_TYPE = 0,
    // Slave interface W channel register type (input)
    // 0 to bypass, 1 for simple buffer, 2 for skid buffer
    parameter S{{'%02d'%p}}_W_REG_TYPE = 0,
    // Slave interface B channel register type (output)
    // 0 to bypass, 1 for simple buffer, 2 for skid buffer
    parameter S{{'%02d'%p}}_B_REG_TYPE = 1,
    // Slave interface AR channel register type (input)
    // 0 to bypass, 1 for simple buffer, 2 for skid buffer
    parameter S{{'%02d'%p}}_AR_REG_TYPE = 0,
    // Slave interface R channel register type (output)
    // 0 to bypass, 1 for simple buffer, 2 for skid buffer
    parameter S{{'%02d'%p}}_R_REG_TYPE = 2,
{%- endfor %}
{%- for p in range(n) %}
    // Master interface AW channel register type (output)
    // 0 to bypass, 1 for simple buffer, 2 for skid buffer
    parameter M{{'%02d'%p}}_AW_REG_TYPE = 1,
    // Master interface W channel register type (output)
    // 0 to bypass, 1 for simple buffer, 2 for skid buffer
    parameter M{{'%02d'%p}}_W_REG_TYPE = 2,
    // Master interface B channel register type (input)
    // 0 to bypass, 1 for simple buffer, 2 for skid buffer
    parameter M{{'%02d'%p}}_B_REG_TYPE = 0,
    // Master interface AR channel register type (output)
    // 0 to bypass, 1 for simple buffer, 2 for skid buffer
    parameter M{{'%02d'%p}}_AR_REG_TYPE = 1,
    // Master interface R channel register type (input)
    // 0 to bypass, 1 for simple buffer, 2 for skid buffer
    parameter M{{'%02d'%p}}_R_REG_TYPE = 0{% if not loop.last %},{% endif %}
{%- endfor %}
)
(
    input  wire                     clk,
    input  wire                     rst,

    /*
     * AXI slave interface
     */
{%- for p in range(m) %}
    input  wire [S_ID_WIDTH-1:0]    s{{'%02d'%p}}_axi_awid,
    input  wire [ADDR_WIDTH-1:0]    s{{'%02d'%p}}_axi_awaddr,
    input  wire [7:0]               s{{'%02d'%p}}_axi_awlen,
    input  wire [2:0]               s{{'%02d'%p}}_axi_awsize,
    input  wire [1:0]               s{{'%02d'%p}}_axi_awburst,
    input  wire                     s{{'%02d'%p}}_axi_awlock,
    input  wire [3:0]               s{{'%02d'%p}}_axi_awcache,
    input  wire [2:0]               s{{'%02d'%p}}_axi_awprot,
    input  wire [3:0]               s{{'%02d'%p}}_axi_awqos,
    input  wire [AWUSER_WIDTH-1:0]  s{{'%02d'%p}}_axi_awuser,
    input  wire                     s{{'%02d'%p}}_axi_awvalid,
    output wire                     s{{'%02d'%p}}_axi_awready,
    input  wire [DATA_WIDTH-1:0]    s{{'%02d'%p}}_axi_wdata,
    input  wire [STRB_WIDTH-1:0]    s{{'%02d'%p}}_axi_wstrb,
    input  wire                     s{{'%02d'%p}}_axi_wlast,
    input  wire [WUSER_WIDTH-1:0]   s{{'%02d'%p}}_axi_wuser,
    input  wire                     s{{'%02d'%p}}_axi_wvalid,
    output wire                     s{{'%02d'%p}}_axi_wready,
    output wire [S_ID_WIDTH-1:0]    s{{'%02d'%p}}_axi_bid,
    output wire [1:0]               s{{'%02d'%p}}_axi_bresp,
    output wire [BUSER_WIDTH-1:0]   s{{'%02d'%p}}_axi_buser,
    output wire                     s{{'%02d'%p}}_axi_bvalid,
    input  wire                     s{{'%02d'%p}}_axi_bready,
    input  wire [S_ID_WIDTH-1:0]    s{{'%02d'%p}}_axi_arid,
    input  wire [ADDR_WIDTH-1:0]    s{{'%02d'%p}}_axi_araddr,
    input  wire [7:0]               s{{'%02d'%p}}_axi_arlen,
    input  wire [2:0]               s{{'%02d'%p}}_axi_arsize,
    input  wire [1:0]               s{{'%02d'%p}}_axi_arburst,
    input  wire                     s{{'%02d'%p}}_axi_arlock,
    input  wire [3:0]               s{{'%02d'%p}}_axi_arcache,
    input  wire [2:0]               s{{'%02d'%p}}_axi_arprot,
    input  wire [3:0]               s{{'%02d'%p}}_axi_arqos,
    input  wire [ARUSER_WIDTH-1:0]  s{{'%02d'%p}}_axi_aruser,
    input  wire                     s{{'%02d'%p}}_axi_arvalid,
    output wire                     s{{'%02d'%p}}_axi_arready,
    output wire [S_ID_WIDTH-1:0]    s{{'%02d'%p}}_axi_rid,
    output wire [DATA_WIDTH-1:0]    s{{'%02d'%p}}_axi_rdata,
    output wire [1:0]               s{{'%02d'%p}}_axi_rresp,
    output wire                     s{{'%02d'%p}}_axi_rlast,
    output wire [RUSER_WIDTH-1:0]   s{{'%02d'%p}}_axi_ruser,
    output wire                     s{{'%02d'%p}}_axi_rvalid,
    input  wire                     s{{'%02d'%p}}_axi_rready,
{% endfor %}
    /*
     * AXI master interface
     */
{%- for p in range(n) %}
    output wire [M_ID_WIDTH-1:0]    m{{'%02d'%p}}_axi_awid,
    output wire [ADDR_WIDTH-1:0]    m{{'%02d'%p}}_axi_awaddr,
    output wire [7:0]               m{{'%02d'%p}}_axi_awlen,
    output wire [2:0]               m{{'%02d'%p}}_axi_awsize,
    output wire [1:0]               m{{'%02d'%p}}_axi_awburst,
    output wire                     m{{'%02d'%p}}_axi_awlock,
    output wire [3:0]               m{{'%02d'%p}}_axi_awcache,
    output wire [2:0]               m{{'%02d'%p}}_axi_awprot,
    output wire [3:0]               m{{'%02d'%p}}_axi_awqos,
    output wire [3:0]               m{{'%02d'%p}}_axi_awregion,
    output wire [AWUSER_WIDTH-1:0]  m{{'%02d'%p}}_axi_awuser,
    output wire                     m{{'%02d'%p}}_axi_awvalid,
    input  wire                     m{{'%02d'%p}}_axi_awready,
    output wire [DATA_WIDTH-1:0]    m{{'%02d'%p}}_axi_wdata,
    output wire [STRB_WIDTH-1:0]    m{{'%02d'%p}}_axi_wstrb,
    output wire                     m{{'%02d'%p}}_axi_wlast,
    output wire [WUSER_WIDTH-1:0]   m{{'%02d'%p}}_axi_wuser,
    output wire                     m{{'%02d'%p}}_axi_wvalid,
    input  wire                     m{{'%02d'%p}}_axi_wready,
    input  wire [M_ID_WIDTH-1:0]    m{{'%02d'%p}}_axi_bid,
    input  wire [1:0]               m{{'%02d'%p}}_axi_bresp,
    input  wire [BUSER_WIDTH-1:0]   m{{'%02d'%p}}_axi_buser,
    input  wire                     m{{'%02d'%p}}_axi_bvalid,
    output wire                     m{{'%02d'%p}}_axi_bready,
    output wire [M_ID_WIDTH-1:0]    m{{'%02d'%p}}_axi_arid,
    output wire [ADDR_WIDTH-1:0]    m{{'%02d'%p}}_axi_araddr,
    output wire [7:0]               m{{'%02d'%p}}_axi_arlen,
    output wire [2:0]               m{{'%02d'%p}}_axi_arsize,
    output wire [1:0]               m{{'%02d'%p}}_axi_arburst,
    output wire                     m{{'%02d'%p}}_axi_arlock,
    output wire [3:0]               m{{'%02d'%p}}_axi_arcache,
    output wire [2:0]               m{{'%02d'%p}}_axi_arprot,
    output wire [3:0]               m{{'%02d'%p}}_axi_arqos,
    output wire [3:0]               m{{'%02d'%p}}_axi_arregion,
    output wire [ARUSER_WIDTH-1:0]  m{{'%02d'%p}}_axi_aruser,
    output wire                     m{{'%02d'%p}}_axi_arvalid,
    input  wire                     m{{'%02d'%p}}_axi_arready,
    input  wire [M_ID_WIDTH-1:0]    m{{'%02d'%p}}_axi_rid,
    input  wire [DATA_WIDTH-1:0]    m{{'%02d'%p}}_axi_rdata,
    input  wire [1:0]               m{{'%02d'%p}}_axi_rresp,
    input  wire                     m{{'%02d'%p}}_axi_rlast,
    input  wire [RUSER_WIDTH-1:0]   m{{'%02d'%p}}_axi_ruser,
    input  wire                     m{{'%02d'%p}}_axi_rvalid,
    output wire                     m{{'%02d'%p}}_axi_rready{% if not loop.last %},{% endif %}
{% endfor -%}
);

localparam S_COUNT = {{m}};
localparam M_COUNT = {{n}};

// parameter sizing helpers
function [ADDR_WIDTH*M_REGIONS-1:0] w_a_r(input [ADDR_WIDTH*M_REGIONS-1:0] val);
    w_a_r = val;
endfunction

function [32*M_REGIONS-1:0] w_32_r(input [32*M_REGIONS-1:0] val);
    w_32_r = val;
endfunction

function [S_COUNT-1:0] w_s(input [S_COUNT-1:0] val);
    w_s = val;
endfunction

function [31:0] w_32(input [31:0] val);
    w_32 = val;
endfunction

function [1:0] w_2(input [1:0] val);
    w_2 = val;
endfunction

function w_1(input val);
    w_1 = val;
endfunction

axi_crossbar #(
    .S_COUNT(S_COUNT),
    .M_COUNT(M_COUNT),
    .DATA_WIDTH(DATA_WIDTH),
    .ADDR_WIDTH(ADDR_WIDTH),
    .STRB_WIDTH(STRB_WIDTH),
    .S_ID_WIDTH(S_ID_WIDTH),
    .M_ID_WIDTH(M_ID_WIDTH),
    .AWUSER_ENABLE(AWUSER_ENABLE),
    .AWUSER_WIDTH(AWUSER_WIDTH),
    .WUSER_ENABLE(WUSER_ENABLE),
    .WUSER_WIDTH(WUSER_WIDTH),
    .BUSER_ENABLE(BUSER_ENABLE),
    .BUSER_WIDTH(BUSER_WIDTH),
    .ARUSER_ENABLE(ARUSER_ENABLE),
    .ARUSER_WIDTH(ARUSER_WIDTH),
    .RUSER_ENABLE(RUSER_ENABLE),
    .RUSER_WIDTH(RUSER_WIDTH),
    .S_THREADS({ {% for p in range(m-1,-1,-1) %}w_32(S{{'%02d'%p}}_THREADS){% if not loop.last %}, {% endif %}{% endfor %} }),
    .S_ACCEPT({ {% for p in range(m-1,-1,-1) %}w_32(S{{'%02d'%p}}_ACCEPT){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_REGIONS(M_REGIONS),
    .M_BASE_ADDR({ {% for p in range(n-1,-1,-1) %}w_a_r(M{{'%02d'%p}}_BASE_ADDR){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_ADDR_WIDTH({ {% for p in range(n-1,-1,-1) %}w_32_r(M{{'%02d'%p}}_ADDR_WIDTH){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_CONNECT_READ({ {% for p in range(n-1,-1,-1) %}w_s(M{{'%02d'%p}}_CONNECT_READ){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_CONNECT_WRITE({ {% for p in range(n-1,-1,-1) %}w_s(M{{'%02d'%p}}_CONNECT_WRITE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_ISSUE({ {% for p in range(n-1,-1,-1) %}w_32(M{{'%02d'%p}}_ISSUE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_SECURE({ {% for p in range(n-1,-1,-1) %}w_1(M{{'%02d'%p}}_SECURE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .S_AR_REG_TYPE({ {% for p in range(m-1,-1,-1) %}w_2(S{{'%02d'%p}}_AR_REG_TYPE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .S_R_REG_TYPE({ {% for p in range(m-1,-1,-1) %}w_2(S{{'%02d'%p}}_R_REG_TYPE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .S_AW_REG_TYPE({ {% for p in range(m-1,-1,-1) %}w_2(S{{'%02d'%p}}_AW_REG_TYPE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .S_W_REG_TYPE({ {% for p in range(m-1,-1,-1) %}w_2(S{{'%02d'%p}}_W_REG_TYPE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .S_B_REG_TYPE({ {% for p in range(m-1,-1,-1) %}w_2(S{{'%02d'%p}}_B_REG_TYPE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_AR_REG_TYPE({ {% for p in range(n-1,-1,-1) %}w_2(M{{'%02d'%p}}_AR_REG_TYPE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_R_REG_TYPE({ {% for p in range(n-1,-1,-1) %}w_2(M{{'%02d'%p}}_R_REG_TYPE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_AW_REG_TYPE({ {% for p in range(n-1,-1,-1) %}w_2(M{{'%02d'%p}}_AW_REG_TYPE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_W_REG_TYPE({ {% for p in range(n-1,-1,-1) %}w_2(M{{'%02d'%p}}_W_REG_TYPE){% if not loop.last %}, {% endif %}{% endfor %} }),
    .M_B_REG_TYPE({ {% for p in range(n-1,-1,-1) %}w_2(M{{'%02d'%p}}_B_REG_TYPE){% if not loop.last %}, {% endif %}{% endfor %} })
)
axi_crossbar_inst (
    .clk(clk),
    .rst(rst),
    .s_axi_awid({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awaddr({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awaddr{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awlen({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awlen{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awsize({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awsize{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awburst({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awburst{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awlock({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awlock{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awcache({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awcache{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awprot({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awprot{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awqos({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awqos{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awuser({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awuser{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awvalid({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awvalid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_awready({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_awready{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_wdata({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_wdata{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_wstrb({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_wstrb{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_wlast({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_wlast{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_wuser({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_wuser{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_wvalid({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_wvalid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_wready({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_wready{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_bid({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_bid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_bresp({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_bresp{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_buser({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_buser{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_bvalid({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_bvalid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_bready({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_bready{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_arid({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_arid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_araddr({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_araddr{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_arlen({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_arlen{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_arsize({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_arsize{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_arburst({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_arburst{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_arlock({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_arlock{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_arcache({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_arcache{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_arprot({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_arprot{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_arqos({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_arqos{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_aruser({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_aruser{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_arvalid({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_arvalid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_arready({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_arready{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_rid({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_rid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_rdata({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_rdata{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_rresp({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_rresp{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_rlast({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_rlast{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_ruser({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_ruser{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_rvalid({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_rvalid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .s_axi_rready({ {% for p in range(m-1,-1,-1) %}s{{'%02d'%p}}_axi_rready{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awid({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awaddr({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awaddr{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awlen({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awlen{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awsize({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awsize{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awburst({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awburst{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awlock({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awlock{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awcache({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awcache{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awprot({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awprot{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awqos({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awqos{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awregion({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awregion{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awuser({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awuser{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awvalid({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awvalid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_awready({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_awready{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_wdata({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_wdata{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_wstrb({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_wstrb{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_wlast({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_wlast{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_wuser({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_wuser{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_wvalid({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_wvalid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_wready({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_wready{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_bid({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_bid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_bresp({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_bresp{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_buser({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_buser{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_bvalid({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_bvalid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_bready({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_bready{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arid({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_araddr({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_araddr{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arlen({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arlen{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arsize({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arsize{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arburst({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arburst{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arlock({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arlock{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arcache({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arcache{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arprot({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arprot{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arqos({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arqos{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arregion({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arregion{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_aruser({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_aruser{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arvalid({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arvalid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_arready({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_arready{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_rid({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_rid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_rdata({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_rdata{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_rresp({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_rresp{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_rlast({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_rlast{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_ruser({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_ruser{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_rvalid({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_rvalid{% if not loop.last %}, {% endif %}{% endfor %} }),
    .m_axi_rready({ {% for p in range(n-1,-1,-1) %}m{{'%02d'%p}}_axi_rready{% if not loop.last %}, {% endif %}{% endfor %} })
);

endmodule

`resetall

""",
    )

    return t.render(m=m, n=n, cm=cm, cn=cn, name=name)
