module wrapper_module(input logic clk, input logic [3:0] in, output logic done, output logic [3:0] out);
    logic [3:0] top_out;

    top_module u_top (
        .in(in),
        .out(top_out)
    );

    extra_mod u_extra (
        .clk(clk),
        .done(done)
    );

    assign out = top_out;
endmodule
