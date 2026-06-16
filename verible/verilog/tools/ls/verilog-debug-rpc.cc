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

#include "verible/verilog/tools/ls/verilog-debug-rpc.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "nlohmann/json.hpp"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/pattern-engine/corpus-validator.h"
#include "verible/verilog/analysis/pattern-engine/rule-set.h"
#include "verible/verilog/analysis/pattern-engine/scan-runner.h"
#include "verible/verilog/analysis/pattern-engine/workspace-manager.h"
#include "verible/verilog/analysis/pattern-engine/yaml-rule-loader.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {

namespace {

using ::verilog::analysis::pattern_engine::LoadedRule;
using ::verilog::analysis::pattern_engine::RuleScope;
using ::verilog::analysis::pattern_engine::ScopePaths;
using ::verilog::analysis::pattern_engine::ValidateCorpus;
using ::verilog::analysis::pattern_engine::WorkspaceManager;
using ::verilog::analysis::pattern_engine::WorkspaceState;

std::string SeverityToString(
    ::verilog::analysis::pattern_engine::Severity sev) {
  using Sev = ::verilog::analysis::pattern_engine::Severity;
  switch (sev) {
    case Sev::kError:
      return "error";
    case Sev::kWarning:
      return "warning";
  }
  return "warning";
}

std::string ScopeToString(RuleScope s) {
  switch (s) {
    case RuleScope::kProject:
      return "project";
    case RuleScope::kUser:
      return "user";
    case RuleScope::kTeam:
      return "team";
    case RuleScope::kCommunity:
      return "community";
  }
  return "project";
}

ScopePaths ParseScopePaths(const nlohmann::json &j) {
  ScopePaths p;
  if (!j.is_object()) return p;
  if (j.contains("project") && j["project"].is_string()) {
    p.project_dir = j["project"].get<std::string>();
  }
  if (j.contains("user") && j["user"].is_string()) {
    p.user_dir = j["user"].get<std::string>();
  }
  if (j.contains("team") && j["team"].is_string()) {
    p.team_dir = j["team"].get<std::string>();
  }
  if (j.contains("community") && j["community"].is_string()) {
    p.community_dir = j["community"].get<std::string>();
  }
  return p;
}

nlohmann::json ErrorObj(const std::string &msg) {
  return {{"ok", false}, {"error", msg}};
}

}  // namespace

nlohmann::json HandleWorkspaceOpen(WorkspaceManager *mgr,
                                   const nlohmann::json &params) {
  if (!params.contains("root") || !params["root"].is_string()) {
    return ErrorObj("missing or non-string 'root' parameter");
  }
  const std::string root = params["root"].get<std::string>();

  ScopePaths scope_paths;
  if (params.contains("scope_paths")) {
    scope_paths = ParseScopePaths(params["scope_paths"]);
  }
  // Convention: when scope_paths is absent, look for <root>/rules/.
  if (scope_paths.project_dir.empty()) {
    scope_paths.project_dir = root + "/rules";
  }

  std::vector<std::string> extra_dirs;
  if (params.contains("extra_source_dirs") &&
      params["extra_source_dirs"].is_array()) {
    for (const auto &v : params["extra_source_dirs"]) {
      if (v.is_string()) extra_dirs.push_back(v.get<std::string>());
    }
  }

  auto status = mgr->Open(root, scope_paths, extra_dirs);
  if (!status.ok()) return ErrorObj(std::string(status.message()));

  nlohmann::json result = {{"ok", true}};
  const WorkspaceState *w = mgr->Get(root);
  if (w != nullptr) {
    result["rule_count"] = w->rule_set.rules().size();
    if (!w->rule_set.load_errors().empty()) {
      result["load_errors"] = w->rule_set.load_errors();
    }
  }
  return result;
}

nlohmann::json HandleWorkspaceClose(WorkspaceManager *mgr,
                                    const nlohmann::json &params) {
  if (!params.contains("root") || !params["root"].is_string()) {
    return ErrorObj("missing or non-string 'root' parameter");
  }
  const std::string root = params["root"].get<std::string>();
  auto status = mgr->Close(root);
  if (!status.ok()) return ErrorObj(std::string(status.message()));
  return {{"ok", true}};
}

nlohmann::json HandleWorkspaceList(WorkspaceManager *mgr,
                                   const nlohmann::json & /*params*/) {
  return {{"workspaces", mgr->ListOpenWorkspaces()}};
}

nlohmann::json HandleDebugScanRules(WorkspaceManager *mgr,
                                    const nlohmann::json &params) {
  if (!params.contains("workspace") || !params["workspace"].is_string()) {
    return ErrorObj("missing or non-string 'workspace' parameter");
  }
  const std::string ws = params["workspace"].get<std::string>();
  WorkspaceState *w = mgr->Get(ws);
  if (w == nullptr) return ErrorObj("workspace not open: " + ws);

  // Optional source_files: pre-open each one in the VerilogProject.
  if (params.contains("source_files") && params["source_files"].is_array()) {
    for (const auto &v : params["source_files"]) {
      if (!v.is_string()) continue;
      // OpenTranslationUnit returns a StatusOr; ignore errors (best-effort).
      w->project->OpenTranslationUnit(v.get<std::string>()).IgnoreError();
    }
  }

  // Optional ruleIds filter.
  std::unordered_set<std::string> only;
  if (params.contains("ruleIds") && params["ruleIds"].is_array()) {
    for (const auto &v : params["ruleIds"]) {
      if (v.is_string()) only.insert(v.get<std::string>());
    }
  }

  // Build a filtered RuleSet view if needed. ScanProject takes a RuleSet by
  // ref so we either pass through the workspace's RuleSet directly or
  // construct a temporary filtered copy.
  const auto &all_hits =
      ::verilog::analysis::pattern_engine::ScanProject(*w->project, w->rule_set);

  nlohmann::json hits_json = nlohmann::json::array();
  for (const auto &hit : all_hits) {
    if (!only.empty() && only.find(hit.rule_name) == only.end()) continue;
    nlohmann::json h;
    h["rule"] = hit.rule_name;
    h["file"] = hit.file_path;
    h["byte_offset"] = hit.byte_offset;
    h["byte_length"] = hit.byte_length;
    nlohmann::json bindings = nlohmann::json::object();
    for (const auto &[name, range] : hit.bindings) {
      bindings[name] = {{"byte_offset", range.first},
                        {"byte_length", range.second}};
    }
    h["bindings"] = bindings;
    hits_json.push_back(h);
  }
  return {{"hits", hits_json}};
}

nlohmann::json HandleDebugReloadRules(WorkspaceManager *mgr,
                                      const nlohmann::json &params) {
  if (!params.contains("workspace") || !params["workspace"].is_string()) {
    return ErrorObj("missing or non-string 'workspace' parameter");
  }
  const std::string ws = params["workspace"].get<std::string>();
  auto status = mgr->ReloadRules(ws);
  if (!status.ok()) return ErrorObj(std::string(status.message()));

  WorkspaceState *w = mgr->Get(ws);
  return {{"ok", true},
          {"rule_count", w->rule_set.rules().size()},
          {"load_errors", w->rule_set.load_errors()}};
}

nlohmann::json HandleDebugValidateRule(WorkspaceManager *mgr,
                                       const nlohmann::json &params) {
  if (!params.contains("workspace") || !params["workspace"].is_string() ||
      !params.contains("ruleId") || !params["ruleId"].is_string()) {
    return ErrorObj("requires 'workspace' and 'ruleId' string params");
  }
  const std::string ws = params["workspace"].get<std::string>();
  const std::string rule_id = params["ruleId"].get<std::string>();
  WorkspaceState *w = mgr->Get(ws);
  if (w == nullptr) return ErrorObj("workspace not open: " + ws);

  auto it = std::find_if(
      w->rule_set.rules().begin(), w->rule_set.rules().end(),
      [&](const LoadedRule &r) { return r.name == rule_id; });
  if (it == w->rule_set.rules().end()) {
    return ErrorObj("no such rule in workspace: " + rule_id);
  }

  auto result = ValidateCorpus(*it);
  if (!result.ok()) return ErrorObj(std::string(result.status().message()));

  return {{"passed", result->passed()},
          {"bad_hit", result->bad_hit},
          {"good_hit", result->good_hit},
          {"bad_diagnostics", result->bad_diagnostics},
          {"good_diagnostics", result->good_diagnostics}};
}

namespace {

// Recursively serialize a CST subtree as JSON, truncated at max_depth.
// Each node carries: tag (string), byte_offset, byte_length, and either
// children (array) for nodes or text for leaves.
nlohmann::json SerializeCst(const verible::Symbol &sym,
                            std::string_view file_content, int max_depth) {
  nlohmann::json out;
  const std::string_view span = verible::StringSpanOfSymbol(sym);
  const int offset = (span.data() >= file_content.data() &&
                      span.data() < file_content.data() + file_content.size())
                         ? static_cast<int>(span.data() - file_content.data())
                         : -1;
  out["byte_offset"] = offset;
  out["byte_length"] = static_cast<int>(span.size());

  if (sym.Tag().kind == verible::SymbolKind::kLeaf) {
    out["kind"] = "leaf";
    const auto *leaf = dynamic_cast<const verible::SyntaxTreeLeaf *>(&sym);
    out["text"] = std::string(leaf != nullptr ? leaf->get().text() : "");
    return out;
  }

  // Node case.
  out["kind"] = "node";
  out["tag"] = verilog::NodeEnumToString(
      static_cast<verilog::NodeEnum>(sym.Tag().tag));
  if (max_depth <= 0) {
    out["children_truncated"] = true;
    return out;
  }

  const auto *node = dynamic_cast<const verible::SyntaxTreeNode *>(&sym);
  if (node == nullptr) {
    out["children"] = nlohmann::json::array();
    return out;
  }
  nlohmann::json children = nlohmann::json::array();
  for (const auto &child : node->children()) {
    if (child == nullptr) continue;
    children.push_back(SerializeCst(*child, file_content, max_depth - 1));
  }
  out["children"] = children;
  return out;
}

}  // namespace

nlohmann::json HandleDebugListRules(WorkspaceManager *mgr,
                                    const nlohmann::json &params) {
  if (!params.contains("workspace") || !params["workspace"].is_string()) {
    return ErrorObj("missing or non-string 'workspace' parameter");
  }
  const std::string ws = params["workspace"].get<std::string>();
  const WorkspaceState *w = mgr->Get(ws);
  if (w == nullptr) return ErrorObj("workspace not open: " + ws);

  nlohmann::json arr = nlohmann::json::array();
  for (const auto &r : w->rule_set.rules()) {
    arr.push_back({{"name", r.name},
                   {"topic", r.topic},
                   {"severity", SeverityToString(r.severity)},
                   {"source_scope", ScopeToString(r.source_scope)},
                   {"source_path", r.source_path},
                   {"why_bad", r.why_bad},
                   {"suggested_fix", r.suggested_fix}});
  }
  return {{"rules", arr}};
}

nlohmann::json HandleDebugGetCstNode(WorkspaceManager *mgr,
                                     const nlohmann::json &params) {
  if (!params.contains("workspace") || !params["workspace"].is_string() ||
      !params.contains("file") || !params["file"].is_string()) {
    return ErrorObj("requires 'workspace' and 'file' string params");
  }
  const std::string ws = params["workspace"].get<std::string>();
  const std::string file = params["file"].get<std::string>();
  WorkspaceState *w = mgr->Get(ws);
  if (w == nullptr) return ErrorObj("workspace not open: " + ws);

  int max_depth = 3;
  if (params.contains("max_depth") && params["max_depth"].is_number_integer()) {
    max_depth = params["max_depth"].get<int>();
  }

  auto src = w->project->OpenTranslationUnit(file);
  if (!src.ok()) return ErrorObj(std::string(src.status().message()));
  (*src)->Parse().IgnoreError();
  const verible::TextStructureView *ts = (*src)->GetTextStructure();
  if (ts == nullptr) return ErrorObj("file produced no text structure: " + file);
  const auto &tree = ts->SyntaxTree();
  if (tree == nullptr) return ErrorObj("file produced no syntax tree: " + file);

  const std::string_view content = (*src)->GetContent();
  return {{"tree", SerializeCst(*tree, content, max_depth)},
          {"file", (*src)->ResolvedPath()}};
}

nlohmann::json HandleDebugListSymbols(WorkspaceManager *mgr,
                                      const nlohmann::json &params) {
  if (!params.contains("workspace") || !params["workspace"].is_string()) {
    return ErrorObj("missing or non-string 'workspace' parameter");
  }
  const std::string ws = params["workspace"].get<std::string>();
  WorkspaceState *w = mgr->Get(ws);
  if (w == nullptr) return ErrorObj("workspace not open: " + ws);

  // Walk every open file looking for top-level declarations. We don't have
  // a SymbolTable here, so identification is structural: find each module /
  // package / function / task / class declaration node, then its first
  // SymbolIdentifier leaf is the declared name.
  nlohmann::json symbols = nlohmann::json::array();

  auto extract = [&](const verible::Symbol &decl_node,
                     std::string_view file_content,
                     const std::string &file_path,
                     std::string_view kind) {
    const std::string_view span = verible::StringSpanOfSymbol(decl_node);
    const int offset = (span.data() >= file_content.data() &&
                        span.data() < file_content.data() + file_content.size())
                           ? static_cast<int>(span.data() - file_content.data())
                           : -1;
    // Find first SymbolIdentifier leaf.
    std::string name = "<anon>";
    const auto idents = verible::SearchSyntaxTree(decl_node, SymbolIdentifierLeaf());
    if (!idents.empty() && idents.front().match != nullptr) {
      const auto *leaf = dynamic_cast<const verible::SyntaxTreeLeaf *>(
          idents.front().match);
      if (leaf != nullptr) name = std::string(leaf->get().text());
    }
    symbols.push_back({{"name", name},
                       {"kind", std::string(kind)},
                       {"file", file_path},
                       {"byte_offset", offset},
                       {"byte_length", static_cast<int>(span.size())}});
  };

  for (const auto &[ref_name, file_ptr] : *w->project) {
    if (file_ptr == nullptr) continue;
    file_ptr->Parse().IgnoreError();
    const verible::TextStructureView *ts = file_ptr->GetTextStructure();
    if (ts == nullptr) continue;
    const auto &tree = ts->SyntaxTree();
    if (tree == nullptr) continue;
    const std::string_view content = file_ptr->GetContent();
    const std::string path = std::string(file_ptr->ResolvedPath());

    // Use the pre-instantiated per-tag matchers from verilog-matchers.h
    // (constexpr NodekFoo instances). No need for DynamicTagMatchBuilder.
    auto scan = [&](const auto &matcher, const char *label) {
      const auto matches = verible::SearchSyntaxTree(*tree, matcher());
      for (const auto &m : matches) {
        if (m.match == nullptr) continue;
        extract(*m.match, content, path, label);
      }
    };
    scan(verilog::NodekModuleDeclaration, "module");
    scan(verilog::NodekPackageDeclaration, "package");
    scan(verilog::NodekFunctionDeclaration, "function");
    scan(verilog::NodekTaskDeclaration, "task");
    scan(verilog::NodekClassDeclaration, "class");
  }
  return {{"symbols", symbols}};
}

nlohmann::json HandleDebugCountReferences(WorkspaceManager *mgr,
                                          const nlohmann::json &params) {
  if (!params.contains("workspace") || !params["workspace"].is_string() ||
      !params.contains("symbol_name") ||
      !params["symbol_name"].is_string()) {
    return ErrorObj("requires 'workspace' and 'symbol_name' string params");
  }
  const std::string ws = params["workspace"].get<std::string>();
  const std::string sym_name = params["symbol_name"].get<std::string>();
  WorkspaceState *w = mgr->Get(ws);
  if (w == nullptr) return ErrorObj("workspace not open: " + ws);

  nlohmann::json locations = nlohmann::json::array();
  int count = 0;

  for (const auto &[ref_name, file_ptr] : *w->project) {
    if (file_ptr == nullptr) continue;
    file_ptr->Parse().IgnoreError();
    const verible::TextStructureView *ts = file_ptr->GetTextStructure();
    if (ts == nullptr) continue;
    const auto &tree = ts->SyntaxTree();
    if (tree == nullptr) continue;
    const std::string_view content = file_ptr->GetContent();
    const std::string path = std::string(file_ptr->ResolvedPath());

    const auto idents =
        verible::SearchSyntaxTree(*tree, SymbolIdentifierLeaf());
    for (const auto &m : idents) {
      if (m.match == nullptr) continue;
      const auto *leaf =
          dynamic_cast<const verible::SyntaxTreeLeaf *>(m.match);
      if (leaf == nullptr) continue;
      if (leaf->get().text() != sym_name) continue;
      const std::string_view span = verible::StringSpanOfSymbol(*m.match);
      const int offset =
          (span.data() >= content.data() &&
           span.data() < content.data() + content.size())
              ? static_cast<int>(span.data() - content.data())
              : -1;
      ++count;
      locations.push_back({{"file", path},
                           {"byte_offset", offset},
                           {"byte_length", static_cast<int>(span.size())}});
    }
  }
  return {{"count", count}, {"locations", locations}};
}

}  // namespace verilog
