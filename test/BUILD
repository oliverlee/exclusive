load("@local_config//:defs.bzl", "PROJECT_DEFAULT_COPTS")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
  name = "fake_clock",
  hdrs = ["include/exclusive/test/fake_clock.hpp"],
  srcs = ["src/fake_clock.cpp"],
  copts = PROJECT_DEFAULT_COPTS,
  strip_include_prefix = "include",
  testonly = True,
  visibility = ["__subpackages__"],
)

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

cc_test(
  name = "clh",
  size = "small",
  srcs = ["clh.cpp"],
  copts = PROJECT_DEFAULT_COPTS,
  deps = [
      "//:exclusive",
      ":fake_clock",
      "@googletest//:gtest_main",
  ],
)

cc_test(
  name = "clh_walltime",
  size = "small",
  srcs = ["clh_walltime.cpp"],
  copts = PROJECT_DEFAULT_COPTS,
  deps = [
      "//:exclusive",
      "@googletest//:gtest_main",
  ],
)