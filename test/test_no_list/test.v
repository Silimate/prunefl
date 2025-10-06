module top_module
#(
    parameter DEC_WIDTH = 8,
    parameter ENC_WIDTH = 3
)
(
    input  wire [ENC_WIDTH-1:0] in_enc_nnn,
    output wire [DEC_WIDTH-1:0] out_dec_nnn
);
  assign out_dec_nnn = DEC_WIDTH'(1'b1 << in_enc_nnn);
endmodule
