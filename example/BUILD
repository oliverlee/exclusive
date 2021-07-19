load("@local_config//:defs.bzl", "PROJECT_DEFAULT_COPTS")
load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
  name = "counter",
  srcs = ["example.cpp"],
  copts = PROJECT_DEFAULT_COPTS,
  deps = ["//:exclusive"],
  linkopts = ["-lpthread"],
)
