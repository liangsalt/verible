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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-6-rule.h"

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

// Tests that different identifiers pass.
TEST(Gjb10157R26RuleTest, DifferentIdentifiersPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Different names
      {"module top1;\n"
       "  task top2;\n"
       "  endtask\n"
       "endmodule\n"},
      // Same case, same name (no conflict)
      {"module foo;\n"
       "  reg bar;\n"
       "endmodule\n"},
      // Empty file
      {""},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R26Rule>(kTestCases, "test.sv");
}

// Tests that identifiers differing only by case are rejected.
TEST(Gjb10157R26RuleTest, CaseOnlyDifferenceFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Module and task with case-only difference
      {"module top1;\n"
       "  task ", {kSymbol, "Top1"}, ";\n"
       "  endtask\n"
       "endmodule\n"},
      // Multiple case variants
      {"module abc;\n"
       "  task ", {kSymbol, "ABC"}, ";\n"
       "  endtask\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R26Rule>(kTestCases, "test.sv");
}

// Tests that variables with case-only difference are rejected.
TEST(Gjb10157R26RuleTest, VariableCaseDifferenceFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Register variables with case difference
      {"module top;\n"
       "  reg data;\n"
       "  reg ", {kSymbol, "DATA"}, ";\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R26Rule>(kTestCases, "test.sv");
}

// Tests that completely different identifiers pass.
TEST(Gjb10157R26RuleTest, CompletelyDifferentPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // All different identifiers
      {"module alpha;\n"
       "  task beta;\n"
       "  endtask\n"
       "  reg gamma;\n"
       "endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R26Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
