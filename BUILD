load("@local_config//:defs.bzl", "PROJECT_DEFAULT_COPTS")

exports_files([".clang-tidy"], visibility=["//:__subpackages__"])

cc_library(
    name = "exclusive",
    hdrs = [
        "include/exclusive/exclusive.hpp",
        "include/exclusive/mutex.hpp",
    ],
    copts = PROJECT_DEFAULT_COPTS,
    strip_include_prefix = "include",
    visibility = ["//visibility:public"],
)