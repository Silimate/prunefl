module module_a (
    input wire clk,
    input wire in_a,
    output wire out_a
);
    wire processed_signal;

    `MODULE_B_NAME u_mod_b (
        .clk(clk),
        .in_b(in_a),
        .out_b(processed_signal)
    );

    assign out_a = processed_signal;
endmodule
