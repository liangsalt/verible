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

#include "verible/verilog/analysis/checkers/gjb-10157-r-3-2-rule.h"

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

// Tests for Check A: input reg is illegal.
TEST(Gjb10157R32RuleTest, InputRegIsIllegal) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m(input reg ", {SymbolIdentifier, "clk"}, "); endmodule"},
      {"module m(input wire clk); endmodule"},
      {"module m(input clk); endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R32Rule>(kTestCases);
}

// Tests for Check B: inout reg is illegal.
TEST(Gjb10157R32RuleTest, InoutRegIsIllegal) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m(inout reg ", {SymbolIdentifier, "data"}, "); endmodule"},
      {"module m(inout wire data); endmodule"},
      {"module m(inout data); endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R32Rule>(kTestCases);
}

// Tests for Check C: output reg connected to known sub-module output port.
TEST(Gjb10157R32RuleTest, RegConnectedToKnownOutputPort) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // reg connected to known output port — violation.
      {"module child(output o); endmodule\n"
       "module parent;\n"
       "  reg sig;\n"
       "  child u(.o(",
       {SymbolIdentifier, "sig"},
       "));\n"
       "endmodule"},
      // wire connected to known output port — no violation.
      {"module child(output o); endmodule\n"
       "module parent;\n"
       "  wire sig;\n"
       "  child u(.o(sig));\n"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R32Rule>(kTestCases);
}

// reg connected to known input port is fine.
TEST(Gjb10157R32RuleTest, RegConnectedToInputPortIsOk) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module child(input i); endmodule\n"
       "module parent;\n"
       "  reg sig;\n"
       "  child u(.i(sig));\n"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R32Rule>(kTestCases);
}

// reg connected to known inout port — violation.
TEST(Gjb10157R32RuleTest, RegConnectedToInoutPort) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module child(inout io); endmodule\n"
       "module parent;\n"
       "  reg sig;\n"
       "  child u(.io(",
       {SymbolIdentifier, "sig"},
       "));\n"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R32Rule>(kTestCases);
}

// output reg connected to known output port — violation.
TEST(Gjb10157R32RuleTest, OutputRegConnectedToKnownOutputPort) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module child(output o); endmodule\n"
       "module parent(output reg sig);\n"
       "  child u(.o(",
       {SymbolIdentifier, "sig"},
       "));\n"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R32Rule>(kTestCases);
}

// Check C Layer 3: output reg connected to unknown sub-module — fallback.
TEST(Gjb10157R32RuleTest, OutputRegConnectedToUnknownSubModule) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // output reg connected to unknown sub-module port — violation.
      {"module parent(output reg sig);\n"
       "  unknown_mod u(.port(",
       {SymbolIdentifier, "sig"},
       "));\n"
       "endmodule"},
      // output wire connected to unknown sub-module port — no violation.
      {"module parent(output wire sig);\n"
       "  unknown_mod u(.port(sig));\n"
       "endmodule"},
      // Internal reg connected to unknown sub-module — no violation
      // (only output reg triggers fallback).
      {"module parent;\n"
       "  reg sig;\n"
       "  unknown_mod u(.port(sig));\n"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R32Rule>(kTestCases);
}

// output reg that is NOT connected to any sub-module — no violation.
TEST(Gjb10157R32RuleTest, OutputRegNotConnectedIsOk) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m(output reg data);\n"
       "  always @(*) data = 1;\n"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R32Rule>(kTestCases);
}

// .done(spi_done) does not affect 'output reg done'.
TEST(Gjb10157R32RuleTest, DifferentSignalNameNoFalsePositive) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module parent(output reg done);\n"
       "  wire spi_done;\n"
       "  unknown_mod u(.done(spi_done));\n"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R32Rule>(kTestCases);
}

// Realistic scenario similar to afe5818_init.v.
TEST(Gjb10157R32RuleTest, RealisticScenario) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module parent(\n"
       "  input wire clk,\n"
       "  output reg done,\n"
       "  output reg error,\n"
       "  output reg cs_n,\n"
       "  output reg sclk_out,\n"
       "  output reg mosi_out\n"
       ");\n"
       "  wire spi_done;\n"
       "  wire spi_busy;\n"
       "  reg start_spi;\n"
       "  unknown_mod u_spi(\n"
       "    .clk(clk),\n"
       "    .start(start_spi),\n"
       "    .done(spi_done),\n"
       "    .busy(spi_busy),\n"
       "    .cs_n(",
       {SymbolIdentifier, "cs_n"},
       "),\n"
       "    .sclk(",
       {SymbolIdentifier, "sclk_out"},
       "),\n"
       "    .mosi(",
       {SymbolIdentifier, "mosi_out"},
       ")\n"
       "  );\n"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, Gjb10157R32Rule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
