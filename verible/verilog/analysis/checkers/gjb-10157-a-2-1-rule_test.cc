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

#include "verible/verilog/analysis/checkers/gjb-10157-a-2-1-rule.h"

#include <initializer_list>
#include <string>

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

constexpr int kToken = SymbolIdentifier;

// Test that no violations when filename matches module name
TEST(Gjb10157A21RuleTest, ModuleMatchesFilename) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module mymod; endmodule"},
      {"module other; endmodule\nmodule mymod; endmodule"},
  };
  const std::string filename = "/path/to/mymod.sv";
  RunLintTestCases<VerilogAnalyzer, Gjb10157A21Rule>(kTestCases, filename);
}

// Test violation when module name doesn't match filename
TEST(Gjb10157A21RuleTest, ModuleDoesNotMatchFilename) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module ", {kToken, "wrong_name"}, "; endmodule"},
      {"module foo; endmodule\nmodule ", {kToken, "bar"}, "; endmodule"},
  };
  const std::string filename = "/path/to/correct_name.sv";
  RunLintTestCases<VerilogAnalyzer, Gjb10157A21Rule>(kTestCases, filename);
}

// Test with no modules - no violation
TEST(Gjb10157A21RuleTest, NoModules) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"package p; endpackage"},
  };
  const std::string filename = "/path/to/anyname.sv";
  RunLintTestCases<VerilogAnalyzer, Gjb10157A21Rule>(kTestCases, filename);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
