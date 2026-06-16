// bad.sv -- canonical FSM with case + non-blocking on `state` register.
// `state` is bound as an FSM state register; references to it fire.
module m(input clk, input go, output reg done);
  reg [1:0] state;

  always_ff @(posedge clk) begin
    case (state)
      2'd0: state <= go ? 2'd1 : 2'd0;
      2'd1: state <= 2'd2;
      2'd2: state <= 2'd0;
      default: state <= 2'd0;
    endcase
  end

  always_comb begin
    done = (state == 2'd2);
  end
endmodule
