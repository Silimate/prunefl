
`include "defs.sv"
import pkg1::*;
import pkg2::*;
import config_pkg::*;

module mod_c(input logic [3:0] c, output logic [3:0] y);
    config_pkg::state_e state;

    always_comb begin
        case (c)
            4'd0: state = IDLE;
            4'd1: state = BUSY;
            4'd2: state = DONE;
            default: state = ERROR;
        endcase
    end

    assign y = `MACRO_FUNC + f1(c) + f2(c);
    assign y = `MACRO_FUNC + f1(c) + f2(c);
endmodule
