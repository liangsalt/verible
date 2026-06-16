// good.sv -- a shallow expression, comb_depth = 1.
// One kBinaryExpression node only; below threshold (3).
module m;
  wire [7:0] a, b;
  wire [7:0] result;
  assign result = a + b;
endmodule
