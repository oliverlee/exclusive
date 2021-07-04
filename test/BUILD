load("@local_config//:bazel_info.bzl", "BAZEL_OUTPUT_BASE")
load("@com_grail_compdb//:aspects.bzl", "compilation_database")
load("//tools:macros.bzl", "lint_sources")
load("@rules_cc//cc:defs.bzl", "cc_test")
load("@local_config//:defs.bzl", "PROJECT_DEFAULT_COPTS")

cc_test(
  name = "shared_resource",
  size = "small",
  srcs = ["shared_resource.cpp"],
  deps = [
      "//:exclusive",
      "@googletest//:gtest_main",
  ],
)