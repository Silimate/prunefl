module top_module (
    input wire clk,
    input wire data_in,
    output wire data_out
);
    module_a u_mod_a (
        .clk(clk),
        .in_a(data_in),
        .out_a(data_out)
    );
endmodule
