load("@local_config//:defs.bzl", "PROJECT_DEFAULT_COPTS")
load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
  name = "shared_resource",
  size = "small",
  srcs = ["shared_resource.cpp"],
  copts = PROJECT_DEFAULT_COPTS,
  deps = [
      "//:exclusive",
      "@googletest//:gtest_main",
  ],
)