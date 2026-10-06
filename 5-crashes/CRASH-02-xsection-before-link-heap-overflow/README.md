# CRASH-02: An [XSECTIONS] row read before its link's own row writes past the conduit array

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
