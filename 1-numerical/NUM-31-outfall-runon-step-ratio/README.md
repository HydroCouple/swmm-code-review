# NUM-31: Outfall flow routed onto a subcatchment is scaled by the ratio of two runoff steps

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | When the runoff step changes, a subcatchment that receives an outfall's flow gets that volume multiplied by new step / old step, while the runoff mass balance books the volume that left the outfall. In the test, 1.00 in of outfall flow arrives as 1.06 in (DRY_STEP 1 h, runoff continuity -1.98 %) or 1.25 in (DRY_STEP 4 h, -8.46 %). Water is created at a wet-to-dry switch and lost when the step shortens. Only the continuity error shows it. |
| **Reached from** | `[OUTFALLS]` with a `RouteTo` subcatchment, whenever the outfall discharges across a change of runoff step: rain stops (WET_STEP to DRY_STEP), the step is cut at the next rainfall or evaporation date, or the last step is clipped to the end of the run |
| **5.3.0** | `runoff_execute()` in [`src/legacy/engine/runoff.c:247`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L247) passes the previous step to `runoff_getOutfallRunon()` ([`runoff.c:519`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L519)) |
| **5.2.4** | Same code, [`src/solver/runoff.c:243`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/runoff.c#L243) and [`runoff.c:512`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/runoff.c#L512) |
| **6.0.0** | Reproduces with the same numbers: `SWMMEngine::assembleRunon()` divides by the previous step's span, [`src/engine/core/SWMMEngine.cpp:9673`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L9673), set at [`SWMMEngine.cpp:1867`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L1867) |
| **Since** | 5.1.011, whose change log says "Prior runoff time step used to convert returned outfall volume to flow". 5.1.008 to 5.1.010 used the current step, as the patch does. |
| **Fix** | Convert the volume with the step it is applied over: [`NUM-31_swmm530.patch`](NUM-31_swmm530.patch), [`NUM-31_swmm600.patch`](NUM-31_swmm600.patch) |

## The problem

An outfall can return its discharge to a subcatchment (`RouteTo` in `[OUTFALLS]`). During routing SWMM adds up the volume leaving the outfall, `Outfall.vRouted`. At the next runoff time it hands that volume to the subcatchment as run-on and books it as "Outfall Runon" in the runoff continuity table.

The volume is handed over as a rate, `vRouted / tStep`, where `tStep` is the runoff step that has just ended. The subcatchment then applies that rate over the runoff step that is about to start. If the two steps differ, the subcatchment receives `vRouted x newStep / oldStep`.

The typical case is the end of a storm. The subcatchments stop producing runoff, so the runoff step changes from WET_STEP to DRY_STEP, but a pond or a long pipe keeps the outfall discharging. In the test deck a roof drains through a storage unit to outfall O1, which is routed onto a pervious subcatchment S2 that infiltrates everything. At 1:00 the step changes from 5 min to 1 h, and the volume O1 discharged in the last 5-min step reaches S2 twelve times over. With a 4-hour dry step it arrives 48 times. Over the event:

| DRY_STEP | Outfall runon booked (in on S2) | Water S2 received (in) | Runoff continuity error |
|---|---|---|---|
| 1 h | 1.00 | 1.06 | -1.979 % |
| 4 h | 1.00 | 1.25 | -8.458 % |

The opposite happens when the step shortens: when the step is cut at the next rainfall date, or the last step of the run is clipped, part of the routed volume never reaches the subcatchment.

## Why it happens

```c
// src/legacy/engine/runoff.c, runoff_execute()
oldRunoffStep = (NewRunoffTime - OldRunoffTime) / 1000.0;   // step just ended
...
runoffStep = runoff_getTimeStep(currentDate);               // step about to start
...
if ( oldRunoffStep > 0.0 ) runoff_getOutfallRunon(oldRunoffStep);

// src/legacy/engine/runoff.c, runoff_getOutfallRunon(tStep)
subcatch_addRunonFlow(k, Outfall[i].vRouted/tStep);          // rate from the OLD step
massbal_updateRunoffTotals(RUNOFF_RUNON, Outfall[i].vRouted); // books the volume

// src/legacy/engine/subcatch.c, subcatch_getRunoff(j, tStep = runoffStep)
vRunon = Subcatch[subcatchIndex].runon * tStep * nonLidArea; // applied over the NEW step
```

The pollutant load returned with the water (`Subcatch[k].newQual[p] += w / tStep`) is converted the same way, so it is scaled by the same ratio.

5.1.011 made this change on purpose, so that the returned flow keeps the rate at which it left the outfall. The rate is preserved only by giving up the volume. The routing mass balance has already counted `vRouted` as leaving the outfall, and the runoff mass balance counts `vRouted` arriving, so any other volume reaching the subcatchment shows up as continuity error.

6.0.0 keeps the 5.1.011 behaviour: `prev_runoff_step_sec_` is the span of the step just completed, and `assembleRunon()` adds `vol / dt_prev`. It also calls `assembleRunon()` before the new runoff step has been chosen.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-31_dry-step-1h.inp`](NUM-31_dry-step-1h.inp) | Roof S1 (1 ac, n = 0, no depression storage) -> storage ST1 -> outlet OL1 -> outfall O1, routed onto pervious S2 (1 ac, infiltration 10 in/hr). 1 in/hr for 1 h; WET_STEP 5 min, DRY_STEP 1 h; no evaporation |
| [`NUM-31_dry-step-4h.inp`](NUM-31_dry-step-4h.inp) | The same with DRY_STEP 4 h |
| [`NUM-31_test.c`](NUM-31_test.c) | Runs both decks through the legacy toolkit (5.2.4 and 5.3.0); reports the outfall runon booked, the water S2 received (its infiltration minus its rain, since nothing ponds or runs off) and the runoff continuity error |
| [`NUM-31_test6.c`](NUM-31_test6.c) | The same with the 6.0.0 engine API |

Nothing in either deck ponds or produces a continuity error of its own, so the runoff continuity error must be zero.

```sh
tools/run-test.sh NUM-31            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-31 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** all three engines print:

```
DRY_STEP   outfall runon  received by S2  runoff continuity
           booked (in)    (in)            error (%)
1 h                  1.00            1.06           -1.979
4 h                  1.00            1.25           -8.458
FAIL: S2 does not receive the volume booked as outfall runon; runoff continuity error above 0.2 % in 2 of 2 runs
NUM-31 5.2.4 base: FAIL
NUM-31 5.3.0 base: FAIL
NUM-31 6.0.0 base: FAIL
```

**With the fix** 5.3.0 and 6.0.0 both print:

```
DRY_STEP   outfall runon  received by S2  runoff continuity
           booked (in)    (in)            error (%)
1 h                  1.00            1.00            0.000
4 h                  1.00            1.00            0.000
PASS: S2 receives the routed outfall volume; runoff continuity error within 0.2 % in both runs
NUM-31 5.3.0 patched: PASS
NUM-31 6.0.0 patched: PASS
```

## The fix

`runoff_execute()` has already chosen the new step, and clipped it to the end of the run, when it calls `runoff_getOutfallRunon()`, so 5.3.0 only needs to pass that step:

```diff
     // --- determine any runon from drainage system outfall nodes
-    if ( oldRunoffStep > 0.0 ) runoff_getOutfallRunon(oldRunoffStep);
+    //     (the volume routed since the last runoff time is applied
+    //     over the runoff step about to be taken)
+    if ( oldRunoffStep > 0.0 ) runoff_getOutfallRunon(runoffStep);
```

The first runoff step is still skipped (nothing has been routed yet). The returned pollutant load is converted with the same step, so it is conserved too.

In 6.0.0 the new step is not known where `assembleRunon()` was called, so the patch moves the call to just after the step is chosen and clipped, and passes the step in: `assembleRunon(dt_runoff)` divides the outfall volume (and its age and temperature) by `dt_runoff`. Nothing between the old and the new position reads or changes the run-on arrays. One side effect: with a `USE RUNOFF` interface file the call is now skipped, as legacy skips `runoff_getOutfallRunon()` in that mode.

**Effect on other models.** While the runoff step stays the same, nothing changes. The returned flow is applied at its true average rate over the step that follows instead of at the previous step's rate, so a long dry step spreads it more thinly. Of the regression decks, only `update_v5111/catchment_as_outfall.inp` routes an outfall onto a subcatchment (0.070 in of outfall runon). Its reports from the patched 5.3.0 and 6.0.0 builds are identical to the unpatched ones. So are those of `Example4`, `porous_pavement`, `rain_garden`, `CoS-Reduced-Inlets` and `swc12`, which check that moving the 6.0.0 call does not change subcatchment-to-subcatchment or LID run-on.
