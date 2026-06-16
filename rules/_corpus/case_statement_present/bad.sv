module m;
  reg [1:0] sel;
  reg out;
  always @* begin
    case (sel)
      2'd0: out = 1'b0;
      2'd1: out = 1'b1;
      default: out = 1'b0;
    endcase
  end
endmodule
