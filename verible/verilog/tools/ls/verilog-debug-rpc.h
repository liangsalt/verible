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

// Handler functions for the verilog/debug/* and verilog/workspace/* JSON-RPC
// methods used by the RTL Brain platform. Each handler receives the raw
// request params as nlohmann::json and returns nlohmann::json-serializable
// results. WorkspaceManager state is shared across all handlers via the
// pointer passed in from VerilogLanguageServer.

#ifndef VERIBLE_VERILOG_TOOLS_LS_VERILOG_DEBUG_RPC_H_
#define VERIBLE_VERILOG_TOOLS_LS_VERILOG_DEBUG_RPC_H_

#include "nlohmann/json.hpp"
#include "verible/verilog/analysis/pattern-engine/workspace-manager.h"

namespace verilog {

// verilog/workspace/open
// Params: { root: string, scope_paths?: { project, user, team, community },
//           extra_source_dirs?: [string] }
// Result: { ok: bool, error?: string, load_errors?: [string] }
nlohmann::json HandleWorkspaceOpen(
    analysis::pattern_engine::WorkspaceManager *mgr,
    const nlohmann::json &params);

// verilog/workspace/close
// Params: { root: string }
// Result: { ok: bool, error?: string }
nlohmann::json HandleWorkspaceClose(
    analysis::pattern_engine::WorkspaceManager *mgr,
    const nlohmann::json &params);

// verilog/workspace/list
// Params: {}
// Result: { workspaces: [string] }
nlohmann::json HandleWorkspaceList(
    analysis::pattern_engine::WorkspaceManager *mgr,
    const nlohmann::json &params);

// verilog/debug/scanRules
// Params: { workspace: string, ruleIds?: [string], source_files?: [string] }
//   source_files: optional list of SV files to open in the project before
//   scanning. If absent, the scan runs only on already-opened files.
// Result: { hits: [{rule, file, byte_offset, byte_length, bindings: {...}}] }
nlohmann::json HandleDebugScanRules(
    analysis::pattern_engine::WorkspaceManager *mgr,
    const nlohmann::json &params);

// verilog/debug/reloadRules
// Params: { workspace: string }
// Result: { ok: bool, rule_count: int, load_errors: [string] }
nlohmann::json HandleDebugReloadRules(
    analysis::pattern_engine::WorkspaceManager *mgr,
    const nlohmann::json &params);

// verilog/debug/validateRule
// Params: { workspace: string, ruleId: string }
// Result: { passed: bool, bad_hit: bool, good_hit: bool,
//           bad_diagnostics: [string], good_diagnostics: [string] }
nlohmann::json HandleDebugValidateRule(
    analysis::pattern_engine::WorkspaceManager *mgr,
    const nlohmann::json &params);

// verilog/debug/listRules
// Params: { workspace: string }
// Result: { rules: [{name, topic, severity, source_scope, source_path,
//                    why_bad, suggested_fix}] }
nlohmann::json HandleDebugListRules(
    analysis::pattern_engine::WorkspaceManager *mgr,
    const nlohmann::json &params);

// verilog/debug/getCstNode
// Params: { workspace: string, file: string,
//           max_depth?: int (default 3) }
// Result: { tag, byte_offset, byte_length, children: [...] }
// Returns the parsed CST of the named source file truncated to max_depth.
nlohmann::json HandleDebugGetCstNode(
    analysis::pattern_engine::WorkspaceManager *mgr,
    const nlohmann::json &params);

// verilog/debug/listSymbols
// Params: { workspace: string }
// Result: { symbols: [{name, kind, file, byte_offset, byte_length}] }
// Enumerates module / function / task / package declarations across the
// workspace's open files.
nlohmann::json HandleDebugListSymbols(
    analysis::pattern_engine::WorkspaceManager *mgr,
    const nlohmann::json &params);

// verilog/debug/countReferences
// Params: { workspace: string, symbol_name: string }
// Result: { count: int, locations: [{file, byte_offset, byte_length}] }
// Cheap textual identifier match across all parsed files. Not a true
// def/use resolution -- that comes with A3's signal-graph primitive.
nlohmann::json HandleDebugCountReferences(
    analysis::pattern_engine::WorkspaceManager *mgr,
    const nlohmann::json &params);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_LS_VERILOG_DEBUG_RPC_H_
