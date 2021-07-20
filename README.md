# exclusive
Provides exclusive access to a shared resources

## challenge responses

### Algorithm analysis

The provided algorithm appears to be similar to the Peterson lock algorithm.

  - mutual exclusion

    Suppose mutual exclusion is not satisfied. From inspection of the code, we
    see the locking steps are:

    ```
    write_0(flag[0] = true) -> read_0(turn == 1) -> read_0(flag[1] == false) -> write_0(turn = 0) -> CS_0
    write_1(flag[1] = true) -> read_1(turn == 0) -> read_1(flag[0] == false) -> write_1(turn = 1) -> CS_1
    ```

    and that

    ```
    write_0(turn = 0) -> read_1(turn == 0)
    ```

    Then

    ```
    write_0(flag[0] = true) -> read_0(turn == 1) -> read_0(flag[1] == false) ->
    write_0(turn = 0) -> read_1(turn == 0) -> read_1(flag[0] === false)
    ```

    It follows that

    ```
    write_0(flag[0] = true) -> read_1(flag[0] == false)
    ```

    This observation yields a contradiction because no other write to `flag[0]`
    was performed before the critical section executions.

  - deadlock free

    This algorithm suffers from deadlock. Imagine the following:

    `turn` is initialized to `0` and each row is a step forward in time

    |          P0              |          P1                 |
    | ------------------------ | --------------------------- |
    | `write(flag[0] = true)`  | `write(flag[1] = true)`     |
    | `read(turn == 1)` ❌     | `read(turn == 0)`        ✅ |
    | `read(turn == 1)` ❌     | `read(flag[0] == false)` ❌ |
    | `read(turn == 1)` ❌     | `read(flag[0] == false)` ❌ |
    | `read(turn == 1)` ❌     | `read(flag[0] == false)` ❌ |

### Alternate methods

An alternate method could be the Peterson lock algorithm. It is similar to the
provided algorithm and provides mutual exclusion, deadlock freedom, and
freedom from starvation. However, it may be poorly suited for a safety critical
environment. The lock method blocks until the critical section is released by
the other process. It can also block indefinitely if the other process stops
participating.

Another alternative that provide mutual exclusion and deadlock freedom is to
simply use a `std::timed_mutex`. This doesn't provide fairness/starvation
freedom, but a backoff scheme might be good enough.

The alternative I implement is CLH queue lock with timeouts, as described in
Chapter 7 of The Art of Multicore. However, instead of each process allocating a
`QNode`, as dynamic memory is hassle with in a safety critical environment
(during init phase, arena alloc., etc.), the nodes come from a pool owned by the
CLH queue lock. The pool size can be configured. While the algorithm provides
first-come-first-served fairness once a process has queued-up, there is a race
to enter the lock queue. In order to guarantee a consistent order, processes may
wait on a `queued_count` to reduce racing.

### Evidence of algorithm correctness

I think this is hard. Tests may be insufficient as races may not manifest, but
still should be written to show that the algorithm doesn't completely fail.
Maybe there are some static analysis or simulation tools that I don't know
about. I think correctness probably comes from human reviewers in the end.

### solution

The solution is implemented in two parts.

The first is the `clh_mutex<N>` class template which satisfies the `TimedMutex`
requirements. `N` is an unsigned integer which specifies the number of
"available" queue nodes. This should match the number of processes sharing a
resource.

The second is the `shared_resource<T, M>` class template. This simply provides a
nicer to use interface as it bundles the shared resource `T` (e.g. an int) along
with a mutex `M`. A proxy, optional-ish object is returned when attempting to
acquire access, where access is possible if locking was successful.

#### library
This repository is built with Bazel 4.1.0 but lower versions may work. It
provides the `exclusive` library which contains the class templates mentioned
above.

#### example
There's an example using `clh_mutex` in the `example` directory

    $ bazel run //example:counter

#### testing
If you want run the tests, you can do

    $ bazel test //...

You can also run tests (or whatever) with sanitizers

    $ bazel test //... --config=tsan

See `.bazelrc` for config options.

Some (but not all!) tests depend on wall time so keep that in mind if you have a
heavily loaded machine.

#### linting
Linting is a config

    $ bazel build --config=clang-tidy //...

This may not work on macOS (possibly due to
[this](https://github.com/bazelbuild/bazel/issues/12049)).

You can take the very annoying step of modifying
`bazel-exclusive/external/bazel_clang_tidy/clang_tidy/run_clang_tidy.sh` and
hard-coding the path to `clang-tidy`. Or create a symlink in one directory of
Bazel's macOS default shell env (e.g. `/usr/local/bin`). You'll also need Bazel
to detect that Clang is the CXX compiler, otherwise a bunch of GCC flags get
used. There's probably a better way to do it but I'm not a Bazel expert.
