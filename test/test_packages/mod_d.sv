import pkg3::*;

module mod_d(input logic [3:0] d, output logic [3:0] y);
    DerivedClass obj = new();
    always @ *
        y = obj.compute(d);
endmodule
