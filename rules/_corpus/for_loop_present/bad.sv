module m;
  integer i;
  reg [7:0] x;
  initial begin
    for (i = 0; i < 8; i = i + 1) begin
      x[i] = 1'b1;
    end
  end
endmodule
