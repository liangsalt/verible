# This package contains the YAML-driven pattern engine for runtime-loadable
# Verilog lint rules. Rules are described in YAML files outside the binary,
# compiled at runtime into verible::matcher::Matcher trees, and scanned over
# project source via verible::SearchSyntaxTree.
#
# Layering:
#   matcher-builder    -- name->NodeEnum / TokenEnum registry
#   yaml-rule-loader   -- YAML -> Matcher tree compiler
#   rule-set           -- multi-scope rule container (project/user/team/community)
#   scan-runner        -- iterate project x rules, emit hits with bindings
#   corpus-validator   -- bad.sv must hit, good.sv must not hit
#
# This is the A1.1 skeleton. Implementations land in A1.3-A1.7.

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

default_visibility = [
    "//verible/verilog:__subpackages__",
]

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = default_visibility,
    features = ["layering_check"],
)

cc_library(
    name = "matcher-builder",
    srcs = ["matcher-builder.cc"],
    hdrs = ["matcher-builder.h"],
    deps = [
        "//verible/common/analysis/matcher",
        "//verible/verilog/CST:verilog-nonterminals",
        "@abseil-cpp//absl/container:flat_hash_map",
    ],
)

cc_test(
    name = "matcher-builder_test",
    srcs = ["matcher-builder_test.cc"],
    deps = [
        ":matcher-builder",
        "//verible/verilog/CST:verilog-nonterminals",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "bit-width-resolver",
    srcs = ["bit-width-resolver.cc"],
    hdrs = ["bit-width-resolver.h"],
    deps = [
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:verilog-project",
    ],
)

# condition and semantic-primitives are mutually recursive (condition.cc
# calls FindPrimitive; semantic-primitives.h declares EvalContext via
# condition.h). Ship them as one library.
cc_library(
    name = "condition",
    srcs = [
        "condition.cc",
        "semantic-primitives.cc",
    ],
    hdrs = [
        "condition.h",
        "semantic-primitives.h",
    ],
    deps = [
        ":bit-width-resolver",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/verilog/CST:verilog-matchers",
        "//verible/verilog/CST:verilog-nonterminals",
        "//verible/verilog/analysis:verilog-project",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "yaml-rule-loader",
    srcs = ["yaml-rule-loader.cc"],
    hdrs = ["yaml-rule-loader.h"],
    deps = [
        ":condition",
        ":matcher-builder",
        # condition.h has the ConditionNode AST that LoadedRule holds.
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:inner-match-handlers",
        "//verible/common/text:concrete-syntax-leaf",
        "//verible/common/text:symbol",
        "//verible/common/text:token-info",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@yaml-cpp",
    ],
)

cc_library(
    name = "rule-set",
    srcs = ["rule-set.cc"],
    hdrs = ["rule-set.h"],
    deps = [
        ":yaml-rule-loader",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "scan-runner",
    srcs = ["scan-runner.cc"],
    hdrs = ["scan-runner.h"],
    deps = [
        ":condition",
        ":rule-set",
        ":yaml-rule-loader",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis/matcher",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/common/text:tree-utils",
        "//verible/verilog/analysis:verilog-project",
    ],
)

cc_library(
    name = "corpus-validator",
    srcs = ["corpus-validator.cc"],
    hdrs = ["corpus-validator.h"],
    deps = [
        ":condition",
        ":rule-set",
        ":scan-runner",
        ":yaml-rule-loader",
        "//verible/common/analysis:syntax-tree-search",
        "//verible/common/analysis/matcher:bound-symbol-manager",
        "//verible/common/text:symbol",
        "//verible/common/text:text-structure",
        "//verible/verilog/analysis:verilog-project",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

cc_library(
    name = "workspace-manager",
    srcs = ["workspace-manager.cc"],
    hdrs = ["workspace-manager.h"],
    deps = [
        ":rule-set",
        "//verible/verilog/analysis:verilog-project",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
    ],
)

cc_test(
    name = "corpus-validator_test",
    srcs = ["corpus-validator_test.cc"],
    data = ["//rules:all_rules"],
    deps = [
        ":corpus-validator",
        ":rule-set",
        ":yaml-rule-loader",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
