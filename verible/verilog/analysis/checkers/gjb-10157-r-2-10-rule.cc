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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-10-rule.h"

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/analysis/top-modules-flag.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeContext;

VERILOG_REGISTER_LINT_RULE(Gjb10157R210Rule);

const LintRuleDescriptor& Gjb10157R210Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-R-2-10",
      .topic = "port-usage",
      .desc =
          "Top-level input ports must not be left floating (unused). "
          "[GJB 10157 Rule 2-10]",
  };
  return d;
}

absl::Status Gjb10157R210Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

// Structure to hold input port info
struct InputPortInfo {
  const verible::TokenInfo* token;
  std::string_view name;
};

// Collect input ports from ANSI-style port declarations (inline in header)
// These are kPortDeclaration nodes: module foo(input clk, output dout);
static void CollectAnsiInputPorts(const verible::Symbol& module_symbol,
                                   const SyntaxTreeContext& context,
                                   std::vector<InputPortInfo>& input_ports) {
  auto port_matches = FindAllPortDeclarations(module_symbol);
  for (const auto& port_match : port_matches) {
    const auto* direction_leaf =
        GetDirectionFromPortDeclaration(*port_match.match);
    if (direction_leaf == nullptr) continue;

    std::string_view direction = direction_leaf->get().text();
    if (direction != "input") continue;

    const auto* id_leaf =
        GetIdentifierFromPortDeclaration(*port_match.match);
    if (id_leaf == nullptr) continue;

    input_ports.push_back({&id_leaf->get(), id_leaf->get().text()});
  }
}

// Collect input ports from non-ANSI-style port declarations (in module body)
// These are kModulePortDeclaration nodes: module foo(clk); input clk; endmodule
static void CollectNonAnsiInputPorts(const verible::Symbol& module_symbol,
                                      const SyntaxTreeContext& context,
                                      std::vector<InputPortInfo>& input_ports) {
  auto port_matches = FindAllModulePortDeclarations(module_symbol);
  for (const auto& port_match : port_matches) {
    const auto* direction_leaf =
        GetDirectionFromModulePortDeclaration(*port_match.match);
    if (direction_leaf == nullptr) continue;

    std::string_view direction = direction_leaf->get().text();
    if (direction.find("input") == std::string_view::npos) continue;

    const auto* id_leaf =
        GetIdentifierFromModulePortDeclaration(*port_match.match);
    if (id_leaf == nullptr) continue;

    input_ports.push_back({&id_leaf->get(), id_leaf->get().text()});
  }
}

// Count all occurrences of SymbolIdentifier tokens in a subtree,
// excluding identifiers in port declarations.
static void CountAllIdentifiers(const verible::Symbol& root,
                                 std::map<std::string_view, int>& id_counts) {
  if (root.Kind() == verible::SymbolKind::kLeaf) {
    const auto& leaf = verible::SymbolCastToLeaf(root);
    if (leaf.get().token_enum() == SymbolIdentifier) {
      id_counts[leaf.get().text()]++;
    }
    return;
  }

  const auto& node = verible::SymbolCastToNode(root);
  // Skip port declarations - don't count identifier in its own declaration
  if (node.MatchesTag(NodeEnum::kModulePortDeclaration)) {
    return;
  }

  for (const auto& child : node.children()) {
    if (child != nullptr) {
      CountAllIdentifiers(*child, id_counts);
    }
  }
}

// Get the module body (items after the port list)
static const verible::SyntaxTreeNode* GetModuleBody(
    const verible::Symbol& module_decl) {
  auto matches = SearchSyntaxTree(module_decl, NodekModuleItemList());
  if (matches.empty()) return nullptr;
  return &verible::SymbolCastToNode(*matches[0].match);
}

// Check if a module is in the top modules list.
static bool IsTopModule(std::string_view module_name,
                        const std::set<std::string>& top_modules_set) {
  return top_modules_set.count(std::string(module_name)) > 0;
}

void Gjb10157R210Rule::Lint(const verible::TextStructureView& text_structure,
                             std::string_view filename) {
  const auto& tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Get the top modules from multiple sources:
  // 1. Command-line flag (--top_modules)
  // 2. Global cache (set by Language Server)
  std::set<std::string> top_modules_set;

  // First, check command-line flag
  const std::string top_modules_flag = absl::GetFlag(FLAGS_top_modules);
  for (const auto& module : absl::StrSplit(top_modules_flag, ',', absl::SkipEmpty())) {
    top_modules_set.insert(std::string(module));
  }

  // If flag is empty, try the global cache (set by Language Server)
  if (top_modules_set.empty()) {
    const auto& cache = TopModulesCache::GetInstance();
    if (cache.HasTopModules()) {
      top_modules_set = cache.GetTopModules();
    }
  }

  // If still no top modules, skip check entirely.
  // This rule requires project-level analysis to identify top modules.
  if (top_modules_set.empty()) return;

  auto module_matches = FindAllModuleDeclarations(*tree);

  for (const auto& module_match : module_matches) {
    const verible::Symbol* module_symbol = module_match.match;
    if (module_symbol == nullptr) continue;

    // Get the module name.
    const auto* module_name_leaf = GetModuleName(*module_symbol);
    if (module_name_leaf == nullptr) continue;
    std::string_view module_name = module_name_leaf->get().text();

    // Only check top-level modules.
    if (!IsTopModule(module_name, top_modules_set)) continue;

    // Collect input ports from both ANSI and non-ANSI styles
    std::vector<InputPortInfo> input_ports;
    CollectAnsiInputPorts(*module_symbol, module_match.context, input_ports);
    CollectNonAnsiInputPorts(*module_symbol, module_match.context, input_ports);

    if (input_ports.empty()) continue;

    // Get the module body
    const auto* module_body = GetModuleBody(*module_symbol);
    if (module_body == nullptr) {
      // If no body, all inputs are unused
      for (const auto& port_info : input_ports) {
        std::string reason = absl::StrCat(
            "Top-level module '", module_name, "': Input port '", port_info.name,
            "' is declared but never used (floating input). "
            "[GJB 10157 R-2-10]");
        violations_.insert(LintViolation(*port_info.token, reason, module_match.context));
      }
      continue;
    }

    // Count all identifier occurrences in the module body only
    std::map<std::string_view, int> id_counts;
    CountAllIdentifiers(*module_body, id_counts);

    // Check each input port
    for (const auto& port_info : input_ports) {
      auto it = id_counts.find(port_info.name);
      int count = (it != id_counts.end()) ? it->second : 0;

      if (count == 0) {
        std::string reason = absl::StrCat(
            "Top-level module '", module_name, "': Input port '", port_info.name,
            "' is declared but never used (floating input). "
            "[GJB 10157 R-2-10]");
        violations_.insert(LintViolation(*port_info.token, reason, module_match.context));
      }
    }
  }
}

verible::LintRuleStatus Gjb10157R210Rule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
