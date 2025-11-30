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

#include "verible/verilog/analysis/checkers/v2001-procedural-decls-rule.h"

#include <initializer_list>
#include <string>
#include <string_view>

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

constexpr int kInteger = TK_integer;

TEST(V2001ProceduralDeclsRuleTest, NoDeclarationsInsideBlockPasses) {
  const std::initializer_list<LintTestCase> kTestCases = {{
      "module m(input clk);\n"
      "  integer i;\n"
      "  initial begin\n"
      "    repeat(5) @(posedge clk);\n"
      "    for (i = 0; i < 8; i = i + 1) begin\n"
      "      $display(\"i=%0d\", i);\n"
      "    end\n"
      "  end\n"
      "endmodule\n",
  }};
  RunLintTestCases<VerilogAnalyzer, V2001ProceduralDeclsRule>(kTestCases,
                                                              "ok.v");
}

TEST(V2001ProceduralDeclsRuleTest, DeclarationInsideInitialFails) {
  const std::initializer_list<LintTestCase> kTestCases = {{
      "module m(input clk);\n"
      "  initial begin\n"
      "    ",
      {kInteger, "integer"},
      " i;\n"
      "    repeat(5) @(posedge clk);\n"
      "  end\n"
      "endmodule\n",
  }};
  RunLintTestCases<VerilogAnalyzer, V2001ProceduralDeclsRule>(kTestCases,
                                                              "bad.v");
}

TEST(V2001ProceduralDeclsRuleTest, TypedForInitInsideBlockFails) {
  const std::initializer_list<LintTestCase> kTestCases = {{
      "module m;\n"
      "  initial begin\n"
      "    for (",
      {kInteger, "integer"},
      " j = 0; j < 4; j = j + 1) begin\n"
      "      $display(j);\n"
      "    end\n"
      "  end\n"
      "endmodule\n",
  }};
  RunLintTestCases<VerilogAnalyzer, V2001ProceduralDeclsRule>(kTestCases,
                                                              "typed.v");
}

TEST(V2001ProceduralDeclsRuleTest, SvFileIsIgnored) {
  const std::initializer_list<LintTestCase> kTestCases = {{
      "module m;\n"
      "  initial begin\n"
      "    integer i;\n"
      "  end\n"
      "endmodule\n",
  }};
  RunLintTestCases<VerilogAnalyzer, V2001ProceduralDeclsRule>(kTestCases,
                                                              "skip.sv");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
