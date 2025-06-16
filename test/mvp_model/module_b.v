module module_b (
    input wire clk,
    input wire in_b,
    output wire out_b
);
    wire intermediate;

    core_logic #(.WIDTH(16)) u_core (
        .clk(clk),
        .data_in(in_b),
        .data_out(intermediate)
    );

    assign out_b = intermediate;
endmodule
