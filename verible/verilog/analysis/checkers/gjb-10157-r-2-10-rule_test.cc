// Copyright 2024 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-10-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/text-structure-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

// Tests that all inputs used passes.
TEST(Gjb10157R210RuleTest, AllInputsUsedPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // All inputs used
      {"module adder(input [3:0] a, input [3:0] b, output [4:0] sum);\n"
       "  assign sum = a + b;\n"
       "endmodule\n"},
      // Single input used
      {"module buf_mod(input clk, output reg out);\n"
       "  always @(posedge clk) out <= 1;\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R210Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
