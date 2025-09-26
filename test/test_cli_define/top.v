module top_module (
    input wire clk,
    input wire in_b,
    output wire out_b
);
  module_b impl (.clk, .in_b, .out_b);
endmodule
