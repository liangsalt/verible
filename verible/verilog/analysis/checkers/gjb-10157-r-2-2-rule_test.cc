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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-2-rule.h"

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
TEST(Gjb10157R22RuleTest, ValidIdentifiersPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Valid variable names (no consecutive underscores, no trailing underscore)
      {"module m; reg data_flag; endmodule"},
      {"module m; reg w_slot_a; endmodule"},
      {"module m; wire valid_signal; endmodule"},
      {"module m; logic clk_100mhz; endmodule"},
      {"module m; integer count_v2; endmodule"},

      // Valid module names
      {"module top_module; endmodule"},
      {"module my_design; endmodule"},

      // Valid function names
      {"module m; function void get_data(); endfunction endmodule"},

      // Valid task names
      {"module m; task run_test(); endtask endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R22Rule>(kTestCases, "test.sv");
}

// Tests that identifiers with consecutive underscores are rejected.
TEST(Gjb10157R22RuleTest, ConsecutiveUnderscoresFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Variable with consecutive underscores
      {"module m; reg ", {kSymbol, "data__flag"}, "; endmodule"},
      {"module m; wire ", {kSymbol, "signal__name"}, "; endmodule"},
      {"module m; logic ", {kSymbol, "clk___fast"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R22Rule>(kTestCases, "test.sv");
}

// Tests that identifiers ending with underscore are rejected.
TEST(Gjb10157R22RuleTest, TrailingUnderscoreFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Variable ending with underscore
      {"module m; reg ", {kSymbol, "w_slot_a_"}, "; endmodule"},
      {"module m; wire ", {kSymbol, "data_"}, "; endmodule"},
      {"module m; logic ", {kSymbol, "signal_"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R22Rule>(kTestCases, "test.sv");
}

// Tests module instances.
TEST(Gjb10157R22RuleTest, ModuleInstanceNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Valid instance names
      {"module m; sub_mod u_inst(); endmodule"},
      {"module m; sub_mod inst_0(); endmodule"},

      // Invalid instance name with consecutive underscores
      {"module m; sub_mod ", {kSymbol, "u__inst"}, "(); endmodule"},

      // Invalid instance name ending with underscore
      {"module m; sub_mod ", {kSymbol, "inst_"}, "(); endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R22Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
