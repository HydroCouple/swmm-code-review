# CRASH-02: An [XSECTIONS] row read before its link's own row writes past the conduit array

**Status: not fixed. PR #191 was closed without merging on 16 November 2026 and issue #178 closed as not planned: the defect is in the bundled legacy 5.3.0 solver only, which is kept at parity with EPA SWMM 5.2.4 (same behavior); 6.0.0 is not affected.** [Issue #178](https://github.com/HydroCouple/openswmm.engine/issues/178) · [Engine PR #191](https://github.com/HydroCouple/openswmm.engine/pull/191) · reviewed head `ac5d4306acc9b4d94faca3b6daf9b79ddce9f9c1` (validated 28 October 2026). OpenSWMM 6.0.0 is not affected.

The reproduction below records the original reviewed revisions; the follow-up validation at the end records the submitted PR.

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Heap write out of bounds while the input is read, in models without conduits (only orifices, weirs, pumps or outlets): the run stops with an empty report instead of the input error, or a valid model crashes. With conduits present the same write lands on the first conduit's barrel count; that silent variant is [IO-16](../../4-io/IO-16-link-properties-reset-by-section-order/). |
| **Reached from** | An `[XSECTIONS]` row for a link whose own row was rejected (e.g. an undefined node, ERROR 209) or comes later in the file (`[XSECTIONS]` above `[WEIRS]`, which the format allows) |
| **5.3.0** | `link_readXsectParams()` in [`src/legacy/engine/link.c:193`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L193) and [`link.c:254-258`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L254-L258); the array is allocated with `Nlinks[CONDUIT]` elements in [`project.c:1051`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L1051) |
| **5.2.4** | Same code, [`src/solver/link.c:190`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L190) and [`link.c:251-255`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L251-L255) |
| **6.0.0** | Not affected: rows that name an object declared further down are replayed after all sections have been read ([`src/engine/input/InputReader.cpp:184`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/InputReader.cpp#L184)), and a regulator's cross-section never touches conduit data |
| **Since** | Every release in the repository's history (the line is in its initial commit, 2014) |
| **Fix** | Write the barrels only once the link's own row is stored: [`CRASH-02_swmm530.patch`](CRASH-02_swmm530.patch) |

## The problem

The legacy reader stores each section's rows as it meets them in the file. An `[XSECTIONS]` row looks at the link's type to decide whether it is a conduit, and if so sets the conduit's number of barrels:

```c
// src/legacy/engine/link.c, link_readXsectParams()
    // --- assign default number of barrels to conduit
    if ( Link[j].type == CONDUIT ) Conduit[Link[j].subIndex].barrels = 1;
    ...
        // --- parse number of barrels if present
        if ( Link[j].type == CONDUIT && ntoks >= 7 )
        {
            i = atoi(tok[6]);
            if ( i <= 0 ) return error_setInpError(ERR_NUMBER, tok[6]);
            else Conduit[Link[j].subIndex].barrels = (char)i;
        }
```

`Link[j].type` and `Link[j].subIndex` are set by the link's own row, through `link_setParams()`. Until then they are the zeros that `calloc()` left, and `CONDUIT` is 0. So for any link whose own row has not been stored, the row writes `Conduit[0].barrels`. That happens in two ways:

- **The link's row was rejected.** In the test, an orifice row names the outfall `O1x` instead of `O1`. `orifice_readParams()` returns ERROR 209 before it stores anything, and the `[XSECTIONS]` row that follows in standard order treats the orifice as conduit 0.
- **The link's row comes later.** The input reference says sections may appear in any order. In the test, `[XSECTIONS]` is placed above `[WEIRS]`. 6.0.0's own test deck `tests/unit/engine/data/lid_chain_mass_balance.inp` has this order.

When the model has no conduits, `Conduit` is a zero-length allocation and the write is out of bounds. The input error that should have been reported (ERROR 209) never reaches the report, which is empty. A valid regulator-only model whose `[XSECTIONS]` is not at the end crashes.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-02_orifice-bad-node.inp`](CRASH-02_orifice-bad-node.inp) | Storage S1 drains through orifice OR1 to outfall O1; the orifice row names `O1x`; no conduits; standard section order |
| [`CRASH-02_xsect-before-weir.inp`](CRASH-02_xsect-before-weir.inp) | 5 cfs into storage SU1, which drains over weir W1 to outfall O1; `[XSECTIONS]` above `[WEIRS]`; no conduits |
| [`CRASH-02_test.c`](CRASH-02_test.c) | Expects ERROR 209 for the first deck, and for the second a run in which the weir passes the 5 cfs inflow at the end |
| [`CRASH-02_test6.c`](CRASH-02_test6.c) | The same through the 6.0.0 API |
| [`CRASH-02_swmm530.patch`](CRASH-02_swmm530.patch) | The fix for 5.3.0 |

```sh
tools/run-test.sh CRASH-02            # 5.2.4 and 5.3.0: CRASH, 6.0.0: PASS
tools/run-test.sh CRASH-02 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 the same, at `link.c:190`), on the first deck:

```
CRASH-02_orifice-bad-node.inp:
=================================================================
==31078==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x5020000000a0 at pc ...
WRITE of size 1 at 0x5020000000a0 thread T0
    #0 ... in link_readXsectParams .../src/legacy/engine/link.c:193:70
    #1 ... in parseLine .../src/legacy/engine/input.c:558:16
...
0x5020000000a0 is located 15 bytes after 1-byte region [0x502000000090,0x502000000091)
...
CRASH-02 5.2.4 base: CRASH
CRASH-02 5.3.0 base: CRASH
```

The second deck stops at the same line when run on its own (`openswmm-legacy CRASH-02_xsect-before-weir.inp ...`).

**6.0.0, and 5.3.0 with the fix:**

```
CRASH-02_orifice-bad-node.inp:
    swmm_open returned 200
    report: ERROR 209: undefined object O1x at line 26 of [ORIFICE] section:
CRASH-02_xsect-before-weir.inp:
    swmm_open returned 0
    W1 flow at the end 5.0000 cfs (inflow 5 cfs), run returned 0
PASS: no overflow; the bad orifice row gives ERROR 209 and the weir model runs with [XSECTIONS] first
CRASH-02 5.3.0 patched: PASS
```

```
CRASH-02_orifice-bad-node.inp:
    swmm_engine_open returned 5
    report: ERROR 209: undefined object O1x.
CRASH-02_xsect-before-weir.inp:
    swmm_engine_open returned 0
    W1 flow at the end 5.0000 cfs (inflow 5 cfs), run returned 0
PASS: the bad orifice row gives ERROR 209 and the weir model runs with [XSECTIONS] first
CRASH-02 6.0.0 base: PASS
```

## The fix

`Link[j].ID` is set by the link's own row, in the same statement block that calls `link_setParams()`, and only when that row is accepted. The patch writes conduit data only when it is set:

```diff
     // --- assign default number of barrels to conduit
-    if ( Link[j].type == CONDUIT ) Conduit[Link[j].subIndex].barrels = 1;
+    //     (Link[j].ID is set when the link's own data line is read; until
+    //     then its type and sub-index are only the zeroed defaults)
+    if ( Link[j].ID && Link[j].type == CONDUIT ) Conduit[Link[j].subIndex].barrels = 1;
 ...
         // --- parse number of barrels if present
-        if ( Link[j].type == CONDUIT && ntoks >= 7 )
+        if ( Link[j].ID && Link[j].type == CONDUIT && ntoks >= 7 )
```

The cross-section itself is still stored on the link, so a regulator's `[XSECTIONS]` row may come before its section, as the format allows, and the weir deck runs. A rejected row's cross-section is ignored, and the input error is reported.

A conduit whose `[XSECTIONS]` row comes before `[CONDUITS]` still cannot get its barrels: the conduit index is not known until its row is read. Before the fix, all those rows wrote the first conduit's barrels and every other conduit failed with ERROR 114 (`invalid number of barrels`); with the fix every such conduit fails with ERROR 114. A model with a single conduit and this order used to run, because its barrels landed on the right conduit by coincidence, and now stops with ERROR 114 (checked with the patched CLI). That case is described in [IO-16](../../4-io/IO-16-link-properties-reset-by-section-order/).

**Effect on other models.** None of the 73 regression decks has `[XSECTIONS]` above a link section. Among 6.0.0's own test decks that do, `structures_dw.inp` and `_speed_PUMP3.inp` give the same report with the patched 5.3.0 CLI, and `lid_chain_mass_balance.inp` no longer crashes; it now stops with input errors for its 6.0.0-only `[LID_CONTROL]` layers.

## Notes

- [IO-11](../../4-io/IO-11-irregular-street-barrels-ignored/) adds a third barrels assignment, for irregular and street sections, with the same `Link[j].type == CONDUIT` test. When both patches are applied it needs the same `Link[j].ID &&` guard.

## Fix validation (28 October 2026)

[PR #191](https://github.com/HydroCouple/openswmm.engine/pull/191), head `ac5d4306acc9b4d94faca3b6daf9b79ddce9f9c1`, implements the two `Link[j].ID` guards from the review patch. Source inspection confirms the ID is stored only with an accepted link row. The correction therefore prevents indexing `Conduit` through an uninitialized link while retaining the regulator cross-section for later use.

Validation used isolated copies of PR base `fbd1d7b03c036d7a80e9bb64c9781dc5ccf464c2` plus the submitted changes, on macOS arm64 / Apple Clang 21.0.0, with `-O1 -g`, AddressSanitizer and UndefinedBehaviorSanitizer. Production source blob hashes were checked against GitHub's PR file metadata; the active engine checkout was not used for the builds.

- On the uncorrected base, the PR test program and original `CRASH-02_test.c` abort with an ASan heap-buffer-overflow during input reading.
- With PR #191, both added Google Tests and the original reproducer pass without sanitizer errors. The invalid orifice reports ERROR 209; the valid out-of-order weir model runs and passes **5.0000 cfs**.
- Five existing legacy test programs (`test_solver_api`, `test_solver_errors`, `test_solver_shapes`, `test_solver_storage_shapes`, `test_solver_transect_nc`) pass **81 Google Tests** on both the base and PR #191. These broader runs use the review's existing `tools/ubsan.supp` for the unrelated CRASH-01 datetime conversion; the focused reproducers run without that suppression.
- The original two CRASH-02 cases also pass when PRs #191, #192 and #193 are combined.

**Merge interaction still requiring action in PR #193:** IO-11 adds a third `Conduit[].barrels` write without the `Link[j].ID` guard. The new [IO-11 rejected-link follow-up](../../4-io/IO-11-irregular-street-barrels-ignored/IO-11_orifice-bad-node.inp) reports ERROR 209 safely with PR #191 alone but triggers an ASan heap-buffer-overflow after combining the three PRs. Applying the same guard to that added write removes the overflow and restores ERROR 209. PR #191's own correction is confirmed; PR #193 must preserve it before the three changes can be approved together. The separate IO-16 conduit-order limitation documented above remains outside this fix.

6.0.0 is unaffected and was not rebuilt for this follow-up. The original patches and historical reproduction results are preserved.

## Outcome (16 November 2026)

[PR #191](https://github.com/HydroCouple/openswmm.engine/pull/191) was closed without merging and [issue #178](https://github.com/HydroCouple/openswmm.engine/issues/178) closed as not planned by the maintainers: the defect is in the bundled legacy 5.3.0 solver only, which is kept at parity with EPA SWMM 5.2.4 (same behavior); 6.0.0 is not affected (checked on `swmm6_rel` 87664bf5 with AddressSanitizer and UBSan, per the closing comments). The validation above stands as the record of the submitted correction.
