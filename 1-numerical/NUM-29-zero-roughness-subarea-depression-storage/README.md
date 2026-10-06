# NUM-29: A subarea with Manning's n = 0 counts its depression-storage fill again as runoff

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | In the runoff step in which depression storage fills, the volume that just went into storage is also booked as runoff. One 0.9 in storm on 0.5 in of storage gives 0.450 in of runoff instead of 0.400 in (+12.5 %), and a runoff continuity error of -5.56 %. The continuity error is the only sign. |
| **Reached from** | `[SUBAREAS]` with `N-Imperv` or `N-Perv` = 0 (accepted, and documented as "no routing") and non-zero depression storage on that subarea, whenever storage fills part-way through a runoff step |
| **5.3.0** | `getSubareaRunoff()` in [`src/legacy/engine/subcatch.c:1007`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L1007) and `findSubareaRunoff()` at [`subcatch.c:1068`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L1068) |
| **5.2.4** | Same code, [`src/solver/subcatch.c:974`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L974) and [`subcatch.c:1035`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L1035) |
| **6.0.0** | Reproduces with the same numbers: `RunoffSolver::execute()` in [`src/engine/hydrology/Runoff.cpp:523`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L523) copies the legacy code |
| **Since** | Every 5.1 and 5.2 release: the rate is divided by the post-fill time since 5.0, and the volume has been booked over the whole step since 5.1.001 |
| **Fix** | Divide the excess by the full step: [`NUM-29_swmm530.patch`](NUM-29_swmm530.patch), [`NUM-29_swmm600.patch`](NUM-29_swmm600.patch) |

## The problem

With Manning's n = 0, SWMM does not route a subarea's ponded water: whatever is above depression storage at the end of the step leaves within that step, and the depth goes back to the depression storage. That is correct in every step except the one in which depression storage fills.

In the test deck 0.9 in/hr falls for one hour on a 1-acre pervious subcatchment with n = 0, 0.5 in of depression storage, no infiltration and no evaporation. Storage fills at 33.3 min, 3.3 min into the 30-35 min runoff step. The excess at the end of that step is 0.025 in, but the step's runoff is booked as 0.075 in: the extra 0.050 in is exactly the water that went into depression storage during the step, and it is also kept there. Over the storm the runoff is 0.450 in instead of 0.9 - 0.5 = 0.400 in. The report shows the inconsistency only as a -5.556 % runoff continuity error.

The error is one step's worth of storage filling, so it depends on where in the step storage fills. It is zero only if storage fills exactly at a step boundary (1.2 in/hr in the same deck: 0.700 in, error 0.000 %).

## Why it happens

`updatePondedDepth()` first fills depression storage and shortens the remaining time `tx` by the time that took. With n = 0 the routing coefficient `Alpha` is 0, so the rest of the inflow is simply added to the depth, and `tx` is returned as `tRunoff`:

```c
// src/legacy/engine/subcatch.c, updatePondedDepth()
dx = Dstore - subarea->depth;
if ( dx > 0.0 && ix > 0.0 )
{
    tx -= dx / ix;                    // time left after storage is full
    subarea->depth = Dstore;
}
...
    subarea->depth += ix * tx;        // Alpha == 0 when n == 0
...
*dt = tx;
```

`findSubareaRunoff()` converts the excess depth into a rate over that shortened time, which is just the inflow rate `ix`:

```c
// src/legacy/engine/subcatch.c, findSubareaRunoff()
// --- case where no routing is used (Mannings N = 0)
runoff = xDepth / tRunoff;            // = ix * tx / tx
subarea->depth = Dstore;
```

The caller then books that rate over the whole step:

```c
// src/legacy/engine/subcatch.c, getSubareaRunoff()
runoff = findSubareaRunoff(subarea, tRunoff);
Voutflow += subarea->fOutlet * runoff * area * tStep;     // ix * tStep, not ix * tx
```

`Voutflow / tStep` is also the runoff rate sent to the outlet, so the outlet receives the same extra volume. The surplus `ix * (tStep - tx)` is the depth `Dstore - d_old` that went into storage.

6.0.0 has the same three steps in `RunoffSolver::execute()` (`runoff_rate = xDepth / t_runoff;` and `Voutflow += fOutlet * runoff_rate * subarea_area * dt;`).

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-29_n0-storage-fills-mid-step.inp`](NUM-29_n0-storage-fills-mid-step.inp) | 1 acre, 100 % pervious, N-Perv = 0, S-Perv = 0.5 in, no infiltration or evaporation, 0.9 in/hr for 1 h, 5-min wet step |
| [`NUM-29_test.c`](NUM-29_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0) and reads the runoff continuity table of the report |
| [`NUM-29_test6.c`](NUM-29_test6.c) | The same with the 6.0.0 engine API |

```sh
tools/run-test.sh NUM-29            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-29 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** all three engines give the same numbers (5.3.0 shown):

```
                         report   expected
Total precipitation (in)   0.900     0.900
Surface runoff (in)        0.450     0.400
Final storage (in)         0.500     0.500
Continuity error (%)      -5.556     0.000  (swmm_getMassBalErr: -5.556)
FAIL: N = 0 subarea produced 0.450 in of runoff from 0.900 in of rain and 0.500 in of storage (expected 0.400 in); continuity error -5.556 %
NUM-29 5.2.4 base: FAIL
NUM-29 5.3.0 base: FAIL
NUM-29 6.0.0 base: FAIL
```

**With the fix**:

```
Surface runoff (in)        0.400     0.400
Final storage (in)         0.500     0.500
Continuity error (%)       0.000     0.000  (swmm_getMassBalErr: 0.000)
PASS: runoff = rain - depression storage = 0.400 in, continuity error 0.000 %
NUM-29 5.3.0 patched: PASS
```

6.0.0 prints the same values (its report shows the continuity error as `-0.000`) and `NUM-29 6.0.0 patched: PASS`.

## The fix

The excess leaves within the step, so its rate over the step is the excess divided by the step:

```diff
     // --- compute runoff based on updated ponded depth
-    runoff = findSubareaRunoff(subarea, tRunoff);
+    //     (an N = 0 subarea's excess leaves within this step, so its
+    //     rate is averaged over the full step tStep, not over tRunoff)
+    runoff = findSubareaRunoff(subarea, tStep);
```

`findSubareaRunoff()` only uses its time argument for n = 0, and `tRunoff` differs from `tStep` only in the step in which depression storage fills, so every other step is unchanged. In that step the subarea's runoff rate falls from the rainfall rate to the excess spread over the step (0.015 to 0.005 in/min in the test). The 6.0.0 patch makes the same change (`runoff_rate = xDepth / dt;`).

**Effect on other models.** Of the regression decks, three have an n = 0 subarea with depression storage. Base and patched 5.3.0 and 6.0.0 command-line runs give (5.3.0 and 6.0.0 identical to each other in every case):

| Deck | Surface runoff, in (base -> patched) | Runoff continuity error, % |
|---|---|---|
| `swc/swc15.inp` | 9.898 -> 9.879 | -0.273 -> -0.237 |
| `swc/swc17.inp` | 8.701 -> 8.697 | -0.203 -> -0.195 |
| `update_v5111/catchment_as_outfall.inp` | 0.140 -> 0.140 | -0.142 -> -0.142 |

Routing continuity is unchanged in all three.
