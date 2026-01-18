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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-7-rule.h"

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

constexpr int kString = TK_StringLiteral;

// Tests that relative paths pass.
TEST(Gjb10157R27RuleTest, RelativePathsPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Relative path
      {"`include \"../one_level_higher/definitions.v\"\n"},
      {"`include \"./local/file.v\"\n"},
      {"`include \"subdir/file.vh\"\n"},
      {"`include \"file.v\"\n"},
      // Empty file
      {""},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R27Rule>(kTestCases, "test.sv");
}

// Tests that absolute paths are rejected.
TEST(Gjb10157R27RuleTest, AbsolutePathsFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Windows absolute path
      {"`include ", {kString, "\"c:/documents/definitions.v\""}, "\n"},
      {"`include ", {kString, "\"C:/Users/file.v\""}, "\n"},
      {"`include ", {kString, "\"D:\\projects\\file.v\""}, "\n"},
      // Unix absolute path
      {"`include ", {kString, "\"/home/user/file.v\""}, "\n"},
      {"`include ", {kString, "\"/usr/local/include/file.vh\""}, "\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R27Rule>(kTestCases, "test.sv");
}

// Tests that UNC paths are rejected.
TEST(Gjb10157R27RuleTest, UNCPathsFails) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // UNC paths
      {"`include ", {kString, "\"\\\\server\\share\\file.v\""}, "\n"},
      {"`include ", {kString, "\"//server/share/file.v\""}, "\n"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R27Rule>(kTestCases, "test.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
