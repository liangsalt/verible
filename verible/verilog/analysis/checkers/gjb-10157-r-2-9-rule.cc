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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-9-rule.h"

#include <set>
#include <string>
#include <string_view>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/context-functions.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::down_cast;
using verible::LintViolation;
using verible::matcher::Matcher;

VERILOG_REGISTER_LINT_RULE(Gjb10157R29Rule);

const LintRuleDescriptor& Gjb10157R29Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-R-2-9",
      .topic = "module-instantiation",
      .desc =
          "Module instantiation must use named port mapping, positional "
          "mapping is prohibited. [GJB 10157 Rule 2-9]",
  };
  return d;
}

absl::Status Gjb10157R29Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

static const Matcher& InstanceMatcher() {
  static const Matcher matcher(
      NodekGateInstance(GateInstanceHasPortList().Bind("list")));
  return matcher;
}

void Gjb10157R29Rule::HandleSymbol(const verible::Symbol& symbol,
                                    const verible::SyntaxTreeContext& context) {
  static constexpr std::string_view kMessage =
      "Positional port mapping is prohibited in module instantiation. "
      "Use named port mapping instead (e.g., .port_name(signal)). "
      "[GJB 10157 R-2-9]";

  // Only check inside module definitions
  if (!ContextIsInsideModule(context)) return;

  verible::matcher::BoundSymbolManager manager;

  if (InstanceMatcher().Matches(symbol, &manager)) {
    if (const auto* port_list_node = manager.GetAsNode("list")) {
      if (!port_list_node->MatchesTag(NodeEnum::kPortActualList)) return;

      // Check each port for positional mapping
      for (const auto& child : port_list_node->children()) {
        if (child == nullptr) continue;

        if (child->Kind() == verible::SymbolKind::kNode) {
          const auto* child_node =
              down_cast<const verible::SyntaxTreeNode*>(child.get());
          if (child_node->MatchesTag(NodeEnum::kActualPositionalPort)) {
            const auto* leaf_ptr = verible::GetLeftmostLeaf(*child_node);
            if (leaf_ptr != nullptr) {
              const verible::TokenInfo token = leaf_ptr->get();
              violations_.insert(LintViolation(token, kMessage, context));
            }
          }
        }
      }
    }
  }
}

verible::LintRuleStatus Gjb10157R29Rule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
