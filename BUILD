load("@local_config//:defs.bzl", "PROJECT_DEFAULT_COPTS")

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