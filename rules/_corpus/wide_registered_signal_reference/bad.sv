// bad.sv -- a 16-bit registered signal referenced in combinational logic.
// Both conditions hold:
//   bit_width_ge(state, 8)   -> true (state is 16-bit)
//   is_registered(state)     -> true (state <= next_state in always_ff)
module m(input clk, input [15:0] data_in, output reg out);
  reg [15:0] state;

  always_ff @(posedge clk) begin
    state <= data_in;
  end

  // Reference to the wide, registered signal in combinational logic.
  always_comb begin
    out = (state[7:0] != 8'h00);
  end
endmodule
