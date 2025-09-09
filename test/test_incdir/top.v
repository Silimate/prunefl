`include "macros.vh"

module top_module (
    input wire clk,
    input wire in_b,
    output wire out_b
);
  `MODULE_B_NAME impl (.clk, .in_b, .out_b);
endmodule
