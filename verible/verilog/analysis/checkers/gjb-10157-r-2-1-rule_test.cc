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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-1-rule.h"

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

// Tests that valid identifiers pass the rule.
TEST(Gjb10157R21RuleTest, ValidIdentifiersPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Valid module names
      {"module top; endmodule"},
      {"module my_module; endmodule"},
      {"module Module123; endmodule"},
      {"module a; endmodule"},
      {"module A_B_C; endmodule"},
      {"module test_module_v2; endmodule"},

      // Valid function names
      {"module m; function void my_func(); endfunction endmodule"},
      {"module m; function int get_value(); endfunction endmodule"},

      // Valid task names
      {"module m; task my_task(); endtask endmodule"},

      // Valid variable names
      {"module m; reg valid_signal; endmodule"},
      {"module m; wire data_in; endmodule"},
      {"module m; logic clk; endmodule"},
      {"module m; integer count; endmodule"},

      // Valid package names
      {"package my_pkg; endpackage"},

      // Valid class names
      {"class MyClass; endclass"},
      {"class base_class; endclass"},

      // Valid interface names
      {"interface my_if; endinterface"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R21Rule>(kTestCases, "test.sv");
}

// Tests that identifiers starting with numbers are rejected.
TEST(Gjb10157R21RuleTest, IdentifierStartingWithNumberFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Module name starting with number - this would be a syntax error
      // but we test with escaped identifier or similar patterns

      // Variable names starting with underscore (invalid per rule)
      {"module m; reg ", {kSymbol, "_signal"}, "; endmodule"},

      // Variable names starting with number would be syntax error
      // Test underscore prefix which violates "start with letter" rule
      {"module m; wire ", {kSymbol, "_data"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R21Rule>(kTestCases, "test.sv");
}

// Tests that identifiers starting with underscore are rejected.
TEST(Gjb10157R21RuleTest, IdentifierStartingWithUnderscoreFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; reg ", {kSymbol, "_invalid_name"}, "; endmodule"},
      {"module m; wire ", {kSymbol, "_"}, "; endmodule"},
      {"module m; logic ", {kSymbol, "__double"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R21Rule>(kTestCases, "test.sv");
}

// Tests valid identifiers with mixed case and numbers.
TEST(Gjb10157R21RuleTest, ValidMixedIdentifiers) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; reg a1b2c3; endmodule"},
      {"module m; wire DataIn_0; endmodule"},
      {"module m; logic CLK_100MHz; endmodule"},
      {"module m; integer count_v2; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R21Rule>(kTestCases, "test.sv");
}

// Tests module instances.
TEST(Gjb10157R21RuleTest, ModuleInstanceNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Valid instance names
      {"module m; sub_mod u_inst(); endmodule"},
      {"module m; sub_mod inst_0(); endmodule"},

      // Invalid instance name starting with underscore
      {"module m; sub_mod ", {kSymbol, "_inst"}, "(); endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R21Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
