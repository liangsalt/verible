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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-3-rule.h"

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
TEST(Gjb10157R23RuleTest, ValidIdentifiersPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Valid names that are not keywords
      {"module my_module; endmodule"},
      {"module m; reg data_flag; endmodule"},
      {"module m; wire my_signal; endmodule"},
      {"module m; logic clk_100mhz; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R23Rule>(kTestCases, "test.sv");
}

// Tests that SDF keywords are detected.
TEST(Gjb10157R23RuleTest, SdfKeywordsFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // SDF keyword SETUP (case-insensitive)
      {"module m; reg ", {kSymbol, "SETUP"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "setup"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "Setup"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R23Rule>(kTestCases, "test.sv");
}

// Tests that EDIF keywords are detected.
TEST(Gjb10157R23RuleTest, EdifKeywordsFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // EDIF keyword NET (also common in EDIF)
      {"module m; wire ", {kSymbol, "cell"}, "; endmodule"},
      {"module m; wire ", {kSymbol, "CELL"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R23Rule>(kTestCases, "test.sv");
}

// Tests that VHDL keywords are detected.
TEST(Gjb10157R23RuleTest, VhdlKeywordsFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // VHDL keywords
      {"module m; reg ", {kSymbol, "architecture"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "ARCHITECTURE"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "entity"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "ENTITY"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R23Rule>(kTestCases, "test.sv");
}

// Tests module instances with keyword names.
TEST(Gjb10157R23RuleTest, ModuleInstanceKeywordNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Valid instance names
      {"module m; sub_mod u_inst(); endmodule"},

      // Invalid: SDF keyword
      {"module m; sub_mod ", {kSymbol, "delay"}, "(); endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R23Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
