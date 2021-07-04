load("@local_config//:defs.bzl", "PROJECT_DEFAULT_COPTS")

cc_binary(
    name = "exclusive_dummy",
    srcs = [
        "exclusive.cpp",
    ],
    copts = PROJECT_DEFAULT_COPTS,
    deps = [
        "//:exclusive",
    ],
)
