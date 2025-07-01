
import pkg2::f2;
module mod_b(input logic [3:0] b, output logic [3:0] y);
    assign y = f2(b);
endmodule
