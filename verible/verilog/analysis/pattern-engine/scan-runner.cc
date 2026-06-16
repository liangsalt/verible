// Copyright 2026 The Verible Authors.
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

#include "verible/verilog/analysis/pattern-engine/scan-runner.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/analysis/pattern-engine/condition.h"
#include "verible/verilog/analysis/pattern-engine/rule-set.h"
#include "verible/verilog/analysis/pattern-engine/semantic-primitives.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

namespace {

// Compute byte offset of `inner` within `outer`. Returns -1 if the spans
// don't share backing storage (defensive; should not happen in normal use).
int RelativeOffset(std::string_view outer, std::string_view inner) {
  const char* base = outer.data();
  const char* tip = inner.data();
  if (tip < base || tip >= base + outer.size()) return -1;
  return static_cast<int>(tip - base);
}

}  // namespace

std::vector<Hit> ScanProject(const VerilogProject& project,
                             const RuleSet& rules) {
  // Make sure built-in primitives are available. Safe to call repeatedly.
  EnsureBuiltinsRegistered();
  std::vector<Hit> hits;

  for (const auto& [name, file_ptr] : project) {
    if (file_ptr == nullptr) continue;
    // Ensure file is parsed. If parse fails we still skip without aborting the
    // whole scan -- broken files shouldn't take out the rest of the project.
    file_ptr->Parse().IgnoreError();

    const verible::TextStructureView* ts = file_ptr->GetTextStructure();
    if (ts == nullptr) continue;
    const auto& syntax_tree = ts->SyntaxTree();
    if (syntax_tree == nullptr) continue;
    const std::string_view file_content = file_ptr->GetContent();

    for (const auto& rule : rules.rules()) {
      // Find all matches under this file's CST root.
      const auto matches =
          verible::SearchSyntaxTree(*syntax_tree, rule.pattern_matcher);

      for (const auto& m : matches) {
        if (m.match == nullptr) continue;

        // Re-run the matcher to capture bindings (SearchSyntaxTree drops them).
        verible::matcher::BoundSymbolManager bound;
        if (!rule.pattern_matcher.Matches(*m.match, &bound)) {
          // Matched once during search but not on direct re-eval. Rare; skip.
          continue;
        }

        // Apply semantic condition (if any) -- a false condition filters out
        // this match. Conditions that fail to evaluate (e.g. unknown
        // primitive) are conservatively treated as "no filter" so a typo in
        // one rule doesn't silently drop legitimate hits from other rules.
        if (rule.condition) {
          EvalContext ectx;
          ectx.bindings = &bound.GetBoundMap();
          ectx.project = &project;
          ectx.current_file = file_ptr.get();
          auto verdict = EvaluateCondition(rule.condition.get(), ectx);
          if (verdict.ok() && !*verdict) continue;
        }

        Hit hit;
        hit.rule_name = rule.name;
        hit.file_path = file_ptr->ResolvedPath();
        const std::string_view span = verible::StringSpanOfSymbol(*m.match);
        hit.byte_offset = RelativeOffset(file_content, span);
        hit.byte_length = static_cast<int>(span.size());

        for (const auto& [bind_id, bound_symbol] : bound.GetBoundMap()) {
          if (bound_symbol == nullptr) continue;
          const std::string_view bspan =
              verible::StringSpanOfSymbol(*bound_symbol);
          const int boff = RelativeOffset(file_content, bspan);
          const int blen = static_cast<int>(bspan.size());
          hit.bindings[bind_id] = {boff, blen};
        }

        hits.push_back(std::move(hit));
      }
    }
  }

  return hits;
}

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
