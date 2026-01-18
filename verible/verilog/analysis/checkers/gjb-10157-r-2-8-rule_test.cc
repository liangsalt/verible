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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-8-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/token-stream-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

// Tests that allowed data types pass.
TEST(Gjb10157R28RuleTest, AllowedTypesPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Allowed types
      {"module test;\n"
       "  reg a;\n"
       "  wire b;\n"
       "  integer c;\n"
       "  tri d;\n"
       "  parameter e = 1;\n"
       "endmodule\n"},
      // Empty file
      {""},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R28Rule>(kTestCases, "test.sv");
}

// Tests that forbidden data types are rejected.
TEST(Gjb10157R28RuleTest, ForbiddenTypesFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // logic type
      {"module test;\n"
       "  ", {TK_logic, "logic"}, " a;\n"
       "endmodule\n"},
      // bit type
      {"module test;\n"
       "  ", {TK_bit, "bit"}, " b;\n"
       "endmodule\n"},
      // int type
      {"module test;\n"
       "  ", {TK_int, "int"}, " c;\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R28Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
