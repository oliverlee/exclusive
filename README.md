# exclusive
Provides exclusive access to a shared resources

This repository is built with Bazel 4.1.0 but lower versions may work.

## testing

    $ bazel test //...

## linting

This does not work on macOS (probably due to [this](https://github.com/bazelbuild/bazel/issues/12049)).

    $ bazel build --config=clang-tidy //...

You can take the very annoying step of modifying
`bazel-exclusive/external/bazel_clang_tidy/clang_tidy/run_clang_tidy.sh` and
hard-coding the path to `clang-tidy`.
