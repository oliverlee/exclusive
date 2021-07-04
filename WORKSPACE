load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(
    ":configure.bzl",
    "configure_local_variables",
)

configure_local_variables(
    name = "local_config",
    defs_template = "//:defs.bzl.tpl",
)

http_archive(
  name = "googletest",
  urls = ["https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip"],
  strip_prefix = "googletest-609281088cfefc76f9d0ce82e1ff6c30cc3591e5",
  sha256 = "5cf189eb6847b4f8fc603a3ffff3b0771c08eec7dd4bd961bfd45477dd13eb73",
)

http_archive(
  name = "bazel_clang_tidy",
  urls = ["https://github.com/erenon/bazel_clang_tidy/archive/1c3a983c054a41be73ac128e3d842a940850b5fe.zip"],
  strip_prefix = "bazel_clang_tidy-1c3a983c054a41be73ac128e3d842a940850b5fe",
  sha256 = "9943da5967e470a5c0165233242e51538b2a0fa11b15820d399314082cd99a09",
)
