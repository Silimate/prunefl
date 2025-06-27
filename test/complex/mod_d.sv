import pkg3::*;

module mod_d(input logic [3:0] d, output logic [3:0] y);
    initial begin
        DerivedClass obj = new();
        y = obj.compute(d);
    end
endmodule
