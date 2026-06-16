module m;
  reg [1:0] sel;
  reg out;
  always @* begin
    out = (sel == 2'd1);
  end
endmodule
