module m;
  reg [7:0] wide_reg;
  reg [15:0] very_wide_reg;
  wire scalar;
  always @* begin
    very_wide_reg = wide_reg + 16'd1;
  end
endmodule
