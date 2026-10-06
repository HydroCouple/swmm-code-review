# IO-48: The routing interface file holds the outfall state at the end of a routing step, stamped with an earlier report time

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | When a report time falls inside a routing step, the `SAVE OUTFLOWS` interface file writes the outfall flow and concentrations from the end of that step under the report time, so the saved hydrograph runs early by up to one routing step. In the test (40-s steps, 1-minute reports, rising inflow) the file gives 0.835 cfs at 00:05 where the run's `.out` file gives 0.562 cfs (+49 %), and 3.377 cfs at 00:07 instead of 2.932. A downstream model that reads the file with `USE INFLOWS` receives the shifted hydrograph. Nothing indicates the difference. |
| **Reached from** | `[FILES] SAVE OUTFLOWS` whenever routing steps do not end on every report time: DYNWAVE variable steps, a fixed step that does not divide `REPORT_STEP`, or a step clipped by a control rule step |
| **5.3.0** | `iface_saveOutletResults()` in [`src/legacy/engine/iface.c:333`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/iface.c#L333), called from `output_saveResults()`, [`output.c:524`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L524) |
| **5.2.4** | Same code: [`src/solver/iface.c:303`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/iface.c#L303), [`output.c:509`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/output.c#L509) |
| **6.0.0** | Reproduces: `InterfaceManager::writeOutfallResults()` in [`InterfaceFile.cpp:386`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/InterfaceFile.cpp#L386), called from [`SWMMEngine.cpp:6118`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L6118) |
| **Since** | Every 5.x release |
| **Fix** | Interpolate the interface values to the report time as the `.out` file does: [`IO-48_swmm530.patch`](IO-48_swmm530.patch), [`IO-48_swmm600.patch`](IO-48_swmm600.patch) |

## The problem

SWMM reports results at fixed report times, but routes with steps that need not end on them. For the `.out` file it interpolates each node and link result between the routing states before and after the report time. The routing interface file, written in the same call, does not: each row carries the outfall's inflow and concentrations at the end of the routing step that passed the report time, stamped with the report time.

The test deck routes a flow ramp (0 to 30 cfs) and a TSS concentration ramp (0 to 300 mg/L), both over 30 minutes, through two KINWAVE conduits with 40-s routing steps and 1-minute reports. Routing steps end at 0:40, 1:20, 2:00, 2:40, ... so every other report time lies inside a step. At those times the interface file holds the state up to 20 s later than its time stamp:

| Time | Interface file | `.out` file (same run) |
|---|---|---|
| 00:05 | 0.8353 cfs, 18.144 mg/L | 0.5621 cfs, 16.219 mg/L |
| 00:06 | 1.6056 cfs | 1.6056 cfs |
| 00:07 | 3.3769 cfs, 31.245 mg/L | 2.9323 cfs, 28.928 mg/L |

13 of the 30 rows differ, by up to 0.44 cfs.

## Why it happens

```c
// src/legacy/engine/output.c, output_saveNodeResults()
// --- find where current reporting time lies between latest routing times
double f = (reportTime - OldRoutingTime) /
           (NewRoutingTime - OldRoutingTime);
...
node_getResults(j, f, NodeResults);       // (1-f)*oldFlowInflow + f*inflow, ...

// src/legacy/engine/output.c, output_saveResults()
if ( Foutflows.mode == SAVE_FILE && !IgnoreRouting )
    iface_saveOutletResults(reportDate, Foutflows.file);

// src/legacy/engine/iface.c, iface_saveOutletResults()
fprintf(file, " %-10f", Node[i].inflow * UCF(FLOW));        // value at NewRoutingTime
for ( p = 0; p < Nobjects[POLLUT]; p++ )
    fprintf(file, " %-10f", Node[i].newQual[p]);
```

6.0.0 does the same: its `.out` snapshot interpolates with `f_rt`, while `writeOutfallResults()` writes `ctx.nodes.inflow` and `ctx.nodes.conc`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-48_ramp.inp`](IO-48_ramp.inp) | J1 - C1 - J2 - C2 - O1, KINWAVE, 40-s routing step, 1-minute reports, flow and TSS ramps at J1, `SAVE OUTFLOWS IO-48_iface.txt` |
| [`IO-48_test.c`](IO-48_test.c) | Legacy toolkit (5.2.4 and 5.3.0): runs the deck, then compares every O1 row of the interface file with O1's total inflow and TSS for the same time in the run's `.out` file |
| [`IO-48_test6.c`](IO-48_test6.c) | The same for 6.0.0 |

The test allows 0.001 cfs and 0.01 mg/L (the `.out` file stores 4-byte floats and the interface file prints six decimals); the missing interpolation shifts values by more than 0.1 cfs.

```sh
tools/run-test.sh IO-48            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-48 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0 print the same flows):

```
Time   Interface file      Results file (.out)
       Flow (cfs)  TSS     Flow (cfs)  TSS (mg/L)
00:04      0.0396   10.959      0.0396      10.959
00:05      0.8353   18.144      0.5621      16.219  <-- differs
00:06      1.6056   22.258      1.6056      22.258
00:07      3.3769   31.245      2.9323      28.928  <-- differs
00:08      4.2302   36.188      4.2302      36.188
FAIL: 13 of 30 interface-file rows do not hold the outfall state at their time stamp (largest flow difference +0.4446 cfs)

---- 6.0.0 ----
00:05      0.8506   18.144      0.5732      16.219  <-- differs
00:07      3.3834   31.245      2.9415      28.911  <-- differs
FAIL: 13 of 30 interface-file rows do not hold the outfall state at their time stamp (largest flow difference +0.4419 cfs)
```

**With the fix**:

```
---- 5.3.0 ----
00:05      0.5621   16.219      0.5621      16.219
00:07      2.9323   28.928      2.9323      28.928
PASS: all 30 interface-file rows match the outfall results at their time stamp

---- 6.0.0 ----
00:05      0.5732   16.219      0.5732      16.219
00:07      2.9415   28.911      2.9415      28.911
PASS: all 30 interface-file rows match the outfall results at their time stamp
```

The two engines' flows differ by up to 2 % before and after the patch (0.5621 and 0.5732 cfs at 00:05 in their `.out` files); that difference is in their KINWAVE results, not in the interface file. In 5.3.0 the TSS columns of both files read `-nan` at 00:01 and 00:02, while the conduits are still empty; that is a separate 5.3.0 defect in the link quality update and is not changed here.

## The fix

5.3.0: pass the interpolation weight that `output_saveNodeResults()` uses to `iface_saveOutletResults()`, and write the weighted values:

```diff
-        fprintf(file, " %-10f", Node[i].inflow * UCF(FLOW));
+        fprintf(file, " %-10f",
+            ((1.0 - f) * Node[i].oldFlowInflow + f * Node[i].inflow) * UCF(FLOW));
         for ( p = 0; p < Nobjects[POLLUT]; p++ )
         {
-            fprintf(file, " %-10f", Node[i].newQual[p]);
+            fprintf(file, " %-10f",
+                (1.0 - f) * Node[i].oldQual[p] + f * Node[i].newQual[p]);
         }
```

```diff
     if ( Foutflows.mode == SAVE_FILE && !IgnoreRouting ) 
-        iface_saveOutletResults(reportDate, Foutflows.file);
+        iface_saveOutletResults(reportDate,
+            (reportTime - OldRoutingTime) / (NewRoutingTime - OldRoutingTime),
+            Foutflows.file);
```

The record written when the file is opened (report start at the simulation start) passes f = 1, which is the previous behaviour. The prototype in `funcs.h` changes accordingly. 6.0.0 computes the same weight inside `writeOutfallResults()` from `elapsed_ms`, `old_elapsed_ms` and `next_report_ms` and interpolates `old_inflow`/`inflow` and `conc_old`/`conc`.

**Effect on other models.** Only runs that save an outflows interface file change, and only in rows whose report time falls inside a routing step; when the routing step divides the report step every row has f = 1 and the file is unchanged. With `REPORT AVERAGES YES` the interface file still gets point values, now interpolated. No regression deck saves or uses an interface file.
