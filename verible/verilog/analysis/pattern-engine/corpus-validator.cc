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

#include "verible/verilog/analysis/pattern-engine/corpus-validator.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/symbol.h"
#include "verible/verilog/analysis/pattern-engine/condition.h"
#include "verible/verilog/analysis/pattern-engine/rule-set.h"
#include "verible/verilog/analysis/pattern-engine/scan-runner.h"
#include "verible/verilog/analysis/pattern-engine/semantic-primitives.h"
#include "verible/verilog/analysis/pattern-engine/yaml-rule-loader.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

namespace {

// Set up a single-file VerilogProject so primitives that need project
// context (e.g. bit_width walks declarations) work the same as in
// production scans. The corpus file's parent directory becomes the project
// root so the relative-path open works.
absl::StatusOr<int> CountHitsViaScan(const std::string& sv_path,
                                     const LoadedRule& rule) {
  EnsureBuiltinsRegistered();

  // Project rooted at the file's parent dir.
  const auto last_slash = sv_path.find_last_of("/\\");
  const std::string root =
      last_slash == std::string::npos ? "." : sv_path.substr(0, last_slash);
  const std::string filename =
      last_slash == std::string::npos ? sv_path : sv_path.substr(last_slash + 1);

  std::vector<std::string> include_paths;
  VerilogProject project(root, include_paths);
  auto src = project.OpenTranslationUnit(filename);
  if (!src.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("cannot open ", sv_path, ": ", src.status().message()));
  }
  (*src)->Parse().IgnoreError();

  // Build a minimal RuleSet containing just this rule.
  RuleSet rs;
  // RuleSet doesn't expose a "set rules manually" entry point; route through
  // ScanProject directly by passing a hand-built local RuleSet via the
  // public LoadFromScopes(empty) + we directly call ScanProject of a custom
  // wrapper. To avoid changing RuleSet's surface, instead replicate just the
  // narrow scan path here.

  const verible::TextStructureView* ts = (*src)->GetTextStructure();
  if (ts == nullptr || ts->SyntaxTree() == nullptr) {
    return absl::InternalError(
        absl::StrCat("no syntax tree for ", sv_path));
  }
  const auto& tree = ts->SyntaxTree();

  // Run pattern matches.
  const auto matches = verible::SearchSyntaxTree(*tree, rule.pattern_matcher);
  int hits = 0;
  for (const auto& m : matches) {
    if (m.match == nullptr) continue;
    verible::matcher::BoundSymbolManager bound;
    if (!rule.pattern_matcher.Matches(*m.match, &bound)) continue;
    if (rule.condition) {
      EvalContext ectx;
      ectx.bindings = &bound.GetBoundMap();
      ectx.project = &project;
      ectx.current_file = *src;
      auto verdict = EvaluateCondition(rule.condition.get(), ectx);
      if (verdict.ok() && !*verdict) continue;
    }
    ++hits;
  }
  return hits;
}

}  // namespace

absl::StatusOr<CorpusResult> ValidateCorpus(const LoadedRule& rule) {
  CorpusResult result;

  auto bad_hits = CountHitsViaScan(rule.examples.bad, rule);
  if (!bad_hits.ok()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "loading examples.bad for rule ", rule.name, ": ",
        bad_hits.status().message()));
  }
  result.bad_hit = *bad_hits > 0;
  if (!result.bad_hit) {
    result.bad_diagnostics.push_back(absl::StrCat(
        rule.examples.bad,
        ": expected rule to fire (>=1 hit) but produced 0 hits"));
  }

  auto good_hits = CountHitsViaScan(rule.examples.good, rule);
  if (!good_hits.ok()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "loading examples.good for rule ", rule.name, ": ",
        good_hits.status().message()));
  }
  result.good_hit = *good_hits > 0;
  if (result.good_hit) {
    result.good_diagnostics.push_back(absl::StrCat(
        rule.examples.good,
        ": expected rule NOT to fire but produced ", *good_hits, " hits"));
  }

  return result;
}

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
