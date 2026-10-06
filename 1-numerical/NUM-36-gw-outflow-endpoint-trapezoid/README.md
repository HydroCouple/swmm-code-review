# NUM-36: Groundwater flow sent to the node is not the volume the aquifer released

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | The groundwater volume delivered to the drainage system differs from the volume the aquifer loses. A 10-acre aquifer drains 10.980 ac-ft in 4 days whatever the time step, but the node receives 10.904 ac-ft with a 15-min DRY_STEP (-0.7 %), 10.680 (1 h, -2.7 %), 10.799 (6 h, -1.6 %) and 12.481 ac-ft (1 day, +13.7 %). Over the first 1-day step the node receives 22 % more than the aquifer released. The groundwater continuity error shows it (up to -3.264 %); the flow routing continuity does not. |
| **Reached from** | Any `[GROUNDWATER]` aquifer that discharges laterally: the first runoff step of every run, and every step of a curved recession. Grows with the runoff time step; Vol I sec. 3.5 says the dry step "is typically several hours or even days" and is used "to generate groundwater flow" |
| **5.3.0** | `gwater_getGroundwater()` integrates the aquifer over the step but keeps only the end-of-step rate, [`src/legacy/engine/gwater.c:595`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L595); the flow starts from 0, [`gwater.c:423`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L423); routing interpolates the two end rates, [`routing.c:766`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L766), and `updateMassBal()` books their trapezoid, [`gwater.c:634`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L634) |
| **5.2.4** | Same code, [`src/solver/gwater.c:585`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gwater.c#L585), [`gwater.c:624`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gwater.c#L624), [`routing.c:651`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L651) |
| **6.0.0** | Reproduces with identical numbers: [`src/engine/hydrology/Groundwater.cpp:388`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Groundwater.cpp#L388), [`:409`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Groundwater.cpp#L409), [`src/engine/core/SWMMEngine.cpp:3162`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L3162) |
| **Since** | 5.0 (the same code is in 5.0.022, the oldest source in the history) |
| **Fix** | Integrate the outflow volume in the aquifer ODE and send its step average: [`NUM-36_swmm530.patch`](NUM-36_swmm530.patch), [`NUM-36_swmm600.patch`](NUM-36_swmm600.patch). Apply after [IO-46](../../4-io/IO-46-runoff-file-groundwater-flow/) (and, for 5.3.0, [IO-47](../../4-io/IO-47-runoff-file-groundwater-elevation-sign/)), as the `Requires:` lines say: with the flow delivered as a step average, the runoff interface file has to hold it constant over the step too, which needs IO-46's reader. |

## The problem

The test deck is a 10-acre aquifer with the water table 8 ft above its bottom, draining to junction J1 (invert 2 ft) with A1 = 0.05 and B1 = 1.5. There is no rain, no evaporation and no deep percolation, so the lateral groundwater flow is the only way water leaves the aquifer, and the volume J1 receives must equal the drop in aquifer storage.

The aquifer state is integrated with the adaptive Runge-Kutta solver, so the storage drop is the same, 10.980 ac-ft, at every time step. The volume sent to J1 is not:

| DRY_STEP | Storage lost (ac-ft) | GW inflow to J1 (ac-ft) | Difference | GW continuity error |
|---|---|---|---|---|
| 0:15 | 10.980 | 10.904 | -0.69 % | 0.165 % |
| 1:00 | 10.980 | 10.680 | -2.73 % | 0.652 % |
| 6:00 | 10.980 | 10.799 | -1.65 % | 0.391 % |
| 24:00 | 10.980 | 12.481 | +13.67 % | -3.264 % |

Two errors add up here. Every run starts with a GW flow of 0, so the first runoff step delivers a ramp from 0. With DRY_STEP 1:00 or longer that step is 1 h in this deck (the rain gage's recording interval cuts it short); the ramp goes from 0 to 6.8 cfs and about half of that hour's volume, 0.30 ac-ft, never reaches the node. After that the node receives straight lines between end-of-step rates. For a recession that flattens out, a straight line between the two ends lies above the curve. With a 1-day step, J1 receives a line from 6.82 cfs at 1:00 down to 1.69 cfs at 25:00, an average of 4.25 cfs (8.44 ac-ft), while the aquifer released an average of 3.49 cfs (6.91 ac-ft) over that day.

## Why it happens

```c
// src/legacy/engine/gwater.c, gwater_getGroundwater()
    // --- integrate eqns. for d(Theta)/dt and d(LowerDepth)/dt
    odesolve_integrate(x, 2, 0, tStep, GWTOL, tStep, getDxDt);
    ...
    GW->theta = x[THETA];
    GW->lowerDepth  = x[LOWERDEPTH];
    getFluxes(GW->theta, GW->lowerDepth);
    GW->oldFlow = GW->newFlow;
    GW->newFlow = GWFlow;            // rate at the end of the step only
```

Inside `odesolve_integrate()` the lateral flow is evaluated continuously as the water table falls, and the state loses exactly its integral. Afterwards only the end-of-step rate is kept. Routing interpolates the GW inflow between the previous and the new end-of-step rates, and the GW mass balance books the same trapezoid:

```c
// src/legacy/engine/routing.c, addGroundwaterInflows()
                q = ( (1.0 - f)*(gw->oldFlow) + f*(gw->newFlow) )
                    * Subcatch[i].area;

// src/legacy/engine/gwater.c, updateMassBal()
    vGwater    = 0.5 * (GW->oldFlow + GW->newFlow) * ft2sec;
```

`gwater_initState()` sets `oldFlow = newFlow = 0`, so the first interpolation starts from zero even when the aquifer starts above the outlet. The GW summary table uses a third value, the end-of-step rate times the step.

6.0.0 copies the scheme: `GWSolver::execute()` keeps `c.gw_flow` from the final `getFluxes()` call, the engine interpolates between the old and new rates, and the mass balance books `0.5 * (old_flow + gw_flow)`.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-36_recession.inp`](NUM-36_recession.inp) | 4-day groundwater recession, no rain, DRY_STEP 24:00; the trailing `*` in `[GROUNDWATER]` works around IO-15 |
| [`NUM-36_test.c`](NUM-36_test.c) | Writes copies of the deck with DRY_STEP 0:15, 1:00, 6:00 and 24:00, runs each through the legacy toolkit, and compares the routing table's Groundwater Inflow with Initial minus Final Storage from the groundwater table. Tolerance 0.25 % (the report's rounding is 0.005 %) |
| [`NUM-36_test6.c`](NUM-36_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-36            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-36 --patched  # 5.3.0, 6.0.0 with the fix: PASS
```

**Without the fix**, all three engines print:

```
DRY_STEP   Aquifer storage   GW inflow to J1   Difference   GW continuity
             lost (ac-ft)          (ac-ft)          (%)       error (%)
00:15:00            10.980            10.904        -0.69           0.165
01:00:00            10.980            10.680        -2.73           0.652
06:00:00            10.980            10.799        -1.65           0.391
24:00:00            10.980            12.481        13.67          -3.264
FAIL: GW inflow to J1 differs from the aquifer's storage loss by up to +13.67 % (DRY_STEP 24:00:00)
NUM-36 5.2.4 base: FAIL
NUM-36 5.3.0 base: FAIL
NUM-36 6.0.0 base: FAIL
```

**With the fix**, both engines:

```
DRY_STEP   Aquifer storage   GW inflow to J1   Difference   GW continuity
             lost (ac-ft)          (ac-ft)          (%)       error (%)
00:15:00            10.980            10.980         0.00           0.000
01:00:00            10.980            10.980         0.00           0.000
06:00:00            10.980            10.980         0.00           0.000
24:00:00            10.980            10.979        -0.01           0.000
PASS: GW inflow to J1 equals the aquifer's storage loss within 0.25 % for every DRY_STEP (worst -0.009 %)
NUM-36 5.3.0 patched: PASS
NUM-36 6.0.0 patched: PASS
```

## The fix

Let the ODE solver integrate the outflow volume along with the state, and send the step's average rate:

```diff
-    double x[2];                       // upper moisture content & lower depth 
+    double x[3];                       // upper moisture content, lower depth
+                                       // & TotalDepth - GW outflow volume
 ...
+    x[2] = TotalDepth;
 ...
-    odesolve_integrate(x, 2, 0, tStep, GWTOL, tStep, getDxDt);
+    odesolve_integrate(x, 3, 0, tStep, GWTOL, tStep, getDxDt);
 ...
-    GW->oldFlow = GW->newFlow;
-    GW->newFlow = GWFlow;
+    GW->oldFlow = (TotalDepth - x[2]) / tStep;
+    GW->newFlow = GW->oldFlow;
 ...
-    stats_updateGwaterStats(j, infil, GW->evapLoss, GWFlow, LowerLoss,
+    stats_updateGwaterStats(j, infil, GW->evapLoss, GW->newFlow, LowerLoss,
 ...
 // getDxDt()
+    dxdt[2] = -GWFlow;
```

The third variable counts down from the total aquifer depth instead of up from 0. The solver's error test is relative to each variable's size, so this holds the volume to the same accuracy as the zone depths and in practice leaves the solver's step sequence as before (the Example5 report below is unchanged). A variable starting at 0 is held to a relative accuracy that cannot be met where GW flow starts from 0. In a first attempt the solver gave up there (10,000 sub-steps), left the state unchanged and lost the step's infiltration from the balance. Routing now receives a constant rate over each runoff step whose volume is what left the aquifer, so the first step no longer starts from 0. The GW mass balance and the GW summary table book the same volume. 6.0.0 makes the same change in `GWSolver::execute()` and sets the engine's old interpolation rate to the new one in `stepGroundwater()`. Both patched engines print the same numbers.

What changes for users: the subcatchment GW flow in the output file is now the average over each runoff step rather than the rate at its end, and the node receives steps instead of ramps. With short steps the difference is small; with long dry steps the node sees a staircase that carries the correct volume.

Effect on other models (patched 5.3.0 and 6.0.0 against the unpatched builds):

- `examples/Example5.inp` (the only regression deck with groundwater, 5-min steps): every line of the report is identical. In the output file the GW flow series changes by at most 0.0021 cfs (peak 0.283 cfs), because it now shows step averages.
- The R4 deck `t7a_continuous.inp` (2-day recession, DRY_STEP 6 h): Groundwater Flow rises from 9.368 to 9.553 ac-ft, which equals the storage drop (46.000 - 36.447), and the GW continuity error falls from 0.403 % to 0.000 %.
- `t3a_imperv_ponded.inp` (aquifer below the outlet) and `NUM-35_monthly-pattern.inp`: reports identical.

This patch also removes the first-step ramp that BND-11 describes after a hot start, since the restored rate is no longer used for interpolation. BND-11 has its own one-line fix.
