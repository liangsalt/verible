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

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/class.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/functions.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/net.h"
#include "verible/verilog/CST/package.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/tasks.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeContext;

// Register the lint rule.
VERILOG_REGISTER_LINT_RULE(Gjb10157R26Rule);

const LintRuleDescriptor &Gjb10157R26Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-R-2-6",
      .topic = "naming",
      .desc =
          "Checks that identifiers do not differ only by case. "
          "[GJB 10157 Rule 2-6]",
  };
  return d;
}

absl::Status Gjb10157R26Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

// Structure to hold identifier info
struct IdentifierInfo {
  const verible::TokenInfo *token;
  SyntaxTreeContext context;
};

void Gjb10157R26Rule::Lint(const verible::TextStructureView &text_structure,
                           std::string_view filename) {
  const auto &tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Map from lowercase identifier to list of original identifiers with same
  // lowercase form
  std::map<std::string, std::vector<IdentifierInfo>> identifier_map;

  // Helper lambda to add an identifier to the map
  auto add_identifier = [&identifier_map](const verible::TokenInfo *token,
                                          const SyntaxTreeContext &context) {
    if (token == nullptr) return;
    std::string lower = absl::AsciiStrToLower(token->text());
    identifier_map[lower].push_back({token, context});
  };

  // Find all module declarations
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekModuleDeclaration())) {
    const auto *name = GetModuleName(*match.match);
    if (name != nullptr) {
      add_identifier(&name->get(), match.context);
    }
  }

  // Find all function declarations
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekFunctionDeclaration())) {
    const auto *name = GetFunctionName(*match.match);
    if (name != nullptr) {
      add_identifier(&name->get(), match.context);
    }
  }

  // Find all task declarations
  for (const auto &match : SearchSyntaxTree(*tree, NodekTaskDeclaration())) {
    const auto *name = GetTaskName(*match.match);
    if (name != nullptr) {
      add_identifier(&name->get(), match.context);
    }
  }

  // Find all class declarations
  for (const auto &match : SearchSyntaxTree(*tree, NodekClassDeclaration())) {
    const auto *name = GetClassName(*match.match);
    if (name != nullptr) {
      add_identifier(&name->get(), match.context);
    }
  }

  // Find all package declarations
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekPackageDeclaration())) {
    const auto *name = GetPackageNameToken(*match.match);
    if (name != nullptr) {
      add_identifier(name, match.context);
    }
  }

  // Find all interface declarations
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekInterfaceDeclaration())) {
    const auto *name = GetInterfaceNameToken(*match.match);
    if (name != nullptr) {
      add_identifier(name, match.context);
    }
  }

  // Find all register variables (reg, logic, etc.)
  for (const auto &match : SearchSyntaxTree(*tree, NodekRegisterVariable())) {
    const auto *name = GetInstanceNameTokenInfoFromRegisterVariable(*match.match);
    if (name != nullptr) {
      add_identifier(name, match.context);
    }
  }

  // Find all net/wire variables (wire, tri, etc.)
  for (const auto &match : SearchSyntaxTree(*tree, NodekNetVariable())) {
    const auto *name_leaf = GetNameLeafOfNetVariable(*match.match);
    if (name_leaf != nullptr) {
      add_identifier(&name_leaf->get(), match.context);
    }
  }

  // Find all module port declarations (non-ANSI style)
  for (const auto &match : SearchSyntaxTree(*tree, NodekModulePortDeclaration())) {
    const auto *name_leaf = GetIdentifierFromModulePortDeclaration(*match.match);
    if (name_leaf != nullptr) {
      add_identifier(&name_leaf->get(), match.context);
    }
  }

  // Find all port declarations (ANSI style)
  for (const auto &match : SearchSyntaxTree(*tree, NodekPortDeclaration())) {
    const auto *name_leaf = GetIdentifierFromPortDeclaration(*match.match);
    if (name_leaf != nullptr) {
      add_identifier(&name_leaf->get(), match.context);
    }
  }

  // Find all gate instances
  for (const auto &match : SearchSyntaxTree(*tree, NodekGateInstance())) {
    const auto *name = GetModuleInstanceNameTokenInfoFromGateInstance(*match.match);
    if (name != nullptr) {
      add_identifier(name, match.context);
    }
  }

  // Check for case-only differences
  for (const auto &[lower_name, identifiers] : identifier_map) {
    if (identifiers.size() < 2) continue;

    // Track the first occurrence of each exact name (by code position)
    // identifiers vector preserves code order
    std::string_view first_name = identifiers[0].token->text();
    std::set<std::string_view> seen_names;
    seen_names.insert(first_name);

    // Check subsequent identifiers for case-only differences
    for (size_t i = 1; i < identifiers.size(); ++i) {
      const auto &id = identifiers[i];
      std::string_view current_name = id.token->text();

      // Skip if we've already seen this exact name (same case)
      if (seen_names.count(current_name) > 0) continue;
      seen_names.insert(current_name);

      // This is a case-only difference from first_name
      std::string reason = absl::StrCat(
          "Identifier '", current_name,
          "' differs from '", first_name,
          "' only by case. Do not use case alone to distinguish identifiers. "
          "[GJB 10157 R-2-6]");
      violations_.insert(LintViolation(*id.token, reason, id.context));
    }
  }
}

verible::LintRuleStatus Gjb10157R26Rule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
