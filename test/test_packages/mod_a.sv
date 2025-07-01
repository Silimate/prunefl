
`include "defs.sv"
import pkg1::*;
module mod_a(input logic [3:0] a, output logic [3:0] y);
    assign y = `ADD(a, `DW_MACRO);
endmodule
