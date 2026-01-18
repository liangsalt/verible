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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-5-rule.h"

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

constexpr int kSymbol = SymbolIdentifier;

// Tests that single module files pass.
TEST(Gjb10157R25RuleTest, SingleModulePasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Single module
      {"module adder; endmodule"},
      {"module top;\n"
       "  reg a;\n"
       "endmodule\n"},
      // Empty file
      {""},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R25Rule>(kTestCases, "test.sv");
}

// Tests that multiple modules in one file are rejected.
TEST(Gjb10157R25RuleTest, MultipleModulesFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Two modules
      {"module adder; endmodule\n"
       "module ", {kSymbol, "full_adder"}, "; endmodule\n"},
      // Three modules
      {"module m1; endmodule\n"
       "module ", {kSymbol, "m2"}, "; endmodule\n"
       "module ", {kSymbol, "m3"}, "; endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R25Rule>(kTestCases, "test.sv");
}

// Tests that nested modules are allowed.
TEST(Gjb10157R25RuleTest, NestedModulesAllowed) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Nested module (SystemVerilog)
      {"module outer;\n"
       "  module inner;\n"
       "  endmodule\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R25Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
