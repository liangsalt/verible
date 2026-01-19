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

#include "verible/verilog/analysis/checkers/gjb-10157-r-2-3-rule.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <string_view>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/class.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/functions.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/net.h"
#include "verible/verilog/CST/package.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/tasks.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeLeaf;

// Register the lint rule.
VERILOG_REGISTER_LINT_RULE(Gjb10157R23Rule);

namespace {

// Verilog/SystemVerilog keywords
const std::set<std::string> kVerilogKeywords = {
    "always", "and", "assign", "automatic", "begin", "buf", "bufif0", "bufif1",
    "case", "casex", "casez", "cell", "cmos", "config", "deassign", "default",
    "defparam", "design", "disable", "edge", "else", "end", "endcase",
    "endconfig", "endfunction", "endgenerate", "endmodule", "endprimitive",
    "endspecify", "endtable", "endtask", "event", "for", "force", "forever",
    "fork", "function", "generate", "genvar", "highz0", "highz1", "if",
    "ifnone", "incdir", "include", "initial", "inout", "input", "instance",
    "integer", "join", "large", "liblist", "library", "localparam", "macromodule",
    "medium", "module", "nand", "negedge", "nmos", "nor", "noshowcancelled",
    "not", "notif0", "notif1", "or", "output", "parameter", "pmos", "posedge",
    "primitive", "pull0", "pull1", "pulldown", "pullup", "pulsestyle_ondetect",
    "pulsestyle_onevent", "rcmos", "real", "realtime", "reg", "release",
    "repeat", "rnmos", "rpmos", "rtran", "rtranif0", "rtranif1", "scalared",
    "showcancelled", "signed", "small", "specify", "specparam", "strong0",
    "strong1", "supply0", "supply1", "table", "task", "time", "tran", "tranif0",
    "tranif1", "tri", "tri0", "tri1", "triand", "trior", "trireg", "unsigned",
    "use", "uwire", "vectored", "wait", "wand", "weak0", "weak1", "while",
    "wire", "wor", "xnor", "xor",
    // SystemVerilog additional keywords
    "accept_on", "alias", "always_comb", "always_ff", "always_latch", "assert",
    "assume", "before", "bind", "bins", "binsof", "bit", "break", "byte",
    "chandle", "checker", "class", "clocking", "const", "constraint", "context",
    "continue", "cover", "covergroup", "coverpoint", "cross", "dist", "do",
    "endchecker", "endclass", "endclocking", "endgroup", "endinterface",
    "endpackage", "endprogram", "endproperty", "endsequence", "enum", "eventually",
    "expect", "export", "extends", "extern", "final", "first_match", "foreach",
    "forkjoin", "global", "iff", "ignore_bins", "illegal_bins", "implements",
    "implies", "import", "inside", "int", "interconnect", "interface", "intersect",
    "join_any", "join_none", "let", "local", "logic", "longint", "matches",
    "modport", "new", "nexttime", "null", "package", "packed", "priority",
    "program", "property", "protected", "pure", "rand", "randc", "randcase",
    "randsequence", "ref", "reject_on", "restrict", "return", "s_always",
    "s_eventually", "s_nexttime", "s_until", "s_until_with", "sequence",
    "shortint", "shortreal", "solve", "static", "string", "strong", "struct",
    "super", "sync_accept_on", "sync_reject_on", "tagged", "this", "throughout",
    "timeprecision", "timeunit", "type", "typedef", "union", "unique", "unique0",
    "until", "until_with", "untyped", "var", "virtual", "void", "wait_order",
    "weak", "wildcard", "with", "within"
};

// VHDL keywords
const std::set<std::string> kVhdlKeywords = {
    "abs", "access", "after", "alias", "all", "and", "architecture", "array",
    "assert", "assume", "attribute", "begin", "block", "body", "buffer", "bus",
    "case", "component", "configuration", "constant", "context", "cover",
    "default", "disconnect", "downto", "else", "elsif", "end", "entity", "exit",
    "fairness", "file", "for", "force", "function", "generate", "generic",
    "group", "guarded", "if", "impure", "in", "inertial", "inout", "is",
    "label", "library", "linkage", "literal", "loop", "map", "mod", "nand",
    "new", "next", "nor", "not", "null", "of", "on", "open", "or", "others",
    "out", "package", "parameter", "port", "postponed", "private", "procedure",
    "process", "property", "protected", "pure", "range", "record", "register",
    "reject", "release", "rem", "report", "restrict", "return", "rol", "ror",
    "select", "sequence", "severity", "shared", "signal", "sla", "sll", "sra",
    "srl", "strong", "subtype", "then", "to", "transport", "type", "unaffected",
    "units", "until", "use", "variable", "view", "vmode", "vpkg", "vprop",
    "vunit", "wait", "when", "while", "with", "xnor", "xor"
};

// SDF (Standard Delay Format) keywords
const std::set<std::string> kSdfKeywords = {
    "absolute", "cell", "celltype", "cond", "condelse", "date", "delay",
    "delayfile", "design", "device", "divider", "hold", "increment", "instance",
    "interconnect", "iopath", "name", "netdelay", "nochange", "pathpulse",
    "pathpulsepercent", "period", "port", "process", "program", "recovery",
    "recrem", "removal", "retain", "sdfversion", "setup", "setuphold", "skew",
    "temperature", "timescale", "timingcheck", "timingenv", "vendor", "version",
    "voltage", "width"
};

// EDIF (Electronic Design Interchange Format) keywords
const std::set<std::string> kEdifKeywords = {
    "acload", "after", "annotate", "apply", "arc", "array", "arraymacro",
    "arrayrelatedinfo", "arraysite", "atleast", "atmost", "author", "becomes",
    "between", "boolean", "booleandisplay", "booleanmap", "borderpattern",
    "borderwidth", "boundingbox", "cell", "cellref", "celltype", "change",
    "circle", "color", "comment", "commentgraphics", "compound", "connectlocation",
    "contents", "cornertype", "criticality", "currentmap", "curve", "cycle",
    "dataorigin", "dcfaninload", "dcfanoutload", "dcmaxfanin", "dcmaxfanout",
    "delay", "delta", "derivation", "design", "designator", "difference",
    "direction", "display", "dominates", "dot", "duration", "e", "edif",
    "ediflevel", "edifversion", "enclosuredistance", "endtype", "entry", "event",
    "exactly", "external", "fabricate", "false", "figure", "figurearea",
    "figuregroup", "figuregroupobject", "figuregroupoverride", "figuregroupref",
    "figureperimeter", "figurewidth", "fillpattern", "follow", "forbiddenevent",
    "globalportref", "greaterthan", "gridmap", "ignore", "includefiguregroup",
    "initial", "instance", "instancebackannotate", "instancegroup", "instancemap",
    "instanceref", "integer", "integerdisplay", "interface", "interfiguregroupspacing",
    "intersection", "intrafiguregroupspacing", "inverse", "isolated", "joined",
    "justify", "keywordalias", "keywordlevel", "keywordmap", "lessthan", "library",
    "libraryref", "listofnets", "listofports", "loaddelay", "logicassign",
    "logicinput", "logiclist", "logicmapinput", "logicmapoutput", "logiconeof",
    "logicoutput", "logicport", "logicref", "logicvalue", "logicwaveform",
    "maintain", "match", "member", "minomax", "minomaxdisplay", "mnm", "multiplevalueset",
    "mustjoin", "name", "net", "netbackannotate", "netbundle", "netdelay", "netgroup",
    "netmap", "netref", "nochange", "nonpermutable", "notallowed", "notchspacing",
    "number", "numberdefinition", "numberdisplay", "offpageconnector", "offsetevent",
    "openshape", "orientation", "origin", "overhangdistance", "overlapdistance",
    "oversize", "owner", "page", "pagesize", "parameter", "parameterassign",
    "parameterdisplay", "path", "pathdelay", "pathwidth", "permutable", "physicaldesignrule",
    "plug", "point", "pointdisplay", "pointlist", "polygon", "port", "portbackannotate",
    "portbundle", "portdelay", "portgroup", "portimplementation", "portinstance",
    "portlist", "portlistalias", "portmap", "portref", "program", "property",
    "propertydisplay", "protectionframe", "pt", "rangevector", "rectangle",
    "rectanglesize", "rename", "resolves", "scale", "scalex", "scaley", "section",
    "shape", "simulate", "simulationinfo", "singlevalueset", "site", "socket",
    "socketset", "status", "steady", "string", "stringdisplay", "strong", "symbol",
    "symmetry", "table", "tabledefault", "technology", "textheight", "timeinterval",
    "timestamp", "timing", "transform", "transition", "trigger", "true", "unconstrained",
    "undefined", "union", "unit", "unused", "userdata", "version", "view", "viewlist",
    "viewmap", "viewref", "viewtype", "visible", "voltagemap", "wavevalue", "weak",
    "weakjoined", "when", "written"
};

}  // namespace

const LintRuleDescriptor &Gjb10157R23Rule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "GJB-10157-R-2-3",
      .topic = "identifiers",
      .desc =
          "Checks that identifiers are not the same as reserved keywords from "
          "Verilog, SystemVerilog, VHDL, SDF, or EDIF (case-insensitive). "
          "[GJB 10157 Rule 2-3]",
  };
  return d;
}

absl::Status Gjb10157R23Rule::Configure(std::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(
      "This rule does not accept any configuration.");
}

std::string Gjb10157R23Rule::IsReservedKeyword(std::string_view name) {
  // Convert to lowercase for case-insensitive comparison
  std::string lower_name = absl::AsciiStrToLower(name);

  if (kVerilogKeywords.count(lower_name)) {
    return "Verilog/SystemVerilog";
  }
  if (kVhdlKeywords.count(lower_name)) {
    return "VHDL";
  }
  if (kSdfKeywords.count(lower_name)) {
    return "SDF";
  }
  if (kEdifKeywords.count(lower_name)) {
    return "EDIF";
  }

  return "";  // Not a reserved keyword
}

void Gjb10157R23Rule::Lint(const verible::TextStructureView &text_structure,
                           std::string_view filename) {
  const auto &tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Helper lambda to check identifier and add violation if it's a keyword
  auto check_identifier = [&](const verible::TokenInfo &token,
                              const verible::SyntaxTreeContext &context,
                              std::string_view kind) {
    std::string_view name = token.text();
    std::string language = IsReservedKeyword(name);
    if (!language.empty()) {
      std::string reason = absl::StrCat(
          kind, " name '", name, "' conflicts with ", language,
          " keyword. [GJB 10157 R-2-3]");
      violations_.insert(LintViolation(token, reason, context));
    }
  };

  // Check module names
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekModuleDeclaration())) {
    const auto *name_leaf = GetModuleName(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Module");
    }
  }

  // Check interface names
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekInterfaceDeclaration())) {
    const auto *name_token = GetInterfaceNameToken(*match.match);
    if (name_token != nullptr) {
      check_identifier(*name_token, match.context, "Interface");
    }
  }

  // Check package names
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekPackageDeclaration())) {
    const auto *name_token = GetPackageNameToken(*match.match);
    if (name_token != nullptr) {
      check_identifier(*name_token, match.context, "Package");
    }
  }

  // Check function names
  for (const auto &match :
       SearchSyntaxTree(*tree, NodekFunctionDeclaration())) {
    const auto *name_leaf = GetFunctionName(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Function");
    }
  }

  // Check task names
  for (const auto &match : SearchSyntaxTree(*tree, NodekTaskDeclaration())) {
    const auto *name_leaf = GetTaskName(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Task");
    }
  }

  // Check class names
  for (const auto &match : SearchSyntaxTree(*tree, NodekClassDeclaration())) {
    const auto *name_leaf = GetClassName(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Class");
    }
  }

  // Check variable/signal names (register variables: reg, logic, etc.)
  for (const auto &match : SearchSyntaxTree(*tree, NodekRegisterVariable())) {
    const auto *name_token =
        GetInstanceNameTokenInfoFromRegisterVariable(*match.match);
    if (name_token != nullptr) {
      check_identifier(*name_token, match.context, "Variable");
    }
  }

  // Check net/wire names (wire, tri, etc.)
  for (const auto &match : SearchSyntaxTree(*tree, NodekNetVariable())) {
    const auto *name_leaf = GetNameLeafOfNetVariable(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Wire");
    }
  }

  // Check module port declarations (non-ANSI style: module foo(a); input a;)
  for (const auto &match : SearchSyntaxTree(*tree, NodekModulePortDeclaration())) {
    const auto *name_leaf = GetIdentifierFromModulePortDeclaration(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Port");
    }
  }

  // Check port declarations (ANSI style: module foo(input a);)
  for (const auto &match : SearchSyntaxTree(*tree, NodekPortDeclaration())) {
    const auto *name_leaf = GetIdentifierFromPortDeclaration(*match.match);
    if (name_leaf != nullptr) {
      check_identifier(name_leaf->get(), match.context, "Port");
    }
  }

  // Check instance names (gate instances)
  for (const auto &match : SearchSyntaxTree(*tree, NodekGateInstance())) {
    const auto *name_token =
        GetModuleInstanceNameTokenInfoFromGateInstance(*match.match);
    if (name_token != nullptr) {
      check_identifier(*name_token, match.context, "Instance");
    }
  }
}

verible::LintRuleStatus Gjb10157R23Rule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
