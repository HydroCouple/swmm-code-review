# NUM-30: Pervious runoff routed to the impervious area disappears when PctZero = 100

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | The routed share of pervious runoff is removed from the pervious area and never reaches the impervious area or the outlet. 50 % impervious, RouteTo IMPERVIOUS 100 %: 0.504 in of runoff from 1.000 in of rain with no losses, a +46.9 % runoff continuity error. With PctZero = 99.9 the same deck gives 0.978 in. Only the continuity error shows it. |
| **Reached from** | `[SUBAREAS]` with `RouteTo` = `IMPERVIOUS`, `PctRouted` > 0 and `PctZero` = 100 on a partly impervious subcatchment |
| **5.3.0** | `subcatch_getRunon()` in [`src/legacy/engine/subcatch.c:580`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L580), with `fOutlet` set in `subcatch_readSubareaParams()` at [`subcatch.c:288`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L288) |
| **5.2.4** | Same code, [`src/solver/subcatch.c:573`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L573) and [`subcatch.c:268`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L268) |
| **6.0.0** | Reproduces with the same numbers: `RunoffSolver::execute()` in [`src/engine/hydrology/Runoff.cpp:585`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L585) copies the legacy condition |
| **Since** | Every release (the code is the same in 5.0.022) |
| **Fix** | Route the share onto IMPERV0 when IMPERV1 has no area: [`NUM-30_swmm530.patch`](NUM-30_swmm530.patch), [`NUM-30_swmm600.patch`](NUM-30_swmm600.patch) |

## The problem

SWMM splits the impervious part of a subcatchment into IMPERV0 (no depression storage, `PctZero` percent of it) and IMPERV1 (with depression storage, the rest). With `RouteTo IMPERVIOUS`, `PctRouted` percent of the pervious runoff is meant to run onto the impervious area and leave from there.

When `PctZero` is 100, all impervious area is IMPERV0 and IMPERV1 has zero area. The routed share is then lost. In the test deck (1 acre, 50 % impervious, PctZero 100, RouteTo IMPERVIOUS 100 %, no infiltration or evaporation, 1.0 in of rain), the report shows 0.504 in of runoff and 0.027 in of final storage: 0.469 in of the rain has gone. That is essentially all the pervious runoff. The report gives a +46.875 % runoff continuity error and no warning.

The result is discontinuous in `PctZero`: with 99.9 instead of 100 the same deck gives 0.978 in of runoff and -0.518 % (the remainder is [CON-16](../../2-conceptual/CON-16-subarea-runoff-end-of-step-rate/)).

## Why it happens

`subcatch_readSubareaParams()` books only `fOutlet = 1 - PctRouted` of the pervious runoff to the outlet. It does not look at `PctZero`:

```c
// src/legacy/engine/subcatch.c, subcatch_readSubareaParams()
if ( k == TO_IMPERV && Subcatch[j].fracImperv )
{
    Subcatch[j].subArea[PERV].routeTo = k;
    Subcatch[j].subArea[PERV].fOutlet = 1.0 - x[6];
}
```

The other `1 - fOutlet` is supposed to arrive at the next step as inflow to IMPERV1. `subcatch_getRunon()` only does that if IMPERV1 has area:

```c
// src/legacy/engine/subcatch.c, subcatch_getRunon()
// --- Case 2: perv --> imperv
if ( Subcatch[subcatchIndex].fracImperv > 0.0 &&
     Subcatch[subcatchIndex].subArea[PERV].routeTo == TO_IMPERV &&
     Subcatch[subcatchIndex].subArea[IMPERV1].fArea > 0.0 )
{
    q = Subcatch[subcatchIndex].subArea[PERV].runoff;
    Subcatch[subcatchIndex].subArea[IMPERV1].inflow += ...
```

The guard avoids dividing by a zero area, but nothing else picks up the water. It has already left the pervious ponded depth through the routing equation, so it is gone.

6.0.0 has the same condition: `else if (route_mode == 1 && f1 > 0.0)`, with no other branch.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-30_pctzero100-route-to-imperv.inp`](NUM-30_pctzero100-route-to-imperv.inp) | 1 acre, 50 % impervious, PctZero 100, RouteTo IMPERVIOUS 100 %, no infiltration or evaporation, 1 in/hr for 1 h |
| [`NUM-30_test.c`](NUM-30_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0) and checks that runoff plus final surface storage equals the rain |
| [`NUM-30_test6.c`](NUM-30_test6.c) | The same with the 6.0.0 engine API |

```sh
tools/run-test.sh NUM-30            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-30 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 6.0.0 prints the same, 5.2.4 shows a continuity error of 46.876 %):

```
Total precipitation (in)   1.000
Surface runoff (in)        0.504
Final storage (in)         0.027
Runoff + storage (in)      0.531   (must equal the rain)
Continuity error (%)      46.875   (swmm_getMassBalErr: 46.875)
FAIL: runoff 0.504 in + storage 0.027 in = 0.531 in, but 1.000 in of rain fell and there are no losses (continuity error 46.875 %)
NUM-30 5.2.4 base: FAIL
NUM-30 5.3.0 base: FAIL
NUM-30 6.0.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same values):

```
Total precipitation (in)   1.000
Surface runoff (in)        0.977
Final storage (in)         0.028
Runoff + storage (in)      1.005   (must equal the rain)
Continuity error (%)      -0.491   (swmm_getMassBalErr: -0.491)
PASS: runoff 0.977 in + storage 0.028 in = rain 1.000 in, continuity error -0.491 %
NUM-30 5.3.0 patched: PASS
NUM-30 6.0.0 patched: PASS
```

The remaining -0.491 % is the end-of-step runoff booking of [CON-16](../../2-conceptual/CON-16-subarea-runoff-end-of-step-rate/), and is close to the -0.518 % of the PctZero 99.9 deck.

## The fix

When IMPERV1 has no area, the impervious area is all IMPERV0, so the routed runoff goes there:

```diff
     // --- Case 2: perv --> imperv
+    //     (onto IMPERV1, or onto IMPERV0 when no impervious area has
+    //     depression storage, i.e. PctZero = 100)
     if ( Subcatch[subcatchIndex].fracImperv > 0.0 &&
-         Subcatch[subcatchIndex].subArea[PERV].routeTo == TO_IMPERV &&
-         Subcatch[subcatchIndex].subArea[IMPERV1].fArea > 0.0 )
+         Subcatch[subcatchIndex].subArea[PERV].routeTo == TO_IMPERV )
     {
+        i = IMPERV1;
+        if ( Subcatch[subcatchIndex].subArea[IMPERV1].fArea <= 0.0 ) i = IMPERV0;
         q = Subcatch[subcatchIndex].subArea[PERV].runoff;
-        Subcatch[subcatchIndex].subArea[IMPERV1].inflow +=
+        Subcatch[subcatchIndex].subArea[i].inflow +=
             q * (1.0 - Subcatch[subcatchIndex].subArea[PERV].fOutlet) *
             Subcatch[subcatchIndex].subArea[PERV].fArea /
-            Subcatch[subcatchIndex].subArea[IMPERV1].fArea;
+            Subcatch[subcatchIndex].subArea[i].fArea;
     }
```

`fracImperv > 0` with no IMPERV1 area means IMPERV0 has all of it, so the division is safe. The 6.0.0 patch adds the same branch (`else if (route_mode == 1 && f0 > 0.0)`, onto `runon_imperv0`).

Another option would be to treat `PctZero = 100` as `RouteTo OUTLET`, as the code already does for a 0 % or 100 % impervious subcatchment. That also conserves water but drops the delay of flowing over the impervious area, which the user asked for. The patch keeps it.

**Effect on other models.** Only subcatchments with `RouteTo IMPERVIOUS` and no IMPERV1 area are affected. None of the regression decks routes pervious runoff to the impervious area, so none of their results can change.
