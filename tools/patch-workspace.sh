#!/usr/bin/env bash
# An editable copy of the 5.3.0/6.0.0 source for writing an issue's fix.
#
#   tools/patch-workspace.sh ID           create (or reuse) .build/ws/ID and print its path.
#                                         Prerequisite patches (Requires: lines) are applied and
#                                         committed; the issue's own patches are applied on top,
#                                         uncommitted, so `git diff` shows exactly the fix.
#   tools/patch-workspace.sh ID --save    write the workspace's changes back to the issue folder:
#                                         changes under src/legacy/ and include/openswmm/legacy/
#                                         go to ID_swmm530.patch, all other changes (src/engine,
#                                         and tests/ where a unit test pinned the old behaviour)
#                                         to ID_swmm600.patch. The description above the first
#                                         "diff --git" line of an existing patch is kept.
#   tools/patch-workspace.sh ID --reset   discard the workspace and start again.
set -euo pipefail
. "$(dirname "$0")/config.sh"

id="${1:?usage: $0 ID [--save|--reset]}"; action="${2:-}"
d="$(issue_dir "$id")" || { echo "no issue folder for $id (create <category>/$id-<slug>/ first)" >&2; exit 2; }
ws="$BUILD_ROOT/ws/$id"

if [ "$action" = --reset ]; then rm -rf "$ws"; action=""; fi

if [ ! -d "$ws/.git" ]; then
    SRC_REPO="$("$ROOT/tools/get-swmm.sh")"
    mkdir -p "$ws"
    git -C "$SRC_REPO" archive "$SWMM_REV_600" CMakeLists.txt cmake include src tests | tar -x -C "$ws"
    git -C "$ws" init -q
    git -C "$ws" add -A
    git -C "$ws" -c user.name=review -c user.email=review@localhost commit -qm "cb3e192b"
    for pre in $("$ROOT/tools/patch-order.sh" "$id"); do
        [ "$pre" = "$id" ] && continue
        pd="$(issue_dir "$pre")"
        for p in "$pd/${pre}_swmm530.patch" "$pd/${pre}_swmm600.patch"; do
            [ -f "$p" ] && git -C "$ws" apply --whitespace=nowarn "$p"
        done
        git -C "$ws" add -A
        git -C "$ws" -c user.name=review -c user.email=review@localhost commit -qm "$pre" --allow-empty
    done
    git -C "$ws" tag -f pre >/dev/null
    for p in "$d/${id}_swmm530.patch" "$d/${id}_swmm600.patch"; do
        [ -f "$p" ] && git -C "$ws" apply --whitespace=nowarn "$p"
    done
fi

if [ "$action" = --save ]; then
    write_patch() {   # $1 = output file, rest = git pathspecs
        local out="$1"; shift
        local header="" tmp
        tmp="$(mktemp)"
        git -C "$ws" add -A
        git -C "$ws" diff --cached pre -- "$@" > "$tmp"
        git -C "$ws" reset -q
        if [ ! -s "$tmp" ]; then
            if [ -f "$out" ]; then echo "removing $(basename "$out") (no changes)"; rm -f "$out"; fi
            rm -f "$tmp"; return 0
        fi
        if [ -f "$out" ]; then header="$(sed -n '/^diff --git/q;p' "$out")"; fi
        if [ -z "$header" ]; then
            header="$id: <one-line summary of the fix>

<What the patch changes and why, in a few lines.>

Apply from the root of a Stormwater-Management-Model checkout at cb3e192b:
    git apply $(basename "$out")
"
        fi
        { printf '%s\n' "$header"; [ "$(printf '%s' "$header" | tail -c1)" = "" ] || echo; cat "$tmp"; } > "$out.new"
        # drop a doubled blank line between header and diff
        awk 'BEGIN{b=0} /^diff --git/{b=0} {if($0==""){b++; if(b>1 && !d) next} else b=0; if(/^diff --git/) d=1; print}' "$out.new" > "$out"
        rm -f "$out.new" "$tmp"
        echo "wrote $(basename "$out")"
    }
    write_patch "$d/${id}_swmm530.patch" src/legacy include/openswmm/legacy
    write_patch "$d/${id}_swmm600.patch" . ':(exclude)src/legacy' ':(exclude)include/openswmm/legacy'
    exit 0
fi

echo "$ws"
