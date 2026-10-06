# IO-16: Link properties in a section placed above the link's own section are lost or land on another link

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Silent wrong results when sections are not in the order the GUI writes them. A flap gate given in `[LOSSES]` above `[CONDUITS]` is dropped: in the test 5.8 cfs flows backwards and the upstream junction fills to its rim. A regulator's `[XSECTIONS]` row above its own section resets the first conduit's barrels to 1: in the test the upstream junction's peak depth is 10.0 ft instead of 3.9 ft. No warning. |
| **Reached from** | Input files whose `[LOSSES]` comes before `[CONDUITS]`, or whose `[XSECTIONS]` comes before `[ORIFICES]`, `[WEIRS]`, `[PUMPS]` or `[OUTLETS]`. The input reference says the sections may appear in any order |
| **5.3.0** | `link_setParams()` in [`src/legacy/engine/link.c:340`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L340) resets the flap gate that `link_readLossParams()` set at [`link.c:311`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L311); `link_readXsectParams()` at [`link.c:193`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L193) writes the barrels of conduit 0 |
| **5.2.4** | Same code, [`src/solver/link.c:337`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L337), [`link.c:308`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L308) and [`link.c:190`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L190) |
| **6.0.0** | Not affected: rows that name an object declared further down are replayed after all sections have been read ([`src/engine/input/InputReader.cpp:184`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/InputReader.cpp#L184)) |
| **Since** | Every release in the repository's history (both lines are in its initial commit, 2014) |
| **Fix** | Do not reset the flap gate in `link_setParams()`: [`IO-16_swmm530.patch`](IO-16_swmm530.patch), on top of [CRASH-02](../../5-crashes/CRASH-02-xsection-before-link-heap-overflow/)'s patch, which fixes the barrels |

## The problem

The input reference says: "The sections can appear in any arbitrary order in the input file". The legacy reader stores each row as it meets it, and two kinds of link data depend on the link's own row having been stored first.

**Flap gates in `[LOSSES]`.** `link_readLossParams()` sets `Link[j].hasFlapGate`. When the conduit's own row is read later, `link_setParams()` initialises the link and sets `hasFlapGate` back to 0. In the test, conduit C1 drains junction J1 to an outfall whose fixed stage is 2 ft above J1's invert, and C1 has a flap gate. With `[LOSSES]` above `[CONDUITS]`, the gate is gone: 5.8 cfs flows back through C1, J1 fills to its 10-ft rim, and the routing continuity error is −4.3 %. With the usual order, nothing flows. 6.0.0's own round-trip test deck `tests/unit/engine/data/inp_roundtrip/flap_gate_spellings.inp` has `[LOSSES]` above `[CONDUITS]`.

**`[XSECTIONS]` rows for regulators.** `link_readXsectParams()` sets the conduit's barrels when `Link[j].type == CONDUIT`. Before a link's own row is read, its type and sub-index are the zeros `calloc()` left, which read as "conduit 0". So the row of a weir, orifice, pump or outlet whose section comes later sets `Conduit[0].barrels = 1`, overwriting the value from the first conduit's own `[XSECTIONS]` row. In the test, C1 has 2 barrels; with `[XSECTIONS]` between `[CONDUITS]` and `[WEIRS]`, weir W1's row sets them back to 1 and J1 surcharges. With no conduits at all the same write is out of bounds: that is [CRASH-02](../../5-crashes/CRASH-02-xsection-before-link-heap-overflow/). Several of 6.0.0's test decks (`structures_dw.inp`, `fv_structures.inp`, the `_speed_PUMP*.inp` decks) have `[XSECTIONS]` above the regulator sections.

## Why it happens

```c
// src/legacy/engine/link.c, link_readLossParams()
    Link[j].hasFlapGate  = k;

// src/legacy/engine/link.c, link_setParams()
    Link[j].targetSetting = 1.0;
    Link[j].hasFlapGate = 0;
    Link[j].qLimit      = 0.0;         // 0 means that no limit is defined
```

`link_setParams()` runs once per link, from the link's own row. For a conduit nothing else sets `hasFlapGate` again; pumps set it to FALSE and orifices, weirs and outlets set it from their own rows in the `switch` that follows.

```c
// src/legacy/engine/link.c, link_readXsectParams()
    // --- assign default number of barrels to conduit
    if ( Link[j].type == CONDUIT ) Conduit[Link[j].subIndex].barrels = 1;
```

`Link[j].type` and `Link[j].subIndex` are set only by `link_setParams()`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-16_losses-first.inp`](IO-16_losses-first.inp) | J1 → C1 (flap gate) → outfall O1 at a fixed stage 2 ft above J1's invert; `[LOSSES]` above `[CONDUITS]` |
| [`IO-16_xsect-above-weirs.inp`](IO-16_xsect-above-weirs.inp) | 5 cfs into J1 → C1 (1 ft, 2 barrels) → J2 → weir W1 → O1; `[XSECTIONS]` between `[CONDUITS]` and `[WEIRS]` |
| [`IO-16_xsect-below-weirs.inp`](IO-16_xsect-below-weirs.inp) | The same model with `[XSECTIONS]` after `[WEIRS]` |
| [`IO-16_test.c`](IO-16_test.c) | A: expects no flow in C1 and a dry J1 at every step. B: runs both orders and compares J1's depth and C1's flow step by step |
| [`IO-16_test6.c`](IO-16_test6.c) | The same through the 6.0.0 API |
| [`IO-16_swmm530.patch`](IO-16_swmm530.patch) | The flap-gate fix (requires [`CRASH-02_swmm530.patch`](../../5-crashes/CRASH-02-xsection-before-link-heap-overflow/CRASH-02_swmm530.patch)) |

```sh
tools/run-test.sh IO-16            # 5.2.4 and 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh IO-16 --patched  # 5.3.0 with CRASH-02 and IO-16: PASS
```

**Without the fix** (5.2.4 prints the same):

```
A. [LOSSES] above [CONDUITS], flap gate on C1 (run returned 0)
   largest |C1 flow| 5.822 cfs, largest J1 depth 10.000 ft (both should be 0)

B. the same model with [XSECTIONS] below or above [WEIRS] (runs returned 0)
                          steps   J1 max depth (ft)   C1 max flow (cfs)
   [XSECTIONS] below       1440               3.911               5.017
   [XSECTIONS] above       1440              10.000               5.002
   largest difference at the same step: J1 depth 9.414173 ft, C1 flow 2.552466 cfs
FAIL: section order changes the results: the flap gate in [LOSSES] is lost (reverse flow through C1); [XSECTIONS] above [WEIRS] changes J1's depth (C1 loses a barrel)
IO-16 5.2.4 base: FAIL
IO-16 5.3.0 base: FAIL
```

**With the fix** (5.3.0 with CRASH-02 and IO-16; 6.0.0 unpatched gives the same numbers, with one more step):

```
A. [LOSSES] above [CONDUITS], flap gate on C1 (run returned 0)
   largest |C1 flow| 0.000 cfs, largest J1 depth 0.000 ft (both should be 0)

B. the same model with [XSECTIONS] below or above [WEIRS] (runs returned 0)
                          steps   J1 max depth (ft)   C1 max flow (cfs)
   [XSECTIONS] below       1440               3.911               5.017
   [XSECTIONS] above       1440               3.911               5.017
   largest difference at the same step: J1 depth 0.000000 ft, C1 flow 0.000000 cfs
PASS: the flap gate holds and the results do not depend on the section order
IO-16 5.3.0 patched: PASS
IO-16 6.0.0 base: PASS
```

## The fix

The flap gate: `link_setParams()` no longer resets it. `Link[]` is zero-filled when it is allocated, and every non-conduit type still sets `hasFlapGate` from its own row, so only a conduit's `[LOSSES]` value is affected, and it is now kept whatever the order.

```diff
     Link[j].targetSetting = 1.0;
-    Link[j].hasFlapGate = 0;
+    // (hasFlapGate is left as is: a [LOSSES] line read earlier may have set it)
     Link[j].qLimit      = 0.0;         // 0 means that no limit is defined
```

The barrels: [CRASH-02](../../5-crashes/CRASH-02-xsection-before-link-heap-overflow/)'s patch makes `link_readXsectParams()` write conduit data only once the link's own row has been stored (`Link[j].ID` is set), so a regulator's row no longer touches conduit 0. This patch declares `Requires: CRASH-02`.

**Not fixed: `[XSECTIONS]` above `[CONDUITS]`.** A conduit's index is assigned when its row is read, so a cross-section row that comes first cannot know which conduit's barrels to set. 5.3.0 reports ERROR 114 (`invalid number of barrels`) for such conduits: before the fixes for every conduit but the first, with CRASH-02's patch for all of them. That is an error message, not a wrong result. Supporting this order would need the reader to store `[XSECTIONS]` after the link sections, as 6.0.0 does.

**Effect on other models.** With both patches, `extran1.inp`, `user3.inp` and `test2.inp` (regression decks with `[LOSSES]` in the usual place) give byte-identical `.rpt` (apart from the run times) and `.out` files. `flap_gate_spellings.inp` (from 6.0.0's tests, `[LOSSES]` above `[CONDUITS]`) gives the same report but a different `.out` file, because its flap gates now take effect.

## Notes

- `[CONTROLS]` has the same dependence: a rule clause such as `PUMP P1 STATUS = ON` checks `Link[j].type` when it is read, so a `[CONTROLS]` section above `[PUMPS]` gives `ERROR 209: undefined object P1` (`controls.c:1446`). Not fixed here.
