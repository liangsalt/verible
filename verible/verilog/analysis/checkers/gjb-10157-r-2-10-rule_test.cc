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
#include <string>

#include "absl/flags/flag.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/text-structure-linter-test-utils.h"
#include "verible/verilog/analysis/top-modules-flag.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

// RAII helper to set and restore FLAGS_top_modules for testing.
class TopModulesFlagSetter {
 public:
  explicit TopModulesFlagSetter(const std::string& value)
      : original_value_(absl::GetFlag(FLAGS_top_modules)) {
    absl::SetFlag(&FLAGS_top_modules, value);
  }
  ~TopModulesFlagSetter() {
    absl::SetFlag(&FLAGS_top_modules, original_value_);
  }
 private:
  std::string original_value_;
};

// Tests that when no top_modules specified, no violations are reported.
// This is because the rule only applies to top-level modules.
TEST(Gjb10157R210RuleTest, NoTopModulesSpecifiedSkipsCheck) {
  TopModulesFlagSetter setter("");  // No top modules specified
  const std::initializer_list<LintTestCase> kTestCases = {
      // Even with unused input, no violation because not a top module
      {"module sub(input clk, input unused, output y);\n"
       "  assign y = clk;\n"
       "endmodule\n"},
      // Multiple unused inputs in non-top module
      {"module child(input a, input b, input c, output y);\n"
       "  assign y = a;\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R210Rule>(kTestCases, "test.sv");
}

// Tests that sub-modules (not in top_modules list) are not checked.
// This is the key engineering reason: only top-level modules have
// floating inputs that correspond to physical pins.
TEST(Gjb10157R210RuleTest, SubModulesNotChecked) {
  TopModulesFlagSetter setter("top_module");  // Only check "top_module"
  const std::initializer_list<LintTestCase> kTestCases = {
      // child module has unused input 'b', but it's not a top module
      // so no violation should be reported
      {"module child(input a, input b, output y);\n"
       "  assign y = a;\n"
       "endmodule\n"},
      // fifo has unused 'flush', but it's not a top module
      {"module fifo(input clk, input rst, input flush, output empty);\n"
       "  assign empty = clk & rst;\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R210Rule>(kTestCases, "test.sv");
}

// Tests that top-level modules with all inputs used passes.
TEST(Gjb10157R210RuleTest, TopModuleAllInputsUsedPasses) {
  TopModulesFlagSetter setter("adder,buf_mod");
  const std::initializer_list<LintTestCase> kTestCases = {
      // All inputs used in top module
      {"module adder(input [3:0] a, input [3:0] b, output [4:0] sum);\n"
       "  assign sum = a + b;\n"
       "endmodule\n"},
      // Single input used in top module
      {"module buf_mod(input clk, output reg out);\n"
       "  always @(posedge clk) out <= 1;\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R210Rule>(kTestCases, "test.sv");
}

// Tests that top-level modules with unused inputs report violations.
TEST(Gjb10157R210RuleTest, TopModuleUnusedInputReportsViolation) {
  TopModulesFlagSetter setter("top");
  const std::initializer_list<LintTestCase> kTestCases = {
      // 'unused' is not used in top module - violation
      {"module top(input clk, input ",
       {SymbolIdentifier, "unused"},
       ", output y);\n"
       "  assign y = clk;\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R210Rule>(kTestCases, "test.sv");
}

// Tests non-ANSI style port declarations in top module.
TEST(Gjb10157R210RuleTest, TopModuleNonAnsiStyleUnusedInput) {
  TopModulesFlagSetter setter("top");
  const std::initializer_list<LintTestCase> kTestCases = {
      // Non-ANSI style with unused input in top module
      {"module top(clk, unused, y);\n"
       "  input clk;\n"
       "  input ",
       {SymbolIdentifier, "unused"},
       ";\n"
       "  output y;\n"
       "  assign y = clk;\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R210Rule>(kTestCases, "test.sv");
}

// Tests multiple top modules specified.
TEST(Gjb10157R210RuleTest, MultipleTopModules) {
  TopModulesFlagSetter setter("top1,top2");
  const std::initializer_list<LintTestCase> kTestCases = {
      // top1 has unused input - violation
      {"module top1(input clk, input ",
       {SymbolIdentifier, "unused"},
       ", output y);\n"
       "  assign y = clk;\n"
       "endmodule\n"},
      // top2 has all inputs used - no violation
      {"module top2(input a, input b, output y);\n"
       "  assign y = a & b;\n"
       "endmodule\n"},
      // child is not in top_modules, so no check
      {"module child(input x, input unused, output z);\n"
       "  assign z = x;\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R210Rule>(kTestCases, "test.sv");
}

// Tests that top module with multiple unused inputs reports all violations.
TEST(Gjb10157R210RuleTest, TopModuleMultipleUnusedInputs) {
  TopModulesFlagSetter setter("top");
  const std::initializer_list<LintTestCase> kTestCases = {
      // Multiple unused inputs in top module
      {"module top(input clk, input ",
       {SymbolIdentifier, "unused1"},
       ", input ",
       {SymbolIdentifier, "unused2"},
       ", output y);\n"
       "  assign y = clk;\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R210Rule>(kTestCases, "test.sv");
}

// Tests that top module with no body (empty) reports all inputs as unused.
TEST(Gjb10157R210RuleTest, TopModuleEmptyBodyAllUnused) {
  TopModulesFlagSetter setter("top");
  const std::initializer_list<LintTestCase> kTestCases = {
      // Empty module body - all inputs unused
      {"module top(input ",
       {SymbolIdentifier, "clk"},
       ", input ",
       {SymbolIdentifier, "rst"},
       ", output y);\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R210Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
