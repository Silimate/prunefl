module core_logic #(parameter WIDTH = `DEFAULT_WIDTH) (
    input wire clk,
    input wire data_in,
    output wire data_out
);
    wire temp;
    `CONNECT(temp, data_in);

    utils #(.DEPTH(WIDTH)) u_utils (
        .in(temp),
        .out(data_out)
    );

    `UNUSED_WIRE(unused_signal);
endmodule
