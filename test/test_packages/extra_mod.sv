module extra_mod(input logic clk, output logic done);
    always_ff @(posedge clk)
        done <= 1'b1;
endmodule
