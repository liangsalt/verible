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

#include <set>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/net.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/analysis/module-port-dirs-cache.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintViolation;
using verible::Symbol;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;

// Register the lint rule.
VERILOG_REGISTER_LINT_RULE(Gjb10157R32Rule);

const LintRuleDescriptor &Gjb10157R32Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-R-3-2",
      .topic = "port-connections",
      .desc =
          "Checks port reg/wire usage: (A) input ports must not be reg, "
          "(B) inout ports must not be reg, "
          "(C) output reg signals must not be connected to sub-module "
          "instance ports (should be wire if driven by sub-module). "
          "[GJB 10157 R-3-2]",
  };
  return d;
}

absl::Status Gjb10157R32Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

namespace {

// Recursively searches a subtree for a leaf with TK_reg token.
bool HasRegKeywordInSubtree(const Symbol &symbol) {
  if (symbol.Kind() == verible::SymbolKind::kLeaf) {
    const auto &leaf = verible::SymbolCastToLeaf(symbol);
    return leaf.get().token_enum() == TK_reg;
  }
  const auto &node = verible::SymbolCastToNode(symbol);
  for (const auto &child : node.children()) {
    if (child && HasRegKeywordInSubtree(*child)) return true;
  }
  return false;
}

// Returns the first SymbolIdentifier leaf found in the subtree, or nullptr.
const SyntaxTreeLeaf *GetFirstIdentifierInSubtree(const Symbol &symbol) {
  if (symbol.Kind() == verible::SymbolKind::kLeaf) {
    const auto &leaf = verible::SymbolCastToLeaf(symbol);
    if (leaf.get().token_enum() == SymbolIdentifier) {
      return &leaf;
    }
    return nullptr;
  }
  const auto &node = verible::SymbolCastToNode(symbol);
  for (const auto &child : node.children()) {
    if (child) {
      const auto *result = GetFirstIdentifierInSubtree(*child);
      if (result) return result;
    }
  }
  return nullptr;
}

// Extracts the text of the first identifier leaf in a Symbol tree.
std::string_view GetFirstIdentifierText(const Symbol &symbol) {
  const auto *leaf = GetFirstIdentifierInSubtree(symbol);
  if (!leaf) return {};
  return leaf->get().text();
}

}  // namespace

void Gjb10157R32Rule::Lint(const verible::TextStructureView &text_structure,
                           std::string_view filename) {
  const auto &tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // ========== Phase 1: Build same-file module port direction map ==========
  // module_name -> {port_name -> direction_text ("input"/"output"/"inout")}
  absl::flat_hash_map<std::string_view,
                       absl::flat_hash_map<std::string_view, std::string_view>>
      module_port_dirs;

  for (const auto &mod_match : FindAllModuleDeclarations(*tree)) {
    const auto *name_leaf = GetModuleName(*mod_match.match);
    if (!name_leaf) continue;
    std::string_view mod_name = name_leaf->get().text();
    auto &ports = module_port_dirs[mod_name];

    // ANSI-style port declarations from module header.
    const auto *port_decl_list =
        GetModulePortDeclarationList(*mod_match.match);
    if (port_decl_list) {
      for (const auto &port_match :
           FindAllPortDeclarations(*port_decl_list)) {
        const auto *dir_leaf =
            GetDirectionFromPortDeclaration(*port_match.match);
        const auto *id_leaf =
            GetIdentifierFromPortDeclaration(*port_match.match);
        if (dir_leaf && id_leaf) {
          ports[id_leaf->get().text()] = dir_leaf->get().text();
        }
      }
    }

    // Non-ANSI-style port declarations from module body.
    for (const auto &port_match :
         FindAllModulePortDeclarations(*mod_match.match)) {
      const auto *dir_leaf =
          GetDirectionFromModulePortDeclaration(*port_match.match);
      const auto *id_leaf =
          GetIdentifierFromModulePortDeclaration(*port_match.match);
      if (dir_leaf && id_leaf) {
        ports[id_leaf->get().text()] = dir_leaf->get().text();
      }
    }
  }

  // ========== Phase 2: For each module, perform checks ==========
  for (const auto &mod_match : FindAllModuleDeclarations(*tree)) {
    // ------ Check A+B: input reg / inout reg declarations ------

    // ANSI-style ports
    const auto *port_decl_list =
        GetModulePortDeclarationList(*mod_match.match);
    if (port_decl_list) {
      for (const auto &port_match :
           FindAllPortDeclarations(*port_decl_list)) {
        const auto *dir_leaf =
            GetDirectionFromPortDeclaration(*port_match.match);
        const auto *id_leaf =
            GetIdentifierFromPortDeclaration(*port_match.match);
        if (!dir_leaf || !id_leaf) continue;
        std::string_view dir = dir_leaf->get().text();
        if ((dir == "input" || dir == "inout") &&
            HasRegKeywordInSubtree(*port_match.match)) {
          std::string reason = absl::StrCat(
              "Port '", id_leaf->get().text(),
              "' is declared as '", dir,
              " reg', which is illegal in Verilog. ",
              dir, " ports cannot be reg type. [GJB 10157 R-3-2]");
          violations_.insert(
              LintViolation(id_leaf->get(), reason, mod_match.context));
        }
      }
    }

    // Non-ANSI-style ports
    for (const auto &port_match :
         FindAllModulePortDeclarations(*mod_match.match)) {
      const auto *dir_leaf =
          GetDirectionFromModulePortDeclaration(*port_match.match);
      const auto *id_leaf =
          GetIdentifierFromModulePortDeclaration(*port_match.match);
      if (!dir_leaf || !id_leaf) continue;
      std::string_view dir = dir_leaf->get().text();
      if ((dir == "input" || dir == "inout") &&
          HasRegKeywordInSubtree(*port_match.match)) {
        std::string reason = absl::StrCat(
            "Port '", id_leaf->get().text(),
            "' is declared as '", dir,
            " reg', which is illegal in Verilog. ",
            dir, " ports cannot be reg type. [GJB 10157 R-3-2]");
        violations_.insert(
            LintViolation(id_leaf->get(), reason, mod_match.context));
      }
    }

    // ------ Check C: reg signals connected to sub-module ports ------

    // Collect output_reg_signals (for fallback check when sub-module unknown).
    absl::flat_hash_set<std::string_view> output_reg_signals;
    if (port_decl_list) {
      for (const auto &port_match :
           FindAllPortDeclarations(*port_decl_list)) {
        const auto *dir_leaf =
            GetDirectionFromPortDeclaration(*port_match.match);
        const auto *id_leaf =
            GetIdentifierFromPortDeclaration(*port_match.match);
        if (dir_leaf && id_leaf && dir_leaf->get().text() == "output" &&
            HasRegKeywordInSubtree(*port_match.match)) {
          output_reg_signals.insert(id_leaf->get().text());
        }
      }
    }
    // Non-ANSI output reg
    for (const auto &port_match :
         FindAllModulePortDeclarations(*mod_match.match)) {
      const auto *dir_leaf =
          GetDirectionFromModulePortDeclaration(*port_match.match);
      const auto *id_leaf =
          GetIdentifierFromModulePortDeclaration(*port_match.match);
      if (dir_leaf && id_leaf && dir_leaf->get().text() == "output" &&
          HasRegKeywordInSubtree(*port_match.match)) {
        output_reg_signals.insert(id_leaf->get().text());
      }
    }

    // Collect all_reg_signals (for precise check when sub-module is known).
    absl::flat_hash_set<std::string_view> all_reg_signals(
        output_reg_signals.begin(), output_reg_signals.end());

    // Standalone reg declarations in the module body.
    for (const auto &reg_match :
         FindAllRegisterVariables(*mod_match.match)) {
      const auto *name_leaf =
          GetNameLeafOfRegisterVariable(*reg_match.match);
      if (name_leaf) {
        all_reg_signals.insert(name_leaf->get().text());
      }
    }

    // Check sub-module instantiation port connections.
    for (const auto &data_match :
         FindAllDataDeclarations(*mod_match.match)) {
      const auto *type_id =
          GetTypeIdentifierFromDataDeclaration(*data_match.match);
      if (!type_id) continue;

      std::string_view type_name = GetFirstIdentifierText(*type_id);
      if (type_name.empty()) continue;

      // Determine if we know the sub-module's port directions.
      auto mod_it = module_port_dirs.find(type_name);
      const bool known_same_file = (mod_it != module_port_dirs.end());

      const auto &cache = ModulePortDirsCache::GetInstance();
      const std::string type_name_str(type_name);
      const bool known_from_cache =
          !known_same_file && cache.HasModule(type_name_str);

      for (const auto &gate_match :
           FindAllGateInstances(*data_match.match)) {
        for (const auto &port_match :
             FindAllActualNamedPort(*gate_match.match)) {
          const auto *port_name_leaf =
              GetActualNamedPortName(*port_match.match);
          if (!port_name_leaf) continue;
          std::string_view port_name = port_name_leaf->get().text();

          // Get connected signal.
          const auto *paren_group =
              GetActualNamedPortParenGroup(*port_match.match);
          if (!paren_group) continue;
          const auto *signal_leaf = GetFirstIdentifierInSubtree(*paren_group);
          if (!signal_leaf) continue;
          std::string_view signal_name = signal_leaf->get().text();

          if (known_same_file) {
            // Layer 1: Sub-module defined in same file — precise check.
            auto port_it = mod_it->second.find(port_name);
            if (port_it == mod_it->second.end()) continue;
            if (port_it->second != "output" && port_it->second != "inout") {
              continue;
            }
            if (all_reg_signals.contains(signal_name)) {
              violations_.insert(LintViolation(
                  signal_leaf->get(),
                  absl::StrCat(
                      "Signal '", signal_name,
                      "' is declared as 'reg' but connected to ",
                      port_it->second, " port '", port_name, "' of module '",
                      type_name,
                      "'. Signals driven by module outputs must be 'wire', "
                      "not 'reg'. [GJB 10157 R-3-2]"),
                  mod_match.context));
            }
          } else if (known_from_cache) {
            // Layer 2: Sub-module known from cross-file cache.
            std::string dir =
                cache.GetPortDirection(type_name_str, std::string(port_name));
            if (dir != "output" && dir != "inout") continue;
            if (all_reg_signals.contains(signal_name)) {
              violations_.insert(LintViolation(
                  signal_leaf->get(),
                  absl::StrCat(
                      "Signal '", signal_name,
                      "' is declared as 'reg' but connected to ", dir,
                      " port '", port_name, "' of module '", type_name,
                      "'. Signals driven by module outputs must be 'wire', "
                      "not 'reg'. [GJB 10157 R-3-2]"),
                  mod_match.context));
            }
          } else {
            // Layer 3: Sub-module unknown — fallback.
            // Flag output reg signals connected to any sub-module port.
            if (output_reg_signals.contains(signal_name)) {
              violations_.insert(LintViolation(
                  signal_leaf->get(),
                  absl::StrCat(
                      "Signal '", signal_name,
                      "' is declared as 'output reg' but connected to port '",
                      port_name, "' of module '", type_name,
                      "'. An output signal connected to a sub-module port "
                      "should be 'wire', not 'reg'. [GJB 10157 R-3-2]"),
                  mod_match.context));
            }
          }
        }
      }
    }
  }
}

verible::LintRuleStatus Gjb10157R32Rule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
