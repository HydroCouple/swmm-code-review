# IO-11: The Barrels item of an IRREGULAR cross section is ignored

**Status: partially addressed in OpenSWMM 5.3.0; changes required before approval.** [Issue #176](https://github.com/HydroCouple/openswmm.engine/issues/176) · [Engine PR #193](https://github.com/HydroCouple/openswmm.engine/pull/193) · reviewed head `eaa292d8b3c2cd94f70fa46c7d83221d47a247de` (validated 28 October 2026). OpenSWMM 6.0.0 is not affected.

The reproduction below records the original reviewed revisions; the follow-up validation at the end records the submitted PR.

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A natural channel conduit written with 2 or more barrels runs with one, without a message, so it carries all the flow in a single channel. In the test, two barrels carrying 20 cfs leave the inlet junction at 0.995 ft instead of 0.696 ft. 6.0.0 reads the value, so the same input file gives different results in 5.3.0 and 6.0.0. |
| **Reached from** | `[XSECTIONS]` lines `Link IRREGULAR Tsect 0 0 0 Barrels` with Barrels > 1 (the GUI writes IRREGULAR lines in this layout); the same for STREET lines (see the end) |
| **5.3.0** | `link_readXsectParams()` in [`src/legacy/engine/link.c:199`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L199) (IRREGULAR) and [`:209`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L209) (STREET) |
| **5.2.4** | Same code, [`src/solver/link.c:196`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L196) |
| **6.0.0** | Not affected: [`handle_xsections()`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/LinksHandler.cpp#L495) reads Barrels for every shape |
| **Since** | Every SWMM 5 release: 5.0 and 5.1 read Barrels only in the branch for regular shapes; 5.2 kept that with an early `return 0` |
| **Fix** | Read Barrels on IRREGULAR lines as on the others: [`IO-11_swmm530.patch`](IO-11_swmm530.patch) |

## The problem

The manual defines Barrels for every `[XSECTIONS]` line as the "number of barrels (i.e., number of parallel pipes of equal size, slope, and roughness) associated with a conduit (default is 1)". Its format line for natural channels is the short form `Link IRREGULAR Tsect`. The GUI, however, writes IRREGULAR lines with the full column layout, `Link IRREGULAR Tsect 0 0 0 Barrels`. The regression decks contain 231 such lines, all with Barrels = 1.

The legacy reader stops reading an IRREGULAR line after the transect name. A Barrels value of 2 or more on that line is ignored, and nothing in the report says so: the Cross Section Summary just lists 1 barrel. The conduit then carries the whole flow through one channel, and the water upstream stands higher than it should.

6.0.0 reads the value. A model with a multi-barrel natural channel therefore runs with one barrel in 5.2.4 and 5.3.0 and with the number given in 6.0.0.

## Why it happens

```c
// src/legacy/engine/link.c, link_readXsectParams()
    // --- assign default number of barrels to conduit
    if ( Link[j].type == CONDUIT ) Conduit[Link[j].subIndex].barrels = 1;
    ...
    if ( k == IRREGULAR )
    {
        i = project_findObject(TRANSECT, tok[2]);
        if ( i < 0 ) return error_setInpError(ERR_NAME, tok[2]);
        Link[j].xsect.type = k;
        Link[j].xsect.transect = i;
        return 0;                               // Barrels (tok[6]) is never read
    }
    ...
        // --- parse number of barrels if present
        if ( Link[j].type == CONDUIT && ntoks >= 7 )
        ...
```

Everything downstream of the reader already handles barrels for any shape: flows, areas, volumes and losses are computed per barrel and multiplied by `Conduit[k].barrels`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-11_irregular-barrels.inp`](IO-11_irregular-barrels.inp) | Two identical channels on the same transect: C1 with 2 barrels and 20 cfs, C2 with 1 barrel and 10 cfs |
| [`IO-11_test.c`](IO-11_test.c) | Runs the deck through the legacy toolkit (5.2.4, 5.3.0), reads the barrel count from the report and the junction depths at the end of the run |
| [`IO-11_test6.c`](IO-11_test6.c) | The same through the 6.0.0 API |

With 2 barrels, each barrel of C1 carries 10 cfs, exactly what C2's single barrel carries, so J1 and J2 must end the 2-hour run at the same depth. The test allows 0.005 ft. Ignoring the second barrel makes the difference 0.3 ft.

```sh
tools/run-test.sh IO-11            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-11 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.3.0; 5.2.4 gives 0.997 and 0.697 ft):

```
Link  Barrels  Barrels in  Flow    Flow per      Inlet depth
      entered  report      (cfs)   barrel (cfs)  at end (ft)
C1          2           1   20.00         20.00        0.995
C2          1           1   10.00         10.00        0.696
FAIL: C1 was given 2 barrels but has 1; J1 stands at 0.995 ft instead of 0.696 ft (the depth of one barrel carrying 10 cfs)
IO-11 5.3.0 base: FAIL
```

**6.0.0, and 5.3.0 with the fix** (same numbers):

```
Link  Barrels  Barrels in  Flow    Flow per      Inlet depth
      entered  report      (cfs)   barrel (cfs)  at end (ft)
C1          2           2   20.00         10.00        0.696
C2          1           1   10.00         10.00        0.696
PASS: C1 has 2 barrels and each carries the same flow at the same depth as C2
IO-11 5.3.0 patched: PASS
IO-11 6.0.0 base: PASS
```

## The fix

Read the Barrels item in the IRREGULAR branch, with the same check as for the other shapes:

```diff
         Link[j].xsect.type = k;
         Link[j].xsect.transect = i;
+
+        // --- parse number of barrels if present (Tsect 0 0 0 Barrels)
+        if ( Link[j].type == CONDUIT && ntoks >= 7 )
+        {
+            i = atoi(tok[6]);
+            if ( i <= 0 ) return error_setInpError(ERR_NUMBER, tok[6]);
+            Conduit[Link[j].subIndex].barrels = i;
+        }
         return 0;
```

The value is assigned without the `(char)` cast used for the other shapes, so the line stays correct once [IO-10](../IO-10-barrels-stored-as-char/) widens the field to `int`. Short-form lines (`Link IRREGULAR Tsect`) and lines with Barrels = 1 read as before. **Effect on other models:** all 231 IRREGULAR lines with a Barrels item in the regression decks have Barrels = 1, so no regression result changes. An IRREGULAR line with Barrels 0 or a non-number in that column is now rejected with ERROR 211, as it already is for every other shape.

**STREET is left as it is.** STREET lines return early in the same way (`link.c:209`), and 6.0.0 reads Barrels for them too, so the two engines also differ there. A street conduit is not a plain open channel, though. Its inlets (`inlet.c`) compute gutter spread and capture from the conduit's total flow on the one street cross-section, and the `[STREETS]` section already describes one- or two-sided streets with its Sides item. A street with 2 barrels has no consistent meaning in that model. The better fix is to reject Barrels > 1 on STREET lines in both engines, not to read it. That needs a decision from the maintainers and is not in this patch.

## PR review validation (28 October 2026)

[PR #193](https://github.com/HydroCouple/openswmm.engine/pull/193), head `eaa292d8b3c2cd94f70fa46c7d83221d47a247de`, implements the original patch's IRREGULAR branch. Its Google Test and the original `IO-11_test.c` fail on the uncorrected base and pass on the PR: C1 has **2 barrels carrying 20.00 cfs**, C2 has **1 barrel carrying 10.00 cfs**, and both inlet depths are **0.696 ft**. These tests also pass with PRs #191–#193 combined.

Validation used isolated copies of common PR base `fbd1d7b03c036d7a80e9bb64c9781dc5ccf464c2` plus the submitted changes, on macOS arm64 / Apple Clang 21.0.0, with `-O1 -g`, AddressSanitizer and UndefinedBehaviorSanitizer. Production source blob hashes were checked against GitHub's PR file metadata. The five existing legacy regression programs listed in the [verification follow-up](../../verification.md#prs-191-193-follow-up-28-october-2026) pass **81 tests** on the base and combined changes, using the existing CRASH-01 datetime suppression. Focused reproducers and the new follow-up checks run without that suppression. 6.0.0 is unaffected and was not rebuilt.

**The finding is only partially addressed; PR #193 requires changes before approval.**

1. **The new assignment bypasses PR #191's safety guard.** At [PR head `link.c:207–211`](https://github.com/HydroCouple/openswmm.engine/blob/eaa292d8b3c2cd94f70fa46c7d83221d47a247de/src/legacy/engine/link.c#L207-L211), the new IRREGULAR branch tests the zero-filled type and uses `subIndex` without checking `Link[j].ID`. With all three PRs combined, a rejected orifice row followed by an IRREGULAR cross-section with an explicit barrel count still writes past the zero-length `Conduit` array. ASan reports **heap-buffer-overflow, WRITE of size 4** at the added assignment (combined source line 213). PR #191 alone rejects the same input with ERROR 209 and no sanitizer error. Adding `Link[j].ID &&` to this new condition in a temporary validation build restores safe ERROR 209 rejection; that experiment was reverted after checking. Add the guard and a combined sanitizer regression before merging.
2. **STREET is unchanged.** The STREET branch still returns without reading the barrel item. The new STREET deck opens and runs but its Cross Section Summary reports **C1 = 1 barrel instead of 2**, while C2 correctly reports 1, on both PR #193 and the combined changes. The title, description, changelog and [issue #176](https://github.com/HydroCouple/openswmm.engine/issues/176) claim coverage of both shapes. The original review explicitly deferred STREET because its inlet semantics need a policy decision (see above). Resolve that policy with a test and implementation, or narrow the PR and issue to IRREGULAR and track STREET separately. The submitted PR does not support claiming STREET completed.

### Follow-up reproducers

| File | Check |
|---|---|
| [`IO-11_orifice-bad-node.inp`](IO-11_orifice-bad-node.inp) | CRASH-02's rejected orifice, with a defined IRREGULAR transect and explicit barrel count; no conduits. It must report ERROR 209 without accessing conduit memory. |
| [`IO-11_street-barrels.inp`](IO-11_street-barrels.inp) | Two STREET conduits entered with 2 and 1 barrels, without inlets. Demonstrates the still-ignored input item; it does not establish a multi-barrel inlet formulation. |
| [`IO-11_followup_test.c`](IO-11_followup_test.c) | `guard` checks safe input rejection; `street` checks the entered counts under the PR's claimed read-Barrels behavior. |

Compile the follow-up test against the freshly built PR library with ASan/UBSan, using `tools/compat/530` and the engine's legacy public headers. Copy the three files above into a scratch working directory before running. For example, with `REVIEW_ROOT` set to this repository, `ENGINE_ROOT` to the PR source copy, and `LEGACY_LIB_DIR` to its built library directory:

```sh
clang -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$REVIEW_ROOT/tools/compat/530" \
  -I"$ENGINE_ROOT/include/openswmm/legacy/engine" \
  IO-11_followup_test.c -L"$LEGACY_LIB_DIR" -lopenswmm.legacy.engine \
  -Wl,-rpath,"$LEGACY_LIB_DIR" -o IO-11_followup_test
ASAN_OPTIONS=detect_leaks=0 ./IO-11_followup_test guard
ASAN_OPTIONS=detect_leaks=0 ./IO-11_followup_test street
```

The isolated validation harness names its library `legacy`, so it uses `-llegacy` in place of `-lopenswmm.legacy.engine`. The combined submitted changes abort under ASan for `guard` and return FAIL for `street`; PR #191 alone passes `guard`. These follow-up tests are separate from the original `tools/run-test.sh IO-11` reproducer, whose passing result checks IRREGULAR only. Historical patch files remain unchanged.
