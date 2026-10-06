#!/usr/bin/env bash
# Prints the path of a git repository that holds both reviewed revisions.
#
# Uses $SWMM_SRC if it is set (any clone of HydroCouple/Stormwater-Management-Model
# or of a fork with the same history), otherwise the `swmm` submodule.
# The repository is only read (git archive); its checkout is never changed.
set -euo pipefail
. "$(dirname "$0")/config.sh"

src="${SWMM_SRC:-$ROOT/swmm}"

if [ ! -e "$src/.git" ] && [ -z "${SWMM_SRC:-}" ]; then
    echo "Fetching the swmm submodule..." >&2
    git -C "$ROOT" submodule update --init swmm >&2
fi

for rev in "$SWMM_REV_524" "$SWMM_REV_600"; do
    if ! git -C "$src" cat-file -e "$rev^{commit}" 2>/dev/null; then
        echo "Fetching $rev into $src..." >&2
        git -C "$src" fetch --quiet origin "$rev" >&2 || git -C "$src" fetch --quiet --tags origin >&2
    fi
    git -C "$src" cat-file -e "$rev^{commit}" || { echo "error: $src does not contain $rev" >&2; exit 2; }
done

echo "$src"
