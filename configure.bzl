"""Generate .bzl files containing local configuration variables.
"""


def _is_clang(repository_ctx, cc):
    return "clang" in repository_ctx.execute([cc, "-v"]).stderr


def _configure_local_variables_impl(repository_ctx):
    """Substitute %{compiler_name} with GCC or CLANG."""
    # Define an empty BUILD file to allow an external project
    repository_ctx.file("BUILD", "");

    cc = repository_ctx.os.environ.get("CC", "CC")

    compiler_name = "CLANG" if _is_clang(repository_ctx, cc) else "GCC"
    repository_ctx.template(
        "defs.bzl",
        repository_ctx.attr.defs_template,
        {"%{compiler_name}": compiler_name}
    )

    result = repository_ctx.execute(["pwd"])
    exec_dir = result.stdout.splitlines()[0]
    output_base = exec_dir.partition("external/")[0]

    repository_ctx.file(
        "bazel_info.bzl",
        'BAZEL_OUTPUT_BASE = "%s"\n' % output_base
    )

configure_local_variables = repository_rule(
    implementation = _configure_local_variables_impl,
    attrs = {
        "defs_template": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
    },
    local = True,
    configure = True,
    environ = ["CC"],
)
"""Generates a compiler configuration .bzl file and a Bazel info .bzl file.

Generate the following .bzl files:
- `defs.bzl`: sets compiler-dependent variables. From template `defs_template`.
- `bazel_info.bzl`: sets Bazel info dependent variables.

Args:
    defs_template: The template file with variable `%{compiler_name}`.
"""


def _configure_clang_tidy_impl(repository_ctx):
    tidy_bin_name = "clang-tidy"

    system_tidy_path = repository_ctx.os.environ.get("CLANG_TIDY_PATH")
    if not system_tidy_path:
        result = repository_ctx.execute(["which", tidy_bin_name])
        system_tidy_path = result.stdout.splitlines()[0]

    repository_ctx.symlink(system_tidy_path, tidy_bin_name)

    repository_ctx.file("BUILD", """
sh_binary(
    name = "clang-tidy-bin",
    srcs = ["%s"],
    visibility = ["//visibility:public"],
)
""" % tidy_bin_name)


configure_clang_tidy = repository_rule(
    implementation = _configure_clang_tidy_impl,
    local = True,
    configure = True,
    environ = ["CLANG_TIDY_PATH"],
)
"""Generates a repository for using an system install of `clang-tidy`.

Attempts to provide access to a system installation of `clang-tidy`. If the
`CLANG_TIDY_PATH` environment variable is defined, it will be used instead of
hard-coded system paths.

If specified, `CLANG_TIDY_PATH` must be an absolute path.
"""
