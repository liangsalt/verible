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

#include "verible/verilog/analysis/pattern-engine/yaml-rule-loader.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/inner-match-handlers.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/verilog/analysis/pattern-engine/condition.h"
#include "verible/verilog/analysis/pattern-engine/matcher-builder.h"
#include "yaml-cpp/yaml.h"

namespace verilog {
namespace analysis {
namespace pattern_engine {

namespace {

using verible::Symbol;
using verible::SymbolKind;
using verible::SymbolTag;
using verible::SyntaxTreeLeaf;
using verible::matcher::BindableMatcher;
using verible::matcher::BoundSymbolManager;
using verible::matcher::InnerMatchAll;
using verible::matcher::Matcher;

// Build a SymbolPredicate that matches a CST leaf whose token text equals
// `expected_text`. Captures by value so the resulting predicate is self-
// contained and can outlive the YAML document.
verible::matcher::SymbolPredicate LeafTextPredicate(std::string expected_text) {
  return [text = std::move(expected_text)](const Symbol& s) -> bool {
    if (s.Tag().kind != SymbolKind::kLeaf) return false;
    const auto* leaf = dynamic_cast<const SyntaxTreeLeaf*>(&s);
    if (leaf == nullptr) return false;
    return leaf->get().text() == text;
  };
}

// Recursively translate a YAML pattern subtree into a BindableMatcher.
// Errors (unknown node names, malformed structure) are reported via `err`;
// the returned matcher is meaningful only when err remains empty.
absl::StatusOr<BindableMatcher> CompilePatternNode(const YAML::Node& yaml) {
  if (!yaml.IsMap()) {
    return absl::InvalidArgumentError(
        "pattern element must be a map with 'node:' or 'leaf:' key");
  }

  // Leaf case: { leaf: "==", bind?: ... }
  if (yaml["leaf"]) {
    const std::string text = yaml["leaf"].as<std::string>();
    BindableMatcher m(LeafTextPredicate(text), InnerMatchAll);
    if (yaml["bind"]) {
      m.Bind(yaml["bind"].as<std::string>());
    }
    return m;
  }

  // Node case: { node: "kBinaryExpression", bind?: ..., inner?: [...] }
  if (!yaml["node"]) {
    return absl::InvalidArgumentError(
        "pattern element must have either 'node:' or 'leaf:' key");
  }

  const std::string node_name = yaml["node"].as<std::string>();
  std::optional<int> node_enum_value = LookupNodeEnum(node_name);
  if (!node_enum_value.has_value()) {
    return absl::InvalidArgumentError(
        absl::StrCat("unknown node name in pattern: ", node_name));
  }

  const SymbolTag tag{SymbolKind::kNode, *node_enum_value};
  BindableMatcher matcher(
      [tag](const Symbol& s) -> bool { return s.Tag() == tag; },
      InnerMatchAll);

  // Recurse into inner children, if any.
  if (yaml["inner"]) {
    if (!yaml["inner"].IsSequence()) {
      return absl::InvalidArgumentError(
          absl::StrCat("'inner' must be a sequence in node ", node_name));
    }
    for (const auto& child_yaml : yaml["inner"]) {
      auto child = CompilePatternNode(child_yaml);
      if (!child.ok()) return child.status();
      matcher.AddMatchers(*child);
    }
  }

  if (yaml["bind"]) {
    matcher.Bind(yaml["bind"].as<std::string>());
  }
  return matcher;
}

absl::StatusOr<std::unique_ptr<ConditionNode>> CompileCondition(
    const YAML::Node& yaml) {
  if (!yaml || !yaml.IsMap()) {
    return absl::InvalidArgumentError("condition entry must be a map");
  }

  // Primitive call leaf: { primitive, target, arg? }
  if (yaml["primitive"]) {
    if (!yaml["target"]) {
      return absl::InvalidArgumentError(
          "primitive condition requires 'target' bind id");
    }
    auto node = std::make_unique<ConditionNode>();
    node->op = ConditionNode::Op::kPrimitive;
    node->primitive.name = yaml["primitive"].as<std::string>();
    node->primitive.target = yaml["target"].as<std::string>();
    if (yaml["arg"]) {
      const YAML::Node& arg = yaml["arg"];
      try {
        // Try integer first; yaml-cpp will throw if it's not numeric.
        node->primitive.arg = arg.as<int64_t>();
      } catch (const YAML::Exception&) {
        node->primitive.arg = arg.as<std::string>();
      }
    }
    return node;
  }

  // Combinators
  for (const char* combo : {"all", "any"}) {
    if (!yaml[combo]) continue;
    const YAML::Node& list = yaml[combo];
    if (!list.IsSequence()) {
      return absl::InvalidArgumentError(
          absl::StrCat("condition '", combo, "' must be a sequence"));
    }
    auto node = std::make_unique<ConditionNode>();
    node->op = std::string(combo) == "all" ? ConditionNode::Op::kAll
                                            : ConditionNode::Op::kAny;
    for (const auto& child_yaml : list) {
      auto child = CompileCondition(child_yaml);
      if (!child.ok()) return child.status();
      node->children.push_back(std::move(*child));
    }
    return node;
  }

  if (yaml["not"]) {
    auto child = CompileCondition(yaml["not"]);
    if (!child.ok()) return child.status();
    auto node = std::make_unique<ConditionNode>();
    node->op = ConditionNode::Op::kNot;
    node->children.push_back(std::move(*child));
    return node;
  }

  return absl::InvalidArgumentError(
      "condition entry must contain 'primitive', 'all', 'any', or 'not'");
}

absl::StatusOr<Severity> ParseSeverity(const YAML::Node& yaml) {
  if (!yaml) return Severity::kWarning;  // default
  const std::string s = yaml.as<std::string>();
  if (s == "error") return Severity::kError;
  if (s == "warning") return Severity::kWarning;
  return absl::InvalidArgumentError(
      absl::StrCat("severity must be 'error' or 'warning', got: ", s));
}

// Read entire file into a string. Returns NotFound if the file is missing.
absl::StatusOr<std::string> SlurpFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) {
    return absl::NotFoundError(absl::StrCat("cannot open ", path));
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace

absl::StatusOr<LoadedRule> LoadRuleFromYaml(std::string_view yaml_path,
                                            RuleScope scope) {
  const std::string path_str(yaml_path);

  auto content = SlurpFile(path_str);
  if (!content.ok()) return content.status();

  YAML::Node doc;
  try {
    doc = YAML::Load(*content);
  } catch (const YAML::Exception& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("YAML parse error in ", path_str, ": ", e.what()));
  }

  if (!doc.IsMap()) {
    return absl::InvalidArgumentError(
        absl::StrCat("rule file ", path_str, " must contain a YAML map"));
  }

  // Required fields
  for (const char* field : {"name", "topic", "severity", "why-bad", "pattern",
                            "examples"}) {
    if (!doc[field]) {
      return absl::InvalidArgumentError(
          absl::StrCat("rule file ", path_str, " missing required field: ",
                       field));
    }
  }

  LoadedRule rule(/*matcher_init=*/Matcher(
      [](const Symbol&) { return false; }, InnerMatchAll));
  rule.source_path = path_str;
  rule.source_scope = scope;
  rule.name = doc["name"].as<std::string>();
  rule.topic = doc["topic"].as<std::string>();

  auto severity = ParseSeverity(doc["severity"]);
  if (!severity.ok()) return severity.status();
  rule.severity = *severity;

  rule.why_bad = doc["why-bad"].as<std::string>();
  if (doc["suggested-fix"]) {
    rule.suggested_fix = doc["suggested-fix"].as<std::string>();
  }

  // Compile pattern
  auto compiled = CompilePatternNode(doc["pattern"]);
  if (!compiled.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("in rule ", rule.name, ": ", compiled.status().message()));
  }
  rule.pattern_matcher = *compiled;

  // Examples: resolve relative paths from the rule file's directory.
  const auto& examples_node = doc["examples"];
  if (!examples_node.IsMap() || !examples_node["bad"] ||
      !examples_node["good"]) {
    return absl::InvalidArgumentError(
        absl::StrCat("rule ", rule.name,
                     " must have examples.bad and examples.good"));
  }
  const std::filesystem::path rule_dir =
      std::filesystem::path(path_str).parent_path();
  rule.examples.bad =
      (rule_dir / examples_node["bad"].as<std::string>()).string();
  rule.examples.good =
      (rule_dir / examples_node["good"].as<std::string>()).string();

  // Parse condition into a structured AST. Skip silently when absent.
  if (doc["condition"]) {
    auto cond = CompileCondition(doc["condition"]);
    if (!cond.ok()) {
      return absl::InvalidArgumentError(
          absl::StrCat("in rule ", rule.name, ": ", cond.status().message()));
    }
    rule.condition = std::shared_ptr<ConditionNode>(std::move(*cond));
  }

  return rule;
}

LoadedRule::LoadedRule(Matcher matcher_init)
    : pattern_matcher(std::move(matcher_init)) {}

}  // namespace pattern_engine
}  // namespace analysis
}  // namespace verilog
