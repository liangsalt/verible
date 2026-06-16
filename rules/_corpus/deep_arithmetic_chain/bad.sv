// bad.sv -- a 5-deep nested arithmetic chain.
// Each `+` adds one level to the binary-expression tree.
//
// kBinaryExpression
//   kBinaryExpression
//     kBinaryExpression
//       kBinaryExpression
//         kBinaryExpression (a + b)
//         + c
//         + d
//         + e
//         + f
// comb_depth = 5 -> exceeds threshold (3)
module m;
  wire [7:0] a, b, c, d, e, f;
  wire [7:0] result;
  assign result = a + b + c + d + e + f;
endmodule
