
module top_module();
  Misc misc;
  initial begin
    misc = new();
    misc.sayHi("top module");
  end
endmodule
