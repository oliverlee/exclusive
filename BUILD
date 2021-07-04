load("@local_config//:defs.bzl", "PROJECT_DEFAULT_COPTS")
load("@local_config//:bazel_info.bzl", "BAZEL_OUTPUT_BASE")
load("@com_grail_compdb//:aspects.bzl", "compilation_database")
load("//tools:macros.bzl", "lint_sources")

exports_files([".clang-tidy"], visibility=["//:__subpackages__"])

cc_library(
    name = "exclusive",
    hdrs = glob([
        "include/exclusive/*.hpp",
    ]),
    copts = PROJECT_DEFAULT_COPTS,
    strip_include_prefix = "include",
    visibility = ["//visibility:public"],
)

compilation_database(
    name = "compdb",
    targets = [
        ":exclusive",
    ],
    exec_root = BAZEL_OUTPUT_BASE + "execroot/__main__",
    testonly = True,
)

lint_sources(
    name = "lint",
    sources = glob(["include/**/*.hpp"]),
    compdb = ":compdb",
)
