# BND-14: Runoff sent to a subcatchment with zero area disappears

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | All runoff from a subcatchment whose outlet is a subcatchment with Area 0 is lost. It reaches neither that subcatchment's outlet nor the runoff totals. In the test, 0.9 in of rain on 1 acre gives no runoff anywhere and a +95.3 % runoff continuity error. No error or warning is written. Zero-area placeholder subcatchments are common in models converted from GIS. |
| **Reached from** | `[SUBCATCHMENTS]` with Area 0 for a subcatchment that another subcatchment (with area) names as its outlet |
| **5.3.0** | `subcatch_addRunonFlow()` in [`src/legacy/engine/subcatch.c:619`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L619), `runoff_execute()` in [`runoff.c:266`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L266) and `subcatch_getRunoff()` at [`subcatch.c:765`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L765); the area is read at [`subcatch.c:163`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L163) |
| **5.2.4** | Same code, [`src/solver/subcatch.c:614`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L614), [`runoff.c:262`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/runoff.c#L262), [`subcatch.c:743`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L743) |
| **6.0.0** | Reproduces with the same numbers: `add_runon` in `SWMMEngine::assembleRunon()` returns at once for a subcatchment with no area, [`src/engine/core/SWMMEngine.cpp:9657`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L9657) |
| **Since** | Every release |
| **Fix** | Report a new ERROR 106 at validation: [`BND-14_swmm530.patch`](BND-14_swmm530.patch), [`BND-14_swmm600.patch`](BND-14_swmm600.patch) |

## The problem

A subcatchment's outlet can be another subcatchment; its runoff is then spread over that subcatchment as run-on. The parser accepts an Area of 0 (it only rejects negative values), so a model can name a zero-area subcatchment as an outlet. Such placeholders appear in models converted from GIS layers, where a polygon was dropped but its name is still used as an outlet.

Three pieces of code then lose the water between them:

- the upstream subcatchment's runoff is not counted as leaving the study area, because it is expected to be counted downstream;
- the zero-area subcatchment cannot receive run-on, so it is dropped;
- the zero-area subcatchment is skipped by the runoff calculation, so nothing reaches its own outlet.

In the test deck S1 (1 acre) drains onto S2 (Area 0), which drains to outfall O1. 0.9 in of rain falls. The report shows a surface runoff of 0.000 in, nothing at O1, and a runoff continuity error of 95.295 %. No message points to S2.

## Why it happens

```c
// src/legacy/engine/subcatch.c, subcatch_readParams()
for ( i = 3; i < 8; i++)
{
    if ( ! getDouble(tok[i], &x[i]) || x[i] < 0.0 )        // Area 0 accepted
        return error_setInpError(ERR_NUMBER, tok[i]);
}

// src/legacy/engine/subcatch.c, subcatch_addRunonFlow()
if ( Subcatch[subcatchIndex].area <= 0.0 ) return;        // run-on dropped

// src/legacy/engine/runoff.c, runoff_execute()
if ( Subcatch[j].area == 0.0 ) continue;                  // S2 never computed

// src/legacy/engine/subcatch.c, subcatch_getRunoff()
// --- include this subcatchment's contribution to overall flow balance
//     only if its outlet is a drainage system node
if ( Subcatch[subcatchIndex].outNode == -1 && Subcatch[subcatchIndex].outSubcatch != subcatchIndex )
{
    vOutflow = 0.0;                                       // S1's runoff not counted
}
```

6.0.0 does the same: its `add_runon` lambda returns when the receiving subcatchment's area is not positive, and the same `vOutflow` rule applies.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-14_runon-to-zero-area.inp`](BND-14_runon-to-zero-area.inp) | S1 (1 ac, impervious) -> S2 (Area 0) -> outfall O1; 0.9 in/hr for 1 h, no infiltration or evaporation |
| [`BND-14_test.c`](BND-14_test.c) | Opens and runs the deck through the legacy toolkit (5.2.4 and 5.3.0). It passes if the project is rejected with an error, or if it runs with a runoff continuity error below 1 %. |
| [`BND-14_test6.c`](BND-14_test6.c) | The same with the 6.0.0 engine API |

The test accepts either correct behaviour (reject the input, or deliver the water), so it does not depend on the form of the fix.

```sh
tools/run-test.sh BND-14            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-14 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0 and 6.0.0; 5.2.4 gives 95.297 %):

```
The project ran with no error
Runoff continuity error (%)    95.295
FAIL: S1's runoff onto zero-area S2 is lost without a message: runoff continuity error 95.295 %
BND-14 5.2.4 base: FAIL
BND-14 5.3.0 base: FAIL
BND-14 6.0.0 base: FAIL
```

**With the fix**:

```
The project was rejected with error 106:   ERROR 106: Subcatchment S1 drains onto a subcatchment with no area.
PASS: a subcatchment draining onto a zero-area subcatchment is reported as an input error
BND-14 5.3.0 patched: PASS
The project was rejected with error 5
    ERROR 106: Subcatchment S1 drains onto a subcatchment with no area.
PASS: a subcatchment draining onto a zero-area subcatchment is reported as an input error
BND-14 6.0.0 patched: PASS
```

## The fix

`subcatch_validate()` rejects a subcatchment with area whose outlet subcatchment has none:

```diff
+    // --- check that an outlet subcatchment has an area to receive runoff
+    if ( k >= 0 && k != subcatchIndex && Subcatch[subcatchIndex].area > 0.0
+    &&   Subcatch[k].area <= 0.0 )
+        report_writeErrorMsg(ERR_SUBCATCH_NO_AREA, Subcatch[subcatchIndex].ID);
```

with a new message, `ERROR 106: Subcatchment %s drains onto a subcatchment with no area.` Error number 106 is unused in 5.2.4, 5.3.0 and 6.0.0. The 6.0.0 patch adds the same check next to its ERROR 108 check in `SWMMEngine.cpp`, and the same code and message to `ErrorCodes.hpp`/`.cpp`.

A zero-area subcatchment that receives nothing is still accepted, and so is one that only receives water from another zero-area subcatchment, because no water is lost there. An outfall routed onto a zero-area subcatchment is not covered. `runoff_getOutfallRunon()` skips such an outfall, and its discharge stays booked as system outflow, so no water is lost.

Rejecting the input is the minimal fix. The alternative is to pass the run-on through to the zero-area subcatchment's own outlet. That would mean rewriting outlets that the input summary, the toolkit getters and (in 6.0.0) the saved input file all report, or adding a pass-through path to the runoff and routing code.

**Effect on other models.** Twenty-four of the regression decks (`swc/*`) contain zero-area subcatchments, but none is the outlet of a subcatchment with area, so the check does not fire and their results are unchanged. A model that loses water this way now stops with ERROR 106 and must be corrected: give the placeholder an area, or point the upstream subcatchment at the placeholder's outlet.
