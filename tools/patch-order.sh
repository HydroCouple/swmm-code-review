#!/usr/bin/env bash
# Lists the issues whose patches must be applied for the given issues,
# prerequisites first. A patch names a prerequisite with a line
#     Requires: <ID>
# in the description above its first "diff --git" line.
set -euo pipefail
. "$(dirname "$0")/config.sh"

seen=" "
order=()

requires_of() {
    local id="$1" d p
    d="$(issue_dir "$id")" || return 0
    for p in "$d/${id}_swmm530.patch" "$d/${id}_swmm600.patch"; do
        [ -f "$p" ] || continue
        sed -n '/^diff --git/q;p' "$p" | sed -nE 's/^Requires:[[:space:]]*([A-Z]+-[0-9]+).*/\1/p'
    done | sort -u
}

visit() {
    local id="$1" r
    case "$seen" in *" $id "*) return 0 ;; esac
    seen="$seen$id "
    for r in $(requires_of "$id"); do visit "$r"; done
    order+=("$id")
}

for id in "$@"; do visit "$id"; done
printf '%s\n' "${order[@]+"${order[@]}"}"
