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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-4-rule.h"

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
TEST(Gjb10157R24RuleTest, ValidIdentifiersPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Valid names that are not power supply names
      {"module m; reg data_flag; endmodule"},
      {"module m; reg voltage; endmodule"},
      {"module m; reg power_good; endmodule"},
      {"module m; reg vdd_ok; endmodule"},  // Not exact match
      {"module m; reg gnd_detect; endmodule"},  // Not exact match
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R24Rule>(kTestCases, "test.sv");
}

// Tests that VDD is rejected (case-insensitive).
TEST(Gjb10157R24RuleTest, VddFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; reg ", {kSymbol, "VDD"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "vdd"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "Vdd"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R24Rule>(kTestCases, "test.sv");
}

// Tests that VSS is rejected (case-insensitive).
TEST(Gjb10157R24RuleTest, VssFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; reg ", {kSymbol, "VSS"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "vss"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "Vss"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R24Rule>(kTestCases, "test.sv");
}

// Tests that VCC is rejected (case-insensitive).
TEST(Gjb10157R24RuleTest, VccFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; reg ", {kSymbol, "VCC"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "vcc"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R24Rule>(kTestCases, "test.sv");
}

// Tests that GND is rejected (case-insensitive).
TEST(Gjb10157R24RuleTest, GndFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; reg ", {kSymbol, "GND"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "gnd"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "Gnd"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R24Rule>(kTestCases, "test.sv");
}

// Tests that VREF is rejected (case-insensitive).
TEST(Gjb10157R24RuleTest, VrefFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; reg ", {kSymbol, "VREF"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "vref"}, "; endmodule"},
      {"module m; reg ", {kSymbol, "Vref"}, "; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R24Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
