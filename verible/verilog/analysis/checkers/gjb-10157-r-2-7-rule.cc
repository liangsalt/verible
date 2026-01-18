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

#include <cstddef>
#include <set>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "absl/strings/match.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TextStructureView;
using verible::TokenInfo;

VERILOG_REGISTER_LINT_RULE(Gjb10157R27Rule);

const LintRuleDescriptor& Gjb10157R27Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-R-2-7",
      .topic = "file-references",
      .desc =
          "Checks that include directives use relative paths, not absolute "
          "paths. [GJB 10157 Rule 2-7]",
  };
  return d;
}

absl::Status Gjb10157R27Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

static bool IsAbsolutePath(std::string_view path) {
  if (path.empty()) return false;
  if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
    path = path.substr(1, path.size() - 2);
  }
  if (path.empty()) return false;
  if (path.front() == '/') return true;
  if (path.size() >= 3) {
    char first = path[0];
    bool is_alpha = (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z');
    if (is_alpha && path[1] == ':' && (path[2] == '/' || path[2] == 92)) {
      return true;
    }
  }
  if (path.size() >= 2) {
    if ((path[0] == 92 && path[1] == 92) || (path[0] == '/' && path[1] == '/')) {
      return true;
    }
  }
  return false;
}

static std::string_view FindIncludePath(std::string_view line) {
  auto first_quote = line.find('"');
  if (first_quote == std::string_view::npos) return {};
  auto second_quote = line.find('"', first_quote + 1);
  if (second_quote == std::string_view::npos) return {};
  return line.substr(first_quote, second_quote - first_quote + 1);
}

void Gjb10157R27Rule::Lint(const TextStructureView& text_structure,
                           std::string_view filename) {
  for (const auto& line : text_structure.Lines()) {
    std::string_view trimmed = line;
    while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) {
      trimmed.remove_prefix(1);
    }
    if (absl::StartsWith(trimmed, "`include")) {
      std::string_view path = FindIncludePath(trimmed);
      if (!path.empty() && IsAbsolutePath(path)) {
        TokenInfo token(TK_StringLiteral, path);
        std::string reason = absl::StrCat(
            "Absolute path in include: ", path,
            ". Use relative paths instead. [GJB 10157 R-2-7]");
        violations_.insert(LintViolation(token, reason));
      }
    }
  }
}

LintRuleStatus Gjb10157R27Rule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
