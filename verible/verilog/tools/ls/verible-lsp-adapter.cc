// Copyright 2021 The Verible Authors.
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
//

#include "verible/verilog/tools/ls/verible-lsp-adapter.h"

#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "nlohmann/json.hpp"
#include "verible/common/analysis/file-analyzer.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/lsp/lsp-protocol-enums.h"
#include "verible/common/lsp/lsp-protocol-operators.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/interval.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/dimensions.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/parameters.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/analysis/verilog-linter.h"
#include "verible/verilog/formatting/format-style-init.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter.h"
#include "verible/verilog/parser/verilog-token-enum.h"
#include "verible/verilog/tools/ls/autoexpand.h"
#include "verible/verilog/tools/ls/document-symbol-filler.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"
#include "verible/verilog/tools/ls/symbol-table-handler.h"

namespace verilog {
// Convert our representation of a linter violation to a LSP-Diagnostic
static verible::lsp::Diagnostic ViolationToDiagnostic(
    const verible::LintViolationWithStatus &v,
    const verible::TextStructureView &text) {
  const verible::LintViolation &violation = *v.violation;
  const verible::LineColumnRange range = text.GetRangeForToken(violation.token);
  const char *fix_msg = violation.autofixes.empty() ? "" : " (fix available)";
  // Map rule severity to LSP DiagnosticSeverity
  const auto lsp_severity =
      (v.status->severity == verible::LintSeverity::kWarning)
          ? verible::lsp::DiagnosticSeverity::kWarning
          : verible::lsp::DiagnosticSeverity::kError;
  return verible::lsp::Diagnostic{
      .range =
          {
              .start = {.line = range.start.line,
                        .character = range.start.column},
              .end = {.line = range.end.line, .character = range.end.column},
          },
      .severity = lsp_severity,
      .has_severity = true,
      .message = absl::StrCat(violation.reason, " ", v.status->url, "[",
                              v.status->lint_rule_name, "]", fix_msg),
  };
}

std::vector<verible::lsp::Diagnostic> CreateDiagnostics(
    const BufferTracker &tracker, int message_limit) {
  // Diagnostics should come from the latest state, including all the
  // syntax errors.
  const auto current = tracker.current();
  if (!current) return {};
  const auto &rejected_tokens = current->parser().GetRejectedTokens();
  auto const &lint_violations =
      verilog::GetSortedViolations(current->lint_result());
  std::vector<verible::lsp::Diagnostic> result;
  int remaining = rejected_tokens.size() + lint_violations.size();

  // TODO: files that generate a lot of messages will create a huge
  // output. So we limit the messages here if "message_limit" is set.
  //
  // We might consider emitting them around the last known
  // edit point in the document as this is what the user sees (if we get
  // individual edits, not full files pushed).
  //
  // TODO(hzeller): to limit repetition, maybe limit the number of messages
  // coming from the _same_ source if we have a "message_limit". So for
  // instance, don't complain on every single line not to use tabs as
  // indentation.
  if (message_limit >= 0 && remaining > message_limit) {
    remaining = message_limit;
  }

  result.reserve(remaining);
  for (const auto &rejected_token : rejected_tokens) {
    if (remaining-- <= 0) break;
    current->parser().ExtractLinterTokenErrorDetail(
        rejected_token,
        [&result, &rejected_token](
            const std::string &filename, verible::LineColumnRange range,
            verible::ErrorSeverity severity, verible::AnalysisPhase phase,
            std::string_view token_text, std::string_view context_line,
            const std::string &msg) {
          std::string message(AnalysisPhaseName(phase));
          absl::StrAppend(&message, " ", ErrorSeverityDescription(severity));
          if (rejected_token.token_info.isEOF()) {
            absl::StrAppend(&message, " (unexpected EOF)");
          } else {
            absl::StrAppend(&message, " at \"", token_text, "\"");
          }
          if (!msg.empty()) {  // Note: msg is often empty and not useful.
            absl::StrAppend(&message, " ", msg);
          }
          result.emplace_back(verible::lsp::Diagnostic{
              .range{.start{.line = range.start.line,
                            .character = range.start.column},
                     .end{.line = range.end.line,  //
                          .character = range.end.column}},
              .severity = severity == verible::ErrorSeverity::kError
                              ? verible::lsp::DiagnosticSeverity::kError
                              : verible::lsp::DiagnosticSeverity::kWarning,
              .has_severity = true,
              .message = message,
          });
        });
  }

  for (const auto &v : lint_violations) {
    if (remaining-- <= 0) break;
    result.emplace_back(ViolationToDiagnostic(v, current->parser().Data()));
  }
  return result;
}

verible::lsp::FullDocumentDiagnosticReport GenerateDiagnosticReport(
    const BufferTracker *tracker,
    const verible::lsp::DocumentDiagnosticParams &p) {
  verible::lsp::FullDocumentDiagnosticReport result;
  if (!tracker) return result;
  result.items = CreateDiagnostics(*tracker, -1);  // no limit in diagnostic msg
  return result;
}

static std::vector<verible::lsp::TextEdit> AutofixToTextEdits(
    const verible::AutoFix &fix, const verible::TextStructureView &text) {
  std::vector<verible::lsp::TextEdit> result;
  // TODO(hzeller): figure out if edits are stacking or are all based
  // on the same start status.
  const std::string_view base = text.Contents();
  for (const verible::ReplacementEdit &edit : fix.Edits()) {
    verible::LineColumn start =
        text.GetLineColAtOffset(edit.fragment.begin() - base.begin());
    verible::LineColumn end =
        text.GetLineColAtOffset(edit.fragment.end() - base.begin());
    result.emplace_back(verible::lsp::TextEdit{
        .range =
            {
                .start = {.line = start.line, .character = start.column},
                .end = {.line = end.line, .character = end.column},
            },
        .newText = edit.replacement,
    });
  }
  return result;
}

std::vector<verible::lsp::CodeAction> GenerateLinterCodeActions(
    const BufferTracker *tracker, const verible::lsp::CodeActionParams &p) {
  std::vector<verible::lsp::CodeAction> result;
  if (!tracker) return result;
  const auto current = tracker->current();
  if (!current) return result;

  auto const &lint_violations =
      verilog::GetSortedViolations(current->lint_result());
  if (lint_violations.empty()) return result;

  const verible::TextStructureView &text = current->parser().Data();

  for (const auto &v : lint_violations) {
    const verible::LintViolation &violation = *v.violation;
    if (violation.autofixes.empty()) continue;
    auto diagnostic = ViolationToDiagnostic(v, text);

    // The editor usually has the cursor on a line or word, so we
    // only want to output edits that are relevant.
    if (!rangeOverlap(diagnostic.range, p.range)) continue;

    bool preferred_fix = true;
    for (const auto &fix : violation.autofixes) {
      result.emplace_back(verible::lsp::CodeAction{
          .title = fix.Description(),
          .kind = "quickfix",
          .diagnostics = {diagnostic},
          .isPreferred = preferred_fix,
          // The following is translated from json, map uri -> edits.
          // We're only sending changes for one document, the current one.
          .edit = {.changes = {{p.textDocument.uri,
                                AutofixToTextEdits(fix,
                                                   current->parser().Data())}}},
      });
      preferred_fix = false;  // only the first is preferred.
    }
  }
  return result;
}

std::vector<verible::lsp::CodeAction> GenerateCodeActions(
    SymbolTableHandler *symbol_table_handler, const BufferTracker *tracker,
    const verible::lsp::CodeActionParams &p) {
  std::vector<verible::lsp::CodeAction> result;

  if (!tracker) return result;
  const auto current = tracker->current();
  if (!current) return result;

  result = GenerateLinterCodeActions(tracker, p);

  auto auto_expand =
      GenerateAutoExpandCodeActions(symbol_table_handler, tracker, p);
  result.insert(result.end(), std::make_move_iterator(auto_expand.begin()),
                make_move_iterator(auto_expand.end()));

  return result;
}

nlohmann::json CreateDocumentSymbolOutline(
    const BufferTracker *tracker, const verible::lsp::DocumentSymbolParams &p,
    bool kate_compatible_tags, bool include_variables) {
  if (!tracker) return nlohmann::json::array();
  // Only if the tree has been fully parsed, it makes sense to create an outline
  const auto last_good = tracker->last_good();
  if (!last_good) return nlohmann::json::array();

  verible::lsp::DocumentSymbol toplevel;
  const auto &text_structure = last_good->parser().Data();
  verilog::DocumentSymbolFiller filler(kate_compatible_tags, include_variables,
                                       text_structure, &toplevel);
  const auto &syntax_tree = text_structure.SyntaxTree();
  syntax_tree->Accept(&filler);
  // We cut down one level, not interested in toplevel file:
  return toplevel.children;
}

std::vector<verible::lsp::DocumentHighlight> CreateHighlightRanges(
    const BufferTracker *tracker,
    const verible::lsp::DocumentHighlightParams &p) {
  std::vector<verible::lsp::DocumentHighlight> result;
  if (!tracker) return result;
  const auto current = tracker->current();
  if (!current) return result;
  const verible::LineColumn cursor{p.position.line, p.position.character};
  const verible::TextStructureView &text = current->parser().Data();

  const verible::TokenInfo cursor_token = text.FindTokenAt(cursor);
  if (cursor_token.token_enum() != SymbolIdentifier) return result;

  // Find all the symbols with the same name in the buffer.
  // Note, this is very simplistic as it does _not_ take scopes into account.
  // For that, we'd need the symbol table, but that implementation is not
  // complete yet.
  for (const verible::TokenInfo &tok : text.TokenStream()) {
    if (tok.token_enum() != cursor_token.token_enum()) continue;
    if (tok.text() != cursor_token.text()) continue;
    const verible::LineColumnRange range = text.GetRangeForToken(tok);
    result.push_back(verible::lsp::DocumentHighlight{
        .range = {
            .start = {.line = range.start.line,
                      .character = range.start.column},
            .end = {.line = range.end.line, .character = range.end.column},
        }});
  }

  return result;
}

std::vector<verible::lsp::TextEdit> FormatRange(
    const BufferTracker *tracker,
    const verible::lsp::DocumentFormattingParams &p) {
  std::vector<verible::lsp::TextEdit> result;
  if (!tracker) return result;
  const auto current = tracker->current();

  // Can only format if we have latest version and it could be parsed.
  if (!current || !current->parsed_successfully()) return result;

  const verible::TextStructureView &text = current->parser().Data();
  verilog::formatter::FormatStyle format_style;
  verilog::formatter::InitializeFromFlags(&format_style);

  if (p.has_range) {
    // If the cursor is at the very beginning of last line, we don't include
    // it in the formatting.
    const int last_line_include = p.range.end.character > 0 ? 1 : 0;
    const verible::Interval<int> format_lines{
        p.range.start.line + 1,  // 1 index based
        p.range.end.line + 1 + last_line_include};
    if (!format_lines.valid()) return result;
    std::string formatted_range;
    if (!FormatVerilogRange(text, format_style, &formatted_range, format_lines)
             .ok()) {
      return result;
    }
    result.push_back(verible::lsp::TextEdit{
        .range =
            {
                .start = {.line = format_lines.min - 1, .character = 0},
                .end = {.line = format_lines.max - 1, .character = 0},
            },
        .newText = formatted_range});
  } else {
    std::string newText;
    if (!FormatVerilog(text, current->uri(), format_style, &newText).ok()) {
      return result;
    }
    // Emit a single edit that replaces the full range the file covers.
    // TODO(hzeller): Could consider patches maybe.
    // TODO(hzeller): Also be safe and don't emit anything if text is the same.
    const auto range = text.GetRangeForText(text.Contents());
    result.push_back(verible::lsp::TextEdit{
        .range =
            {
                .start = {.line = 0, .character = 0},
                .end = {.line = range.end.line, .character = range.end.column},
            },
        .newText = newText});
  }
  return result;
}

// Helper function to extract width string from a symbol (expression or number)
static std::string ExtractExpressionText(const verible::Symbol *symbol,
                                         const verible::TextStructureView &text) {
  if (!symbol) return "";

  // Get the text range for this symbol and extract the actual text
  const auto &content = text.Contents();

  // For leaf nodes, get the text directly
  if (symbol->Kind() == verible::SymbolKind::kLeaf) {
    const auto &leaf = verible::SymbolCastToLeaf(*symbol);
    return std::string(leaf.get().text());
  }

  // For node, try to reconstruct from children
  const auto &node = verible::SymbolCastToNode(*symbol);
  std::string result;
  for (const auto &child : node.children()) {
    if (child) {
      result += ExtractExpressionText(child.get(), text);
    }
  }
  return result;
}

// Helper to get packed dimensions string from a port declaration
static std::string GetPackedDimensionsFromPortDeclaration(
    const verible::Symbol &port_decl, const verible::TextStructureView &text) {
  // Port declaration structure:
  // Index 0: Direction (input/output/inout)
  // Index 1: Net type or data type info
  // Index 2: Data type (may contain packed dimensions)
  // Index 3: Identifier

  const auto *data_type = verible::GetSubtreeAsSymbol(
      port_decl, NodeEnum::kPortDeclaration, 2);

  if (!data_type) {
    // Try index 1 for some port declaration formats
    data_type = verible::GetSubtreeAsSymbol(
        port_decl, NodeEnum::kPortDeclaration, 1);
  }

  if (!data_type) return "1";

  // Search for packed dimensions within the data type
  auto packed_dims = verilog::FindAllPackedDimensions(*data_type);
  if (packed_dims.empty()) return "1";

  // Get the dimension range
  for (const auto &dim : packed_dims) {
    auto decl_dims = verilog::FindAllDeclarationDimensions(*dim.match);
    for (const auto &decl_dim : decl_dims) {
      // Find kDimensionRange nodes
      const auto *node = &verible::SymbolCastToNode(*decl_dim.match);
      for (const auto &child : node->children()) {
        if (!child) continue;
        if (child->Kind() != verible::SymbolKind::kNode) continue;
        const auto &child_node = verible::SymbolCastToNode(*child);
        if (child_node.MatchesTag(NodeEnum::kDimensionRange)) {
          // Extract [high:low] format
          const auto *left = verilog::GetDimensionRangeLeftBound(*child);
          const auto *right = verilog::GetDimensionRangeRightBound(*child);
          if (left && right) {
            std::string left_str = ExtractExpressionText(left, text);
            std::string right_str = ExtractExpressionText(right, text);
            return "[" + left_str + ":" + right_str + "]";
          }
        }
      }
    }
  }

  return "1";
}

nlohmann::json GetModulePorts(const BufferTracker *tracker,
                               const std::string &uri) {
  nlohmann::json result = nlohmann::json::array();

  if (!tracker) return result;

  const auto last_good = tracker->last_good();
  if (!last_good) return result;

  const auto &text_structure = last_good->parser().Data();
  const auto &syntax_tree = text_structure.SyntaxTree();
  if (!syntax_tree) return result;

  // Find all module declarations
  auto modules = verilog::FindAllModuleDeclarations(*syntax_tree);

  for (const auto &module_match : modules) {
    const verible::Symbol *module_symbol = module_match.match;
    nlohmann::json module_json;

    // Get module name
    const auto *module_name_leaf = verilog::GetModuleName(*module_symbol);
    if (!module_name_leaf) continue;

    module_json["name"] = std::string(module_name_leaf->get().text());
    module_json["ports"] = nlohmann::json::array();

    // Get port declaration list
    const auto *port_list = verilog::GetModulePortDeclarationList(*module_symbol);
    if (port_list) {
      // ANSI-style ports in declaration list
      auto port_decls = verilog::FindAllPortDeclarations(*port_list);
      for (const auto &port_match : port_decls) {
        nlohmann::json port_json;

        // Get port name
        const auto *port_name = verilog::GetIdentifierFromPortDeclaration(
            *port_match.match);
        if (!port_name) continue;

        port_json["name"] = std::string(port_name->get().text());

        // Get port direction
        const auto *direction = verilog::GetDirectionFromPortDeclaration(
            *port_match.match);
        if (direction) {
          port_json["direction"] = std::string(direction->get().text());
        } else {
          port_json["direction"] = "input";  // default
        }

        // Get port width
        port_json["width"] = GetPackedDimensionsFromPortDeclaration(
            *port_match.match, text_structure);

        module_json["ports"].push_back(port_json);
      }
    }

    // Also check module port paren group for ports
    const auto *port_paren = verilog::GetModulePortParenGroup(*module_symbol);
    if (port_paren && module_json["ports"].empty()) {
      // Try to find port declarations directly in the paren group
      auto port_decls = verilog::FindAllPortDeclarations(*port_paren);
      for (const auto &port_match : port_decls) {
        nlohmann::json port_json;

        const auto *port_name = verilog::GetIdentifierFromPortDeclaration(
            *port_match.match);
        if (!port_name) continue;

        port_json["name"] = std::string(port_name->get().text());

        const auto *direction = verilog::GetDirectionFromPortDeclaration(
            *port_match.match);
        if (direction) {
          port_json["direction"] = std::string(direction->get().text());
        } else {
          port_json["direction"] = "input";
        }

        port_json["width"] = GetPackedDimensionsFromPortDeclaration(
            *port_match.match, text_structure);

        module_json["ports"].push_back(port_json);
      }

      // Also check for non-ANSI port references
      if (module_json["ports"].empty()) {
        auto port_refs = verilog::FindAllPortReferences(*port_paren);
        for (const auto &port_ref : port_refs) {
          const auto *port_ref_node = verilog::GetPortReferenceFromPort(
              *port_ref.match);
          if (!port_ref_node) continue;

          const auto *port_name = verilog::GetIdentifierFromPortReference(
              *port_ref_node);
          if (!port_name) continue;

          nlohmann::json port_json;
          port_json["name"] = std::string(port_name->get().text());
          port_json["direction"] = "unknown";  // Non-ANSI style
          port_json["width"] = "1";

          module_json["ports"].push_back(port_json);
        }
      }
    }

    result.push_back(module_json);
  }

  return result;
}

nlohmann::json GetModuleInfo(const BufferTracker *tracker,
                              const std::string &uri) {
  nlohmann::json result = nlohmann::json::array();

  if (!tracker) return result;

  const auto last_good = tracker->last_good();
  if (!last_good) return result;

  const auto &text_structure = last_good->parser().Data();
  const auto &syntax_tree = text_structure.SyntaxTree();
  if (!syntax_tree) return result;

  const auto &line_column_map = text_structure.GetLineColumnMap();

  // Find all module declarations
  auto modules = verilog::FindAllModuleDeclarations(*syntax_tree);

  for (const auto &module_match : modules) {
    const verible::Symbol *module_symbol = module_match.match;
    nlohmann::json module_json;

    // Get module name
    const auto *module_name_leaf = verilog::GetModuleName(*module_symbol);
    if (!module_name_leaf) continue;

    module_json["name"] = std::string(module_name_leaf->get().text());

    // Get module range
    const auto &module_token = module_name_leaf->get();
    const auto &content = text_structure.Contents();
    const auto module_start = line_column_map.GetLineColAtOffset(
        content, module_token.left(content));

    // Find endmodule position by traversing the tree
    // For now, use the module declaration span
    auto module_range = verible::StringSpanOfSymbol(*module_symbol);
    const int end_offset = module_range.data() + module_range.length() - content.data();
    const auto module_end = line_column_map.GetLineColAtOffset(content, end_offset);

    module_json["range"] = {
        {"start", {{"line", module_start.line}, {"character", module_start.column}}},
        {"end", {{"line", module_end.line}, {"character", module_end.column}}}
    };

    // Get ports (reuse existing logic)
    module_json["ports"] = nlohmann::json::array();
    const auto *port_list = verilog::GetModulePortDeclarationList(*module_symbol);
    if (port_list) {
      auto port_decls = verilog::FindAllPortDeclarations(*port_list);
      for (const auto &port_match : port_decls) {
        nlohmann::json port_json;
        const auto *port_name = verilog::GetIdentifierFromPortDeclaration(*port_match.match);
        if (!port_name) continue;

        port_json["name"] = std::string(port_name->get().text());
        const auto *direction = verilog::GetDirectionFromPortDeclaration(*port_match.match);
        port_json["direction"] = direction ? std::string(direction->get().text()) : "input";
        port_json["width"] = GetPackedDimensionsFromPortDeclaration(*port_match.match, text_structure);
        module_json["ports"].push_back(port_json);
      }
    }

    // Also check module port paren group
    const auto *port_paren = verilog::GetModulePortParenGroup(*module_symbol);
    if (port_paren && module_json["ports"].empty()) {
      auto port_decls = verilog::FindAllPortDeclarations(*port_paren);
      for (const auto &port_match : port_decls) {
        nlohmann::json port_json;
        const auto *port_name = verilog::GetIdentifierFromPortDeclaration(*port_match.match);
        if (!port_name) continue;

        port_json["name"] = std::string(port_name->get().text());
        const auto *direction = verilog::GetDirectionFromPortDeclaration(*port_match.match);
        port_json["direction"] = direction ? std::string(direction->get().text()) : "input";
        port_json["width"] = GetPackedDimensionsFromPortDeclaration(*port_match.match, text_structure);
        module_json["ports"].push_back(port_json);
      }
    }

    // Get parameters
    module_json["parameters"] = nlohmann::json::array();
    auto param_decls = verilog::FindAllParamDeclarations(*module_symbol);
    for (const auto &param_match : param_decls) {
      nlohmann::json param_json;

      // Get parameter keyword (parameter or localparam)
      auto param_keyword = verilog::GetParamKeyword(*param_match.match);
      param_json["type"] = (param_keyword == verilog_tokentype::TK_localparam) ? "localparam" : "parameter";

      // Get parameter name
      const auto *param_name_token = verilog::GetParameterNameToken(*param_match.match);
      if (!param_name_token) continue;
      param_json["name"] = std::string(param_name_token->text());

      // Get parameter value expression
      const auto *param_expr = verilog::GetParamAssignExpression(*param_match.match);
      if (param_expr) {
        auto expr_span = verible::StringSpanOfSymbol(*param_expr);
        param_json["value"] = std::string(expr_span);
      } else {
        param_json["value"] = "";
      }

      // Get parameter position
      const auto param_pos = line_column_map.GetLineColAtOffset(
          content, param_name_token->left(content));
      param_json["line"] = param_pos.line;

      module_json["parameters"].push_back(param_json);
    }

    // Get instantiations
    module_json["instantiations"] = nlohmann::json::array();

    // Get module item list (body of module)
    const auto *module_items = verilog::GetModuleItemList(*module_symbol);
    if (module_items) {
      auto data_decls = verilog::FindAllDataDeclarations(*module_items);
      for (const auto &data_match : data_decls) {
        // Get instantiation type (the module being instantiated)
        const auto *inst_type = verilog::GetInstantiationTypeOfDataDeclaration(*data_match.match);
        if (!inst_type) continue;

        // Get the type identifier (module name)
        const auto *type_id = verilog::GetTypeIdentifierFromDataDeclaration(*data_match.match);
        if (!type_id) continue;

        auto type_span = verible::StringSpanOfSymbol(*type_id);
        std::string module_type_name(type_span);

        // Skip if it looks like a built-in type
        if (module_type_name == "reg" || module_type_name == "wire" ||
            module_type_name == "logic" || module_type_name == "integer" ||
            module_type_name == "real" || module_type_name == "time") {
          continue;
        }

        // Get instance list
        const auto *inst_list = verilog::GetInstanceListFromDataDeclaration(*data_match.match);
        if (!inst_list) continue;

        auto gate_instances = verilog::FindAllGateInstances(*inst_list);
        for (const auto &gate_match : gate_instances) {
          nlohmann::json inst_json;
          inst_json["moduleName"] = module_type_name;

          // Get instance name
          const auto *inst_name_token = verilog::GetModuleInstanceNameTokenInfoFromGateInstance(*gate_match.match);
          if (inst_name_token) {
            inst_json["instanceName"] = std::string(inst_name_token->text());

            // Get instance position
            const auto inst_pos = line_column_map.GetLineColAtOffset(
                content, inst_name_token->left(content));
            inst_json["line"] = inst_pos.line;
          } else {
            inst_json["instanceName"] = "";
            inst_json["line"] = 0;
          }

          module_json["instantiations"].push_back(inst_json);
        }
      }
    }

    result.push_back(module_json);
  }

  return result;
}

nlohmann::json GetAllModuleInfo(const BufferTrackerContainer &parsed_buffers) {
  nlohmann::json result = nlohmann::json::object();

  // Get all tracked URIs
  std::vector<std::string> all_uris = parsed_buffers.GetAllUris();

  for (const std::string &uri : all_uris) {
    const BufferTracker *tracker = parsed_buffers.FindBufferTrackerOrNull(uri);
    if (tracker) {
      nlohmann::json module_info = GetModuleInfo(tracker, uri);
      if (!module_info.empty()) {
        result[uri] = module_info;
      }
    }
  }

  return result;
}

}  // namespace verilog
