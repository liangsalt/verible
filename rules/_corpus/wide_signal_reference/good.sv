module m;
  wire a, b;
  reg c;
  always @* begin
    c = a & b;
  end
endmodule
