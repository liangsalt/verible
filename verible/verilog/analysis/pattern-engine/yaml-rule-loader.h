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

// Loads a YAML rule definition from disk and compiles its `pattern` section
// into a verible::matcher::Matcher tree. Each rule is referenced by name and
// carries its source path, metadata, suggested fix text, and corpus paths.

#ifndef VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_YAML_RULE_LOADER_H_
#define VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_YAML_RULE_LOADER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/verilog/analysis/pattern-engine/condition.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

// Where a rule was loaded from. Higher precedence wins on name collision.
enum class RuleScope {
  kProject = 0,    // highest precedence
  kUser = 1,
  kTeam = 2,
  kCommunity = 3,  // lowest precedence
};

// Severity mirrors LintRuleSeverity in descriptions.h but kept local here to
// avoid a cyclic dep with the lint-rule registry.
enum class Severity { kError, kWarning };

struct CorpusPaths {
  std::string bad;   // path to bad.sv (must hit)
  std::string good;  // path to good.sv (must not hit)
};

// A rule loaded from YAML, ready to be scanned over a project. Stats are
// updated asynchronously by the telemetry pipeline (not by this loader).
//
// verible::matcher::Matcher has no default constructor, so LoadedRule must
// be initialized with a matcher up front. The loader supplies a "match
// nothing" placeholder during partial construction; on success the field is
// overwritten with the compiled pattern.
struct LoadedRule {
  explicit LoadedRule(verible::matcher::Matcher matcher_init);

  // Default copy/move so callers can store LoadedRule in std::vector.
  LoadedRule(const LoadedRule&) = default;
  LoadedRule(LoadedRule&&) = default;
  LoadedRule& operator=(const LoadedRule&) = default;
  LoadedRule& operator=(LoadedRule&&) = default;

  std::string name;
  std::string topic;
  Severity severity = Severity::kWarning;
  std::string why_bad;
  std::string suggested_fix;

  // Compiled CST matcher. Bindings declared in the YAML pattern survive as
  // BoundSymbolManager keys when this matcher fires.
  verible::matcher::Matcher pattern_matcher;

  // Structured condition AST. nullptr means "no condition" (all pattern
  // matches pass through). Owned by this LoadedRule.
  std::shared_ptr<ConditionNode> condition;

  CorpusPaths examples;

  // Provenance.
  RuleScope source_scope = RuleScope::kProject;
  std::string source_path;  // path to the YAML file itself
};

// Parse one YAML file into a LoadedRule. Fails if the file does not parse,
// has no `pattern` field, or references unknown node/token names.
//
// Implementation lands in A1.4 (depends on a yaml-cpp bazel_dep being added
// to MODULE.bazel). A1.1 is a stub that returns NotFound.
absl::StatusOr<LoadedRule> LoadRuleFromYaml(std::string_view yaml_path,
                                            RuleScope scope);

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_PATTERN_ENGINE_YAML_RULE_LOADER_H_
