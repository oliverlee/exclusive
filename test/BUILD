load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
  name = "shared_resource",
  size = "small",
  srcs = ["shared_resource.cpp"],
  deps = [
      "//:exclusive",
      "@googletest//:gtest_main",
  ],
)