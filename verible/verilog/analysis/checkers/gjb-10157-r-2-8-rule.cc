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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-8-rule.h"

#include <set>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;

VERILOG_REGISTER_LINT_RULE(Gjb10157R28Rule);

const LintRuleDescriptor& Gjb10157R28Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-R-2-8",
      .topic = "data-types",
      .desc =
          "Only reg, wire, integer, tri and parameter data types are allowed. "
          "[GJB 10157 Rule 2-8]",
  };
  return d;
}

absl::Status Gjb10157R28Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

void Gjb10157R28Rule::HandleToken(const TokenInfo& token) {
  int token_type = token.token_enum();
  
  // Check for forbidden data types
  // Allowed: reg, wire, integer, tri, parameter
  // Forbidden: logic, bit, byte, int, shortint, longint, real, realtime,
  //            shortreal, string, time, etc.
  
  bool is_forbidden = false;
  const char* type_name = nullptr;
  
  switch (token_type) {
    case TK_logic:
      is_forbidden = true;
      type_name = "logic";
      break;
    case TK_bit:
      is_forbidden = true;
      type_name = "bit";
      break;
    case TK_byte:
      is_forbidden = true;
      type_name = "byte";
      break;
    case TK_int:
      is_forbidden = true;
      type_name = "int";
      break;
    case TK_shortint:
      is_forbidden = true;
      type_name = "shortint";
      break;
    case TK_longint:
      is_forbidden = true;
      type_name = "longint";
      break;
    case TK_real:
      is_forbidden = true;
      type_name = "real";
      break;
    case TK_realtime:
      is_forbidden = true;
      type_name = "realtime";
      break;
    case TK_shortreal:
      is_forbidden = true;
      type_name = "shortreal";
      break;
    case TK_string:
      is_forbidden = true;
      type_name = "string";
      break;
    case TK_time:
      is_forbidden = true;
      type_name = "time";
      break;
    case TK_wreal:
      is_forbidden = true;
      type_name = "wreal";
      break;
    default:
      break;
  }
  
  if (is_forbidden && type_name != nullptr) {
    std::string reason = absl::StrCat(
        "Forbidden data type '", type_name,
        "'. Only reg, wire, integer, tri and parameter are allowed. "
        "[GJB 10157 R-2-8]");
    violations_.insert(LintViolation(token, reason));
  }
}

LintRuleStatus Gjb10157R28Rule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
