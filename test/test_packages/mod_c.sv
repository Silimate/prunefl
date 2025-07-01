
`include "defs.sv"
import pkg1::*;
import pkg2::*;
import config_pkg::*;

module mod_c(input logic [3:0] c, output logic [3:0] y);
    `STATE_E_MACRO state;

    assign y = `MACRO_FUNC + f1(c) + f2(c);
    assign y = `MACRO_FUNC + f1(c) + f2(c);
endmodule
