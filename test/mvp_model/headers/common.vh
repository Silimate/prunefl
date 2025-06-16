`define DEFAULT_WIDTH 8
`define CONNECT(a, b) assign a = b
`define LITERALLY_USELESS `UNUSED_WIRE(useless)
module test;
    `LITERALLY_USELESS
endmodule
