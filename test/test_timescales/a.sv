module top_module(input logic [3:0] a, output logic [3:0] y);
  wire[3:0] y_int;
  mod_b internal(.b(a), .y(y_int));
  assign y = ~y_int;
endmodule
