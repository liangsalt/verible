# Seed YAML rules and corpus fixtures for the A1 pattern engine.
#
# These files are NOT compiled. They are bundled as test data so the
# corpus-validator unit test can load every *.yaml and assert its
# examples.bad/examples.good gate passes.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//verible/verilog:__subpackages__"],
)

filegroup(
    name = "all_rules",
    srcs = glob([
        "*.yaml",
        "_corpus/**/*.sv",
        "schema.json",
    ]),
)
