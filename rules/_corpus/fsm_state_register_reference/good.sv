// good.sv -- a simple counter, no case-on-self FSM pattern.
// `count` is registered but never used as a case expression in the same
// always_ff block, so is_fsm_state_register returns false.
module m(input clk, input rst, output reg [7:0] count);
  always_ff @(posedge clk) begin
    if (rst) count <= 8'd0;
    else count <= count + 8'd1;
  end
endmodule
