`timescale 1ns/1ps


module mod_b(input logic [3:0] b, output logic [3:0] y);
    assign y = b + 1;
endmodule
