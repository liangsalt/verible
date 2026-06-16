module m;
  function automatic [7:0] add_one;
    input [7:0] x;
    add_one = x + 8'd1;
  endfunction

  wire [7:0] in_val;
  wire [7:0] out_val;
  assign out_val = add_one(in_val);
endmodule
