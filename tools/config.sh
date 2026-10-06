# Shared settings for the review scripts. Sourced, not executed.

# The reviewed revisions. Both live in the same repository:
#   5.2.4  = EPA SWMM tag v5.2.4 (src/solver)
#   5.3.0  = src/legacy/engine at cb3e192b (OpenSWMM's maintained 5.3.0, legacy wrapper 5.3.0-beta.3)
#   6.0.0  = src/engine        at cb3e192b (OpenSWMM 6.0.0-alpha.4, branch swmm6_rel)
SWMM_REV_524=7952ca837988b1c32f791812eccc9fd64547e093
SWMM_REV_600=cb3e192b52674757a16afdfc6494382ef890f02d

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-$ROOT/.build}"

# Sanitizer flags. All checks are fatal except float-cast-overflow, which is
# made recoverable so that the known datetime_timeDiff overflow (CRASH-01) and
# the 5.2.4 single-point gage overflow can be suppressed in the other tests
# (see tools/ubsan.supp).
SANITIZE="${SANITIZE:-1}"
if [ "$SANITIZE" = "1" ]; then
    SAN_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -fsanitize-recover=float-cast-overflow -fno-omit-frame-pointer -g -O1"
    SAN_LDFLAGS="-fsanitize=address,undefined"
    BUILD_SUFFIX=""
else
    SAN_FLAGS="-g -O1"
    SAN_LDFLAGS=""
    BUILD_SUFFIX="-plain"
fi

CC="${CC:-clang}"
CXX="${CXX:-clang++}"

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"

# Platform differences (the scripts run on Linux and on macOS's bash 3.2)
case "$(uname -s)" in Darwin) SOEXT=dylib ;; *) SOEXT=so ;; esac

# Serialise builds when flock(1) is available (Linux); without it (macOS) the
# scripts simply assume one build at a time.
take_build_lock() {
    [ "${LOCK_HELD:-0}" = 1 ] && return 0
    mkdir -p "$BUILD_ROOT"
    if command -v flock >/dev/null 2>&1; then
        exec 9>"$BUILD_ROOT/.lock"
        flock 9
    fi
    export LOCK_HELD=1
}

sha1() { if command -v sha1sum >/dev/null 2>&1; then sha1sum; else shasum; fi; }

# run_timeout SECONDS CMD...: GNU timeout, gtimeout (Homebrew coreutils) or perl.
# A timed-out command returns 124 (timeout) or 142 (perl alarm).
run_timeout() {
    local t="$1"; shift
    if command -v timeout >/dev/null 2>&1; then timeout "$t" "$@"
    elif command -v gtimeout >/dev/null 2>&1; then gtimeout "$t" "$@"
    else perl -e 'alarm shift; exec @ARGV' "$t" "$@"
    fi
}

# Find an issue folder from its ID, e.g. NUM-01 -> 1-numerical/NUM-01-.../
issue_dir() {
    local id="$1" d
    for d in "$ROOT"/[0-9]-*/"$id"-*/; do
        [ -d "$d" ] && { echo "${d%/}"; return 0; }
    done
    return 1
}

all_issue_ids() {
    local d
    for d in "$ROOT"/[0-9]-*/*-[0-9][0-9]-*/; do
        [ -d "$d" ] || continue
        basename "$d" | sed -E 's/^([A-Z]+-[0-9]+)-.*/\1/'
    done
}
