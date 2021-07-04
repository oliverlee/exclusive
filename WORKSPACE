load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

load(
    ":configure.bzl",
    "configure_local_variables",
    "configure_clang_tidy",
)

COMPDB_VERSION="0.4.5"

http_archive(
    name = "com_grail_compdb",
    strip_prefix = "bazel-compilation-database-%s" % COMPDB_VERSION,
    urls = [
        "https://github.com/grailbio/bazel-compilation-database/archive/%s.tar.gz" % COMPDB_VERSION
    ],
    sha256 = "bcecfd622c4ef272fd4ba42726a52e140b961c4eac23025f18b346c968a8cfb4",
)

configure_local_variables(
    name = "local_config",
    defs_template = "//:defs.bzl.tpl",
)

configure_clang_tidy(
    name = "local_tidy_config",
)

http_archive(
  name = "googletest",
  urls = ["https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip"],
  strip_prefix = "googletest-609281088cfefc76f9d0ce82e1ff6c30cc3591e5",
  sha256 = "5cf189eb6847b4f8fc603a3ffff3b0771c08eec7dd4bd961bfd45477dd13eb73",
)
