# Reproducing the issues

Every issue folder contains:

| File | What it is |
|---|---|
| `README.md` | the write-up |
| `<ID>_test.c` | a test against the legacy toolkit API (`swmm5.h`), compiled for **5.2.4** and **5.3.0** |
| `<ID>_test6.c` (or `.cpp`) | the same check against the **6.0.0** C API (`openswmm/engine/*.h`) |
| `<ID>_*.inp` and data files | the input files the tests run |
| `<ID>_swmm530.patch` | the fix for 5.3.0 (`src/legacy/engine`, `src/legacy/output`) |
| `<ID>_swmm600.patch` | the fix for 6.0.0 (`src/engine`, plus `tests/` where one of 6.0.0's unit tests pinned the old behaviour) |

A folder has no 5.3.0 or 6.0.0 patch when that engine is not affected. Issues that exist only in 5.2.4 have no patches: 5.2.4 is reviewed but not patched, and their write-ups say where 5.3.0 and 6.0.0 fixed them. IO-25 is a documentation error; its fix is `IO-25_docs.patch`, against the input file reference in `docs/`.

The engines are pinned:

| Engine | Source | Revision |
|---|---|---|
| 5.2.4 | EPA SWMM, `src/solver` | tag `v5.2.4` ([7952ca8](https://github.com/USEPA/Stormwater-Management-Model/tree/v5.2.4)) |
| 5.3.0 | OpenSWMM's legacy engine, `src/legacy/engine` (legacy wrapper 5.3.0-beta.3) | [cb3e192b](https://github.com/HydroCouple/Stormwater-Management-Model/tree/cb3e192b52674757a16afdfc6494382ef890f02d), branch `swmm6_rel`, 4 October 2026 |
| 6.0.0 | OpenSWMM's engine, `src/engine` (6.0.0-alpha.4) | the same commit |

All three are in the [HydroCouple/Stormwater-Management-Model](https://github.com/HydroCouple/Stormwater-Management-Model) repository, which is the `swmm` submodule of this repository. Both patches of an issue apply from the root of a checkout at cb3e192b.

Each test prints what it observed and ends with one line:

| Last line | Exit code | Meaning |
|---|---|---|
| `PASS: ...` | 0 | The engine behaves correctly |
| `FAIL: ...` | 1 | The test found the bug |

Tests for crash bugs make the call that crashes. In a build with AddressSanitizer they stop there with a sanitizer report; with the fix, the call returns or reports an error and the test prints `PASS`.

## With the scripts (Linux or macOS)

You need a C/C++ compiler with sanitizers (clang or Apple clang), CMake, Ninja (optional), git, and OpenMP (libomp; Homebrew `libomp` on macOS). Clone with `git clone --recurse-submodules`, or let the scripts fetch the submodule the first time they run. To use a clone of the SWMM repository you already have, set `SWMM_SRC` to its path; the scripts read it with `git archive` and never change its checkout.

```sh
tools/run-test.sh NUM-01              # 5.2.4, 5.3.0 and 6.0.0 unpatched: one verdict per engine
tools/run-test.sh NUM-01 --patched    # 5.3.0 and 6.0.0 with NUM-01's patches: PASS
tools/run-test.sh NUM-01 --all        # with every patch in the review applied together
tools/run-test.sh NUM-01 --engine 530 # one engine only (524, 530 or 600)
tools/run-all.sh                      # every test, unpatched, patched and with all patches,
                                      # then the project's unit tests; writes verification.md
```

| Script | What it does |
|---|---|
| [`tools/build-engine.sh`](tools/build-engine.sh) | Builds 5.2.4 (`524`) or the 5.3.0 + 6.0.0 tree (`600`) with AddressSanitizer and UndefinedBehaviorSanitizer: unpatched into `.build/<engine>-base`, with one issue's patches into `.build/600-work`, with every patch into `.build/600-all`. `--unit-tests` also builds and runs the project's ctest suite. `SANITIZE=0` makes a plain build. |
| [`tools/run-test.sh`](tools/run-test.sh) | Compiles an issue's tests against the requested engines, runs them in a scratch copy of the issue folder under `.build/run/`, and prints a verdict: `PASS`, `FAIL`, `CRASH` (sanitizer report or signal), `HANG` (over 300 s), `n/a` (no test for that engine) |
| [`tools/run-all.sh`](tools/run-all.sh) | Runs every test unpatched, with its own patches and with all patches, runs the unit tests unpatched and with all patches, and records the results in [verification.md](verification.md) |
| [`tools/patch-workspace.sh`](tools/patch-workspace.sh) | Gives you an editable copy of the 5.3.0/6.0.0 source with an issue's patches applied, and writes your changes back to its two patch files |
| [`tools/patch-order.sh`](tools/patch-order.sh) | Lists the patches to apply for an issue, prerequisites first (a patch names another one in a `Requires:` line) |
| [`tools/get-swmm.sh`](tools/get-swmm.sh) | Finds the SWMM source the other scripts use: `SWMM_SRC` if it is set, otherwise the `swmm` submodule |

Everything the scripts build or write goes into `.build/`, which git ignores. The 6.0.0 builds turn off the optional 2D, HDF5, GeoPackage and GPU modules, which no issue needs.

## By hand

Against a 5.2.4 build (its toolkit header is `src/solver/include/swmm5.h`):

```sh
cc NUM-03_test.c -I<swmm-5.2.4>/src/solver/include -L<build>/src/solver -lswmm5 -lm -o NUM-03_test
./NUM-03_test
```

Against a 5.3.0 build, add `tools/compat/530` to the include path: its `swmm5.h` includes 5.3.0's `openswmm_solver.h`, a superset of 5.2.4's header.

```sh
cc NUM-03_test.c -I<review>/tools/compat/530 -I<swmm>/include/openswmm/legacy/engine \
   -I<build>/src/legacy/engine -L<build>/src/legacy/engine -lopenswmm.legacy.engine -lm -o NUM-03_test
```

Against a 6.0.0 build (link with the C++ driver):

```sh
cc -c NUM-03_test6.c -I<swmm>/include -I<swmm>/include/openswmm/engine
c++ NUM-03_test6.o -L<build>/src/engine -lopenswmm.engine -o NUM-03_test6
```

Tests that read the binary output file also link the output-file reader: `-lswmm-output` for 5.2.4, `-lopenswmm.legacy.output` for 5.3.0 (`tools/compat/530/swmm_output.h` maps the header name).

To apply a fix, from the root of a SWMM checkout at cb3e192b:

```sh
git apply "<review>/1-numerical/NUM-03-rect-round-hydraulic-radius/NUM-03_swmm530.patch"
git apply "<review>/1-numerical/NUM-03-rect-round-hydraulic-radius/NUM-03_swmm600.patch"
```

The text above the first `diff --git` line of each patch says what it changes and why; `git apply` ignores it. A `Requires:` line there names a patch that has to be applied first (`tools/patch-order.sh` lists them). Every patch applies alone, or after its prerequisites, and all 325 apply together in the order `tools/patch-order.sh` gives.

6.0.0 is designed to give the same results as the legacy engine, and its parity tests compare the two. Several 6.0.0 patches deliberately change results that 6.0.0 had copied from legacy, so apply an issue's 5.3.0 and 6.0.0 patches together to keep the parity tests meaningful.

## Sanitizers

Most crash bugs are memory errors or undefined behaviour. Without AddressSanitizer and UndefinedBehaviorSanitizer they often do no visible harm or fail later somewhere unrelated, so build as `tools/build-engine.sh` does:

```sh
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fsanitize-recover=float-cast-overflow -fno-omit-frame-pointer -g -O1"
cmake -S <swmm> -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="$SAN" -DCMAKE_CXX_FLAGS="$SAN" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DOPENSWMM_BUILD_2D=OFF -DOPENSWMM_WITH_HDF5=OFF -DOPENSWMM_WITH_GEOPACKAGE=OFF -DOPENSWMM_BUILD_GPU_PLUGIN=OFF
cmake --build build-asan --target openswmm_legacy_engine openswmm_legacy_output openswmm_engine
```

Run tests with `ASAN_OPTIONS=detect_leaks=0`. One check is made recoverable: float-cast-overflow. [CRASH-01](5-crashes/CRASH-01-datetime-timediff-overflow/) fires on every deck whose rain series ends before the run does, and 5.2.4's [CRASH-21](5-crashes/CRASH-21-single-point-rain-series-524/) on every one-record rain series. Without suppressing them, a sanitizer build of 5.2.4 or 5.3.0 stops on most models with rain gages before reaching anything else. `tools/run-test.sh` passes `UBSAN_OPTIONS=suppressions=tools/ubsan.supp`, which silences exactly those two reports; the two issues' own folders contain a file `no-ubsan-suppressions` that turns the suppression off for their tests.

Without OpenMP the 5.3.0 legacy library does not link (two files define the same `omp_get_max_threads` stub) and 5.2.4's CMake does not configure; install libomp.
