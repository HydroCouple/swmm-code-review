#!/usr/bin/env bash
# Builds the engines under review, with AddressSanitizer and
# UndefinedBehaviorSanitizer (set SANITIZE=0 for a plain build).
#
#   tools/build-engine.sh 524                 5.2.4 (EPA tag v5.2.4), unpatched         -> .build/524-base
#   tools/build-engine.sh 600                 5.3.0 + 6.0.0 at cb3e192b, unpatched      -> .build/600-base
#   tools/build-engine.sh 600 --all           with every fix patch in this review       -> .build/600-all
#   tools/build-engine.sh 600 --issue ID...   with the patches of the given issues      -> .build/600-work
#   tools/build-engine.sh 600 --issue ID --private                                   -> .build/600-ID
#                                             (a tree of your own, e.g. to run the patched CLIs on other
#                                             decks without another run-test.sh changing it; delete it
#                                             when done: rm -rf .build/600-ID)
#
# "600" trees contain both engines: src/legacy/engine is 5.3.0 and src/engine is 6.0.0.
# An issue's <ID>_swmm530.patch and <ID>_swmm600.patch are both applied.
#
# Options:
#   --targets legacy|engine|both   what to build (default both)
#   --unit-tests                   also build the project's unit tests and run them with ctest
#                                  (writes <build>/ctest.log and <build>/ctest-failed.txt)
#
# The last line printed is the build directory. Builds are serialised with a
# lock (.build/.lock) because they are large; set LOCK_HELD=1 if the caller
# already holds it. ccache is used when available, so trees share objects.
set -euo pipefail
. "$(dirname "$0")/config.sh"

engine="${1:-}"; shift || true
mode=base; ids=(); targets=both; unit_tests=0; private=0
while [ $# -gt 0 ]; do
    case "$1" in
        --all) mode=all ;;
        --issue) mode=work; shift; while [ $# -gt 0 ] && [[ "$1" != --* ]]; do ids+=("$1"); shift; done; continue ;;
        --targets) targets="$2"; shift ;;
        --unit-tests) unit_tests=1 ;;
        --private) private=1 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
case "$engine" in 524|600) ;; *) echo "usage: $0 524|600 [--all | --issue ID...] [--targets legacy|engine|both] [--unit-tests]" >&2; exit 2 ;; esac
[ "$engine" = 524 ] && [ "$mode" != base ] && { echo "5.2.4 is reviewed unpatched; fixes target 5.3.0 and 6.0.0" >&2; exit 2; }

mkdir -p "$BUILD_ROOT"
take_build_lock

SRC_REPO="$("$ROOT/tools/get-swmm.sh")"
name="$engine-$mode$BUILD_SUFFIX"
if [ "$private" = 1 ] && [ "$mode" = work ]; then name="$engine-${ids[0]}$BUILD_SUFFIX"; fi
tree="$BUILD_ROOT/$name"
src="$tree/src"
bld="$tree/build"
log="$tree/build.log"

# ---- source tree: a small git repo holding the exported revision, committed as "base".
# 600 trees always hold tests/, because a 6.0.0 patch also updates any unit test that
# pinned the behaviour it fixes; the tests are only built with --unit-tests.
paths_600=(CMakeLists.txt cmake include src tests)
need_tests=0; [ "$engine" = 600 ] && need_tests=1
if [ ! -d "$src/.git" ]; then
    rm -rf "$src"; mkdir -p "$src"
    if [ "$engine" = 524 ]; then
        git -C "$SRC_REPO" archive "$SWMM_REV_524" | tar -x -C "$src"
    else
        git -C "$SRC_REPO" archive "$SWMM_REV_600" "${paths_600[@]}" | tar -x -C "$src"
    fi
    git -C "$src" init -q
    git -C "$src" add -A
    git -C "$src" -c user.name=review -c user.email=review@localhost commit -qm base
    git -C "$src" tag base
fi
if [ "$need_tests" = 1 ] && [ ! -d "$src/tests" ]; then
    git -C "$SRC_REPO" archive "$SWMM_REV_600" tests | tar -x -C "$src"
    git -C "$src" add -A tests
    git -C "$src" -c user.name=review -c user.email=review@localhost commit -qm tests
    git -C "$src" tag -f base >/dev/null
fi

# ---- patches
patch_list=()
if [ "$mode" = all ]; then
    for id in $(all_issue_ids); do ids+=("$id"); done
fi
if [ "${#ids[@]}" -gt 0 ]; then
    for id in $("$ROOT/tools/patch-order.sh" "${ids[@]}"); do
        d="$(issue_dir "$id")" || { echo "no issue folder for $id" >&2; exit 2; }
        for p in "$d/${id}_swmm530.patch" "$d/${id}_swmm600.patch"; do
            [ -f "$p" ] && patch_list+=("$p")
        done
    done
fi
stamp="$( (for p in "${patch_list[@]+"${patch_list[@]}"}"; do echo "$p"; sha1 < "$p"; done) | sha1 | cut -c1-16)"
if [ "$(cat "$tree/patches.stamp" 2>/dev/null)" != "$stamp" ]; then
    git -C "$src" reset -q --hard base
    git -C "$src" clean -fdq
    for p in "${patch_list[@]+"${patch_list[@]}"}"; do
        if ! git -C "$src" apply --whitespace=nowarn "$p"; then
            echo "error: patch does not apply: $p" >&2
            rm -f "$tree/patches.stamp"
            exit 3
        fi
    done
    printf '%s\n' "${patch_list[@]+"${patch_list[@]}"}" > "$tree/patches.txt"
    echo "$stamp" > "$tree/patches.stamp"
    rm -f "$tree/ctest.log" "$tree/ctest-failed.txt"   # unit-test results of the old patches
fi

# ---- configure
launcher=()
if command -v ccache >/dev/null; then
    export CCACHE_BASEDIR="$BUILD_ROOT" CCACHE_NOHASHDIR=1 CCACHE_DIR="${CCACHE_DIR:-$BUILD_ROOT/ccache}"
    launcher=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
fi
gen=(); command -v ninja >/dev/null && gen=(-G Ninja)
common=(-DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX"
        "-DCMAKE_C_FLAGS=$SAN_FLAGS" "-DCMAKE_CXX_FLAGS=$SAN_FLAGS"
        "-DCMAKE_SHARED_LINKER_FLAGS=$SAN_LDFLAGS" "-DCMAKE_EXE_LINKER_FLAGS=$SAN_LDFLAGS"
        "${launcher[@]+"${launcher[@]}"}")
if [ ! -f "$bld/CMakeCache.txt" ] || { [ "$unit_tests" = 1 ] && ! grep -q "OPENSWMM_BUILD_UNIT_TESTS:BOOL=ON" "$bld/CMakeCache.txt"; }; then
    if [ "$engine" = 524 ]; then
        cmake -S "$src" -B "$bld" "${gen[@]+"${gen[@]}"}" "${common[@]}" > "$log" 2>&1 || { cat "$log" >&2; exit 4; }
    else
        cmake -S "$src" -B "$bld" "${gen[@]+"${gen[@]}"}" "${common[@]}" \
            -DOPENSWMM_WITH_GEOPACKAGE=OFF -DOPENSWMM_WITH_HDF5=OFF -DOPENSWMM_BUILD_2D=OFF \
            -DOPENSWMM_BUILD_GPU_PLUGIN=OFF -DOPENSWMM_INSTALL=OFF \
            -DOPENSWMM_BUILD_UNIT_TESTS=$([ "$unit_tests" = 1 ] && echo ON || echo OFF) > "$log" 2>&1 || { cat "$log" >&2; exit 4; }
    fi
fi

# ---- build
jobs="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)}"
if [ "$engine" = 524 ]; then
    tgt=(swmm5 runswmm swmm-output)
else
    case "$targets" in
        legacy) tgt=(openswmm_legacy_engine openswmm_legacy openswmm_legacy_output) ;;
        engine) tgt=(openswmm_engine openswmm) ;;
        both)   tgt=(openswmm_legacy_engine openswmm_legacy openswmm_legacy_output openswmm_engine openswmm) ;;
        *) echo "bad --targets $targets" >&2; exit 2 ;;
    esac
fi
if ! cmake --build "$bld" -j "$jobs" --target "${tgt[@]}" >> "$log" 2>&1; then
    tail -40 "$log" >&2
    echo "error: build failed, see $log" >&2
    exit 5
fi

if [ "$unit_tests" = 1 ]; then
    # Some test targets need optional modules that are off here (2D, HDF5); keep going past them.
    cmake --build "$bld" -j "$jobs" -- -k 0 >> "$log" 2>&1 || true
    (cd "$bld" && ctest -j "$jobs" --timeout 900 > "$tree/ctest.log" 2>&1) || true
    grep -E "^[[:space:]]+[0-9]+ - .*\((Failed|Not Run|Timeout|SEGFAULT|Subprocess aborted|Exception)" "$tree/ctest.log" \
        | sed -E 's/^[[:space:]]+[0-9]+ - //' > "$tree/ctest-failed.txt" || true
    grep -E "tests passed" "$tree/ctest.log" >&2 || true
fi

echo "$tree"
