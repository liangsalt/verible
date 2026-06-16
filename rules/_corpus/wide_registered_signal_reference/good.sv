// good.sv -- only narrow registered signals.
// Conditions fail because bit_width_ge(flag, 8) is false (flag is 1-bit).
module m(input clk, input in_bit, output reg out);
  reg flag;

  always_ff @(posedge clk) begin
    flag <= in_bit;
  end

  always_comb begin
    out = flag;
  end
endmodule
