#!/usr/bin/env bash
# Runs one issue's tests and prints a verdict per engine.
#
#   tools/run-test.sh ID                  5.2.4, 5.3.0 and 6.0.0, unpatched
#   tools/run-test.sh ID --patched        5.3.0 and 6.0.0 with the issue's own patches
#   tools/run-test.sh ID --all            5.3.0 and 6.0.0 with every patch in the review
#   tools/run-test.sh ID --engine 530     only one engine (524, 530 or 600; repeatable)
#   tools/run-test.sh ID -q               print only the verdict lines
#
# <ID>_test.c is compiled against the legacy toolkit API (swmm5.h) for 5.2.4 and 5.3.0;
# <ID>_test6.c (or .cpp) against the 6.0.0 C API. Each test runs in a scratch copy of
# the issue folder under .build/run/ID/<engine>-<variant>/ and must end with a line
# "PASS: ..." (exit 0) or "FAIL: ..." (exit 1).
#
# Verdicts: PASS, FAIL, CRASH (sanitizer report or signal), HANG (over $TEST_TIMEOUT s),
# n/a (no test for that engine), ERROR (the test did not compile or gave no verdict),
# BUILD-ERROR (the engine or a patch failed to build/apply).
set -uo pipefail
. "$(dirname "$0")/config.sh"

id="${1:?usage: $0 ID [--patched|--all] [--engine 524|530|600] [-q]}"; shift
variant=base; engines=(); quiet=0
while [ $# -gt 0 ]; do
    case "$1" in
        --patched) variant=patched ;;
        --all) variant=all ;;
        --engine) engines+=("$2"); shift ;;
        -q) quiet=1 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
if [ "${#engines[@]}" -eq 0 ]; then
    if [ "$variant" = base ]; then engines=(524 530 600); else engines=(530 600); fi
fi
d="$(issue_dir "$id")" || { echo "no issue folder for $id" >&2; exit 2; }
TEST_TIMEOUT="${TEST_TIMEOUT:-300}"

label() { case "$1" in 524) echo 5.2.4 ;; 530) echo 5.3.0 ;; 600) echo 6.0.0 ;; esac; }

ubsan_opts="print_stacktrace=1:halt_on_error=0"
if [ ! -f "$d/no-ubsan-suppressions" ]; then
    ubsan_opts="$ubsan_opts:suppressions=$ROOT/tools/ubsan.supp"
fi

# ---- make sure the needed engine trees exist (patched trees are built under the lock)
need_lock=0
[ "$variant" != base ] && need_lock=1
if [ "$need_lock" = 1 ]; then
    take_build_lock
fi

tree_for() {   # $1 = engine; prints tree dir or returns 1
    local e="$1" tgt
    case "$e" in 530) tgt=legacy ;; 600) tgt=engine ;; esac
    if [ "$e" = 524 ]; then
        [ -f "$BUILD_ROOT/524-base$BUILD_SUFFIX/build/src/outfile/libswmm-output.$SOEXT" ] || "$ROOT/tools/build-engine.sh" 524 >/dev/null || return 1
        echo "$BUILD_ROOT/524-base$BUILD_SUFFIX"; return 0
    fi
    case "$variant" in
        base)
            local t="$BUILD_ROOT/600-base$BUILD_SUFFIX"
            if [ "$e" = 530 ] && [ -f "$t/build/src/legacy/output/libopenswmm.legacy.output.$SOEXT" ]; then echo "$t"; return 0; fi
            if [ "$e" = 600 ] && [ -f "$t/build/src/engine/libopenswmm.engine.$SOEXT" ]; then echo "$t"; return 0; fi
            if [ "$need_lock" = 0 ]; then
                ( take_build_lock; "$ROOT/tools/build-engine.sh" 600 --targets "$tgt" >/dev/null ) || return 1
            else
                "$ROOT/tools/build-engine.sh" 600 --targets "$tgt" >/dev/null || return 1
            fi
            echo "$t" ;;
        patched) "$ROOT/tools/build-engine.sh" 600 --issue "$id" --targets "$tgt" 2>"$BUILD_ROOT/last-build-error.txt" | tail -1 ;;
        all)     "$ROOT/tools/build-engine.sh" 600 --all --targets "$tgt" 2>"$BUILD_ROOT/last-build-error.txt" | tail -1 ;;
    esac
}

overall=0
for e in "${engines[@]}"; do
    L="$(label "$e")"
    if [ "$e" = 600 ]; then
        test_src=""; for f in "$d/${id}_test6.c" "$d/${id}_test6.cpp"; do [ -f "$f" ] && test_src="$f"; done
    else
        test_src="$d/${id}_test.c"; [ -f "$test_src" ] || test_src=""
    fi
    if [ -z "$test_src" ]; then echo "$id $L $variant: n/a"; continue; fi
    if [ "$e" = 524 ] && [ "$variant" != base ]; then echo "$id $L $variant: n/a"; continue; fi

    tree="$(tree_for "$e")" || tree=""
    if [ -z "$tree" ] || [ ! -d "$tree" ]; then
        echo "$id $L $variant: BUILD-ERROR"; [ -f "$BUILD_ROOT/last-build-error.txt" ] && tail -5 "$BUILD_ROOT/last-build-error.txt" | sed 's/^/    /'
        overall=1; continue
    fi

    run="$BUILD_ROOT/run/$id/$e-$variant"
    rm -rf "$run"; mkdir -p "$run"
    cp -R "$d"/. "$run"/
    exe="$run/test.bin"
    comp="$CC"; [[ "$test_src" == *.cpp ]] && comp="$CXX"
    case "$e" in
        524) inc=(-I"$tree/src/src/solver/include" -I"$tree/src/src/outfile/include" -I"$tree/build/src/outfile")
             lib=(-L"$tree/build/src/solver" -lswmm5 -L"$tree/build/src/outfile" -lswmm-output
                  -Wl,-rpath,"$tree/build/src/solver" -Wl,-rpath,"$tree/build/src/outfile") ;;
        530) inc=(-I"$ROOT/tools/compat/530" -I"$tree/src/include/openswmm/legacy/engine" -I"$tree/build/src/legacy/engine"
                  -I"$tree/src/include/openswmm/legacy/output" -I"$tree/build/src/legacy/output")
             lib=(-L"$tree/build/src/legacy/engine" -lopenswmm.legacy.engine -L"$tree/build/src/legacy/output" -lopenswmm.legacy.output
                  -Wl,-rpath,"$tree/build/src/legacy/engine" -Wl,-rpath,"$tree/build/src/legacy/output") ;;
        600) inc=(-I"$tree/src/include" -I"$tree/src/include/openswmm/engine" -I"$tree/build/src/engine")
             lib=(-L"$tree/build/src/engine" -lopenswmm.engine -Wl,-rpath,"$tree/build/src/engine") ;;
    esac
    # 6.0.0 is C++: link with the C++ driver so the C++ sanitizer runtime is included
    linker="$CC"; [ "$e" = 600 ] && linker="$CXX"
    # shellcheck disable=SC2086
    if ! (cd "$run" && $comp $SAN_FLAGS -w "${inc[@]}" -c "$(basename "$test_src")" -o test.o \
                    && $linker $SAN_FLAGS test.o -o "$exe" "${lib[@]}" -lm $SAN_LDFLAGS) > "$run/compile.log" 2>&1; then
        echo "$id $L $variant: ERROR (test does not compile, see $run/compile.log)"; overall=1
        [ "$quiet" = 0 ] && tail -15 "$run/compile.log" | sed 's/^/    /'
        continue
    fi
    (cd "$run" && UBSAN_OPTIONS="$ubsan_opts" run_timeout "$TEST_TIMEOUT" "$exe") > "$run/output.txt" 2>&1
    rc=$?
    last="$(grep -E '^(PASS|FAIL):' "$run/output.txt" | tail -1)"
    if [ $rc -eq 124 ] || [ $rc -eq 142 ]; then verdict=HANG
    elif grep -qE 'ERROR: AddressSanitizer|runtime error:|ERROR: UndefinedBehaviorSanitizer|ERROR: LeakSanitizer' "$run/output.txt"; then verdict=CRASH
    elif [ $rc -ge 128 ]; then verdict=CRASH
    elif [ $rc -eq 0 ] && [[ "$last" == PASS:* ]]; then verdict=PASS
    elif [ $rc -eq 1 ] && [[ "$last" == FAIL:* ]]; then verdict=FAIL
    else verdict="ERROR (exit $rc, no verdict line)"
    fi
    if [ "$quiet" = 0 ]; then
        echo "---- $id on $L ($variant) ----"
        # keep the console readable: the test's own output, minus sanitizer stack frames beyond the first few
        awk '/^    #[0-9]+ /{n++; if(n>8) next} {print}' "$run/output.txt" | tail -n 60
    fi
    echo "$id $L $variant: $verdict"
    case "$verdict" in PASS) ;; *) overall=1 ;; esac
done
exit $overall
