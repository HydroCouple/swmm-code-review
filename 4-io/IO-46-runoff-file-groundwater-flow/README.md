# IO-46: Groundwater flow read from a runoff interface file is multiplied by the area again

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A `USE RUNOFF` run sends the drainage system groundwater inflow multiplied by the subcatchment area in ft² (and halved by a missing interpolation update). In the test a 10-acre aquifer that delivers 9.368 ac-ft over two days delivers 1,767,909 ac-ft, with a peak lateral inflow of 2.9 million cfs instead of 6.8 cfs. The routing continuity balances, so nothing flags it except the absurd volumes. 6.0.0 routes no groundwater at all from the file (0.000 ac-ft) |
| **Reached from** | `[FILES] USE RUNOFF` for a model with `[GROUNDWATER]` |
| **5.3.0** | `runoff_readFromFile()` in [`src/legacy/engine/runoff.c:468`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L468); saved as total flow in `subcatch_getResults()` at [`subcatch.c:913`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L913); used as a rate per unit area in `addGroundwaterInflows()` at [`routing.c:766`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L766) |
| **5.2.4** | Same code, [`src/solver/runoff.c:461`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/runoff.c#L461) and [`routing.c:651`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L651); same numbers |
| **6.0.0** | Reproduces differently. The reader stores the flow in `ctx.subcatches.gw_flow` ([`src/engine/hydrology/RunoffInterface.cpp:223`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/RunoffInterface.cpp#L223)), but node inflow is built from the groundwater solver's rate ([`src/engine/core/SWMMEngine.cpp:3158`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L3158)), which the file path never sets. The writer also saves each record before the groundwater step ([`SWMMEngine.cpp:2404`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L2404)), so the file holds the previous step's groundwater flow |
| **Since** | Every release in the repository (5.1.000 has the same read and interpolation code) |
| **Fix** | Divide by the area and shift the old value: [`IO-46_swmm530.patch`](IO-46_swmm530.patch); set the solver rate from the file and write the record after the groundwater step: [`IO-46_swmm600.patch`](IO-46_swmm600.patch) |

## The problem

A runoff interface file lets one run compute runoff and groundwater outflow and later runs route them without recomputing. A run that reads the file should send the drainage system the same inflows as the run that wrote it.

The test deck is a two-day groundwater recession with no rain: a 10-acre subcatchment whose aquifer starts with its water table at 8 ft and drains to junction J1. The run that computes the groundwater flow sends 9.368 ac-ft to J1, with a peak lateral inflow of 6.82 cfs. The run that reads the same flows from the file sends 1,767,909 ac-ft, peaking at 2,945,259 cfs. The flow routing continuity error of that run is -0.000 %, because the inflow is booked as groundwater inflow and leaves through the outfall. (The run's Groundwater Continuity table shows a 48.8 % error, but that table is printed for `USE RUNOFF` runs although they compute no groundwater fluxes; it is not a sign of this defect.)

6.0.0 sends nothing: Groundwater Inflow 0.000 ac-ft.

## Why it happens

`subcatch_getResults()`, which `runoff_saveToFile()` uses for each record, saves the groundwater outflow as a total flow in the user's flow units:

```c
// src/legacy/engine/subcatch.c, subcatch_getResults()
        z = (f1*gw->oldFlow + wt*gw->newFlow) * Subcatch[subcatchIndex].area * UCF(FLOW);
        x[SUBCATCH_GW_FLOW] = (float)z;
```

`runoff_readFromFile()` converts it to cfs and stores it in `gw->newFlow`:

```c
// src/legacy/engine/runoff.c, runoff_readFromFile()
            gw->newFlow    = SubcatchResults[SUBCATCH_GW_FLOW] / UCF(FLOW);
```

Everywhere else `gw->newFlow` is a flow per unit area in ft/s, and `addGroundwaterInflows()` multiplies it by the area in ft²:

```c
// src/legacy/engine/routing.c, addGroundwaterInflows()
                q = ( (1.0 - f)*(gw->oldFlow) + f*(gw->newFlow) )
                    * Subcatch[i].area;
```

For 10 acres that is a factor of 435,600. The same function interpolates from `gw->oldFlow`, which in a computing run `gwater_getGroundwater()` sets to the last `newFlow` before each step. In a `USE RUNOFF` run nothing sets it, so it stays 0 and the interpolation roughly halves the flow: 9.368 ac-ft × 435,600 / 2 ≈ 2.0 million ac-ft, against the 1.77 million observed (the recession is not linear within a 6-hour dry-weather runoff step).

In 6.0.0 the record is read into `ctx.subcatches.gw_flow`, in cfs, but `stepRunoff()` builds node groundwater inflow from `groundwater_.state().gw_flow`, a rate per unit area, and its previous value `old_gw_rate_`. The `USE RUNOFF` branch skips the groundwater solver, so that rate stays 0. The 6.0.0 writer has a second problem: it saves each record at step A4b', before washoff, the LID fold and the groundwater step, so the groundwater flow in record *n* is the one computed in step *n − 1*. The first record holds 0.0000 cfs where 5.3.0's holds 6.8180.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-46_save.inp`](IO-46_save.inp) | 10-acre subcatchment, no rain, aquifer water table at 8 ft draining to J1, 2 days; `SAVE RUNOFF IO-46.rof` |
| [`IO-46_use.inp`](IO-46_use.inp) | The same model with `USE RUNOFF IO-46.rof` |
| [`IO-46_test.c`](IO-46_test.c) | Runs both; compares J1's largest lateral inflow and the routing continuity's Groundwater Inflow (5.2.4 and 5.3.0) |
| [`IO-46_test6.c`](IO-46_test6.c) | The same with 6.0.0 |

```sh
tools/run-test.sh IO-46            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-46 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0:

```
                                     J1 max lateral     Groundwater Inflow
                                       inflow (cfs)                (ac-ft)
SAVE RUNOFF run (computes GW)                6.8180                  9.368
USE RUNOFF run (reads the file)        2945259.1191            1767908.675
FAIL: the USE RUNOFF run routes 1767908.675 ac-ft of groundwater inflow (peak 2945259.1191 cfs) instead of 9.368 ac-ft (peak 6.8180 cfs)
IO-46 5.3.0 base: FAIL
```

6.0.0:

```
SAVE RUNOFF run (computes GW)                6.8180                  9.368
USE RUNOFF run (reads the file)              0.0000                  0.000
FAIL: the USE RUNOFF run routes 0.000 ac-ft of groundwater inflow (peak 0.0000 cfs) instead of 9.368 ac-ft (peak 6.8180 cfs)
IO-46 6.0.0 base: FAIL
```

**With the fix**, both engines:

```
SAVE RUNOFF run (computes GW)                6.8180                  9.368
USE RUNOFF run (reads the file)              6.8180                  9.368
PASS: the USE RUNOFF run routes the groundwater inflow of the run that wrote the file
IO-46 5.3.0 patched: PASS
IO-46 6.0.0 patched: PASS
```

With the reader fix alone, 6.0.0 routed 10.394 ac-ft from its own file, because of the one-step lag in what it wrote; moving the write removes it.

## The fix

5.3.0: shift the old rate and convert the file's total flow back to a rate per unit area, as the run that wrote the file had it:

```diff
-            gw->newFlow    = SubcatchResults[SUBCATCH_GW_FLOW] / UCF(FLOW);
+            // --- file holds total flow; gw->newFlow is flow per unit area
+            gw->oldFlow    = gw->newFlow;
+            gw->newFlow    = 0.0;
+            if ( Subcatch[j].area > 0.0 )
+                gw->newFlow = SubcatchResults[SUBCATCH_GW_FLOW] / UCF(FLOW) /
+                              Subcatch[j].area;
```

(A subcatchment may have zero area, hence the guard.)

6.0.0: after reading a record, set `groundwater_.state().gw_flow` to the file's flow divided by the area (the old rate is already rolled at the top of the runoff loop), and move the `saveRunoffIfaceStep()` call from before washoff to after the groundwater step, where legacy `runoff_saveToFile()` runs.

**Effect on other models.** Runs without a runoff interface file are not affected in either engine. The 5.3.0 change only affects `USE RUNOFF` runs of models with groundwater. Moving the 6.0.0 write changes the content of `SAVE RUNOFF` files: comparing the files 6.0.0 and 5.3.0 write for the same deck, Example1 (two pollutants) had 630 differing washoff concentrations before the move and none after, Example4 (LID) had 1,672 differing runoff values before and 582 after (6.0.0's LID handling is otherwise known to differ), and the test deck's 9 groundwater flows now all match. The groundwater elevation and soil moisture fields, which 6.0.0 writes as 0, are IO-47.
