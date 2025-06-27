
module top_module(input logic [config_pkg::DATA_WIDTH:0] in, output logic [config_pkg::DATA_WIDTH:0] out);
    logic [3:0] a_out, b_out, c_out;

    mod_a u1 (.a(in), .y(a_out));
    mod_b u2 (.b(a_out), .y(b_out));
    mod_c u3 (.c(b_out), .y(c_out));
    mod_d u4 (.d(c_out), .y(d_out));

    assign out = d_out;
endmodule
