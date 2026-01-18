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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-9-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

constexpr int kSymbol = SymbolIdentifier;

// Tests that named port mapping passes.
TEST(Gjb10157R29RuleTest, NamedPortMappingPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Named port mapping
      {"module top;\n"
       "  submod inst1 (.in1(a), .in2(b), .out1(c));\n"
       "endmodule\n"},
      // Single named port
      {"module top;\n"
       "  submod inst1 (.clk(clock));\n"
       "endmodule\n"},
      // Empty port list
      {"module top;\n"
       "  submod inst1 ();\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R29Rule>(kTestCases, "test.sv");
}

// Tests that positional port mapping is rejected.
TEST(Gjb10157R29RuleTest, PositionalPortMappingFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Positional port mapping
      {"module top;\n"
       "  submod inst1 (", {kSymbol, "in1"}, ", in2, out1);\n"
       "endmodule\n"},
      // Single positional port
      {"module top;\n"
       "  submod inst1 (", {kSymbol, "clk"}, ");\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R29Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
