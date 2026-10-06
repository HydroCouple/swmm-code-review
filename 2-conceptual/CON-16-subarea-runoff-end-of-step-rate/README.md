# CON-16: Subarea runoff is booked as the end-of-step rate times the step, not as the water that left

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | The runoff volume sent to the outlet is not the water that left the ponded surface, so the runoff continuity error grows with the wet step. A 1-acre impervious area with no storage or losses gives 2.011 in of runoff from 2.000 in of rain at a 5-min wet step (-0.59 %), and 2.062 in at 15 min (-3.1 %). The drainage system receives the extra water. Only the runoff continuity error shows it. |
| **Reached from** | Every subarea with Manning's n > 0 and a non-zero slope, i.e. practically every subcatchment, whenever the runoff rate changes within a step |
| **5.3.0** | `getSubareaRunoff()` in [`src/legacy/engine/subcatch.c:1012`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L1012), with the rate from `findSubareaRunoff()` at [`subcatch.c:1062`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L1062) |
| **5.2.4** | Same code, [`src/solver/subcatch.c:979`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L979) and [`subcatch.c:1029`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L1029) |
| **6.0.0** | Reproduces with the same numbers: `RunoffSolver::execute()` in [`src/engine/hydrology/Runoff.cpp:521`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L521) and [`Runoff.cpp:549`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L549) |
| **Since** | Every release: 5.0 booked the average of the start- and end-of-step rates, 5.1.001 onwards the end-of-step rate. The reference manual describes it (Vol. I, section 3.4, step c: "Compute the runoff per unit area q at the end of the time step"). |
| **Fix** | Use the depth the routing equation removed, divided by the step: [`CON-16_swmm530.patch`](CON-16_swmm530.patch), [`CON-16_swmm600.patch`](CON-16_swmm600.patch). It conserves water but lowers peak flows; see [The fix](#the-fix). |

## The problem

Each runoff step, SWMM integrates the nonlinear-reservoir equation for each subarea's ponded depth,

    dd/dt = i_x - alpha (d - d_s)^(5/3)

with an adaptive Runge-Kutta solver. The depth at the end of the step therefore already reflects the water that ran off during the step: `d_old + i_x dt - d_new`. SWMM does not use that volume. It takes the runoff rate at the end of the step, `alpha (d_new - d_s)^(5/3)`, and books it over the whole step. It sends the same rate to the drainage system and uses it for the runoff volume in the mass balance and in the subcatchment statistics.

On a rising hydrograph the end-of-step rate is the largest rate of the step, so too much is booked. On a falling one it is the smallest, so too little is booked. The two do not cancel: a sharp rise to near-equilibrium within a step is overstated by almost a full step of flow, while the long recession is understated by little. The test deck is a 1-acre impervious area (n = 0.015, width 100 ft, 1 % slope) with no depression storage, no infiltration and no evaporation, under 2 in/hr for 1 hour:

| WET_STEP | Surface runoff (in) | Runoff continuity error |
|---|---|---|
| 1 min | 2.000 | -0.026 % |
| 5 min | 2.011 | -0.585 % |
| 15 min | 2.062 | -3.120 % |
| 30 min | 2.072 | -3.623 % |

More runoff leaves the surface than rain fell on it, and the routing model receives that volume as wet-weather inflow. In regression decks with ordinary inputs the effect is a few tenths of a percent (-0.27 % in `Example1`, -0.41 % in `user1`). Depression storage that fills during a step makes it worse: the ODE then runs only for the part of the step after storage is full, but the end-of-step rate is still booked for the whole step.

## Why it happens

```c
// src/legacy/engine/subcatch.c, getSubareaRunoff()
subarea->inflow -= surfEvap + infil;
updatePondedDepth(subarea, &tRunoff);            // integrates d over the step
...
runoff = findSubareaRunoff(subarea, tRunoff);    // Alpha * pow(d - Dstore, 5/3) at the END
...
Voutflow += subarea->fOutlet * runoff * area * tStep;
return runoff;
```

`Voutflow / tStep` becomes `Subcatch.newRunoff`, the flow the drainage system receives. The depth state and the booked outflow disagree by `(runoff * tStep) - (d_old + i_x * tStep - d_new)` every step. Evaporation and infiltration are step-average rates, so the subarea's water balance closes for every term except runoff.

6.0.0 does the same (`runoff_rate = alpha * std::pow(xDepth, MEXP)` after `updatePondedDepth()`, then `Voutflow += fOutlet * runoff_rate * subarea_area * dt`).

## How to reproduce

| File | What it is |
|---|---|
| [`CON-16_impervious-roof.inp`](CON-16_impervious-roof.inp) | 1 acre, 100 % impervious, no depression storage, no infiltration or evaporation, 2 in/hr for 1 h |
| [`CON-16_test.c`](CON-16_test.c) | Writes the deck with WET_STEP = 1, 5, 15 and 30 min (`CON-16_wetNN.inp`), runs each through the legacy toolkit (5.2.4 and 5.3.0) and checks that runoff + final storage = rain within 0.1 % |
| [`CON-16_test6.c`](CON-16_test6.c) | The same with the 6.0.0 engine API |

```sh
tools/run-test.sh CON-16            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-16 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0 and 6.0.0; 5.2.4 prints -0.586, -3.119 and -3.614 % at 5, 15 and 30 min):

```
WET_STEP   rain    runoff   final storage   continuity error
  (min)    (in)    (in)     (in)            (%)
    1      2.000   2.000    0.000            -0.026
    5      2.000   2.011    0.000            -0.585
   15      2.000   2.062    0.000            -3.120
   30      2.000   2.072    0.000            -3.623
FAIL: runoff + storage differs from the rain by more than 0.1 % for 3 of 4 wet steps
CON-16 5.2.4 base: FAIL
CON-16 5.3.0 base: FAIL
CON-16 6.0.0 base: FAIL
```

**With the fix** (6.0.0 prints `-0.000` at 1 and 5 min):

```
WET_STEP   rain    runoff   final storage   continuity error
  (min)    (in)    (in)     (in)            (%)
    1      2.000   2.000    0.000             0.000
    5      2.000   2.000    0.000             0.000
   15      2.000   2.000    0.000             0.000
   30      2.000   2.000    0.000             0.000
PASS: runoff + storage = rain within 0.1 % for every wet step
CON-16 5.3.0 patched: PASS
CON-16 6.0.0 patched: PASS
```

## The fix

For a subarea with routing (n > 0 and alpha > 0), the runoff over the step is the depth the ODE removed, divided by the step:

```diff
     else
     {
         subarea->inflow -= surfEvap + infil;
+        dNoRunoff = subarea->depth + subarea->inflow * tStep;
         updatePondedDepth(subarea, &tRunoff);
     }
 ...
+    //     (with routing, the runoff over the step is the depth that left the
+    //     subarea while its ponded depth was integrated, not the end-of-step
+    //     rate times the step)
+    if ( subarea->N > 0.0 && Alpha > 0.0 )
+        runoff = MAX(0.0, dNoRunoff - subarea->depth) / tStep;
     Voutflow += subarea->fOutlet * runoff * area * tStep;
```

The value replaces the rate everywhere it is used: the flow to the outlet, the mass balance, the subcatchment statistics, and the transfer to another subarea under `RouteTo`. A step without outflow is unchanged, because the depth update without the ODE is the same expression and the difference is exactly 0. Subareas with n = 0 are left alone; [NUM-29](../../1-numerical/NUM-29-zero-roughness-subarea-depression-storage/) fixes their own step-average. The 6.0.0 patch makes the same change in `RunoffSolver::execute()`, and both patched engines print the same values.

**This is a trade-off, not a free correction.** The step-average rate is assigned to the end of the step, as the end-of-step rate was. It is the average over the step that has just passed, so the hydrograph the drainage system receives is smoothed and shifted by about half a step, and peak flows fall. Volumes become exact.

**Effect on other models.** Patched 5.3.0 and 6.0.0 command-line runs give the same results. Runoff continuity errors go to zero, surface runoff falls by up to 0.7 %, and system outfall peaks fall by 2-6 %:

| Deck (WET_STEP) | Runoff continuity error, % | Surface runoff | Outfall system peak flow | Routing continuity error, % |
|---|---|---|---|---|
| `examples/Example1.inp` (15 min) | -0.272 -> 0.000 | 1.064 -> 1.057 in | 19.59 -> 19.20 cfs | 0.106 -> 0.187 (6.0.0: 0.102 -> 0.160) |
| `user/user1.inp` (5 min) | -0.411 -> 0.000 | 19.126 -> 19.018 in | 13.122 -> 12.351 | 0.084 -> 0.116 |
| `user/user4.inp` (15 min) | -0.073 -> 0.000 | 20.068 -> 20.027 ac-ft | 172.15 -> 166.32 | 0.054 -> 0.055 |
| `examples/Example4.inp` (GA, evaporation) | -0.044 -> -0.014 | 0.585 -> 0.584 ac-ft | | 0.000 |
| `swc/swc1.inp` (GA, evaporation) | -0.515 -> -0.400 | 22.351 -> 22.195 in | | 0.000 |

What is left in `swc1` and `Example4` is the evaporation overdraw of [NUM-32](../../1-numerical/NUM-32-evaporation-and-infiltration-same-water/). Run with a 1-min wet step as a reference, `user1` gives an outfall peak of 13.011 (unpatched) and 12.817 (patched). At 5 min the unpatched engine is 0.9 % above its own 1-min peak and the patched one 3.6 % below. The fix moves volumes to the right answer and peaks away from it. A smaller wet step reduces both errors. Whether to accept the patch, or to keep the end-of-step rate and only document the volume error, is a decision for the maintainers.
