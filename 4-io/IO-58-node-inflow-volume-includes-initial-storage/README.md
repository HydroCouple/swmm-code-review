# IO-58: "Total Inflow Volume" of a node includes the water it held at the start

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | In the Node Inflow Summary, every node that starts with water in it (a storage unit with an initial depth, mostly) shows its initial stored volume as part of its Total Inflow Volume. In the test, a storage unit that receives 0.0539 Mgal reports 0.0913 Mgal. A storage unit that starts full and receives nothing shows a non-zero total inflow next to a maximum total inflow of 0.00. The flow balance column is not affected, and nothing in the report says the column includes storage. |
| **Reached from** | Any input file with an initial depth at a node that has storage volume (`[STORAGE]` InitDepth), or a hot start file that leaves water in such nodes |
| **5.3.0** | `massbal_open()` seeds `NodeInflow[j] = Node[j].newVolume` in [`src/legacy/engine/massbal.c:239`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L239), and `writeNodeFlows()` prints it as Total Inflow Volume in [`statsrpt.c:392`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L392) |
| **5.2.4** | Same code, [`src/solver/massbal.c:239`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/massbal.c#L239) and [`statsrpt.c:389`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L389) |
| **6.0.0** | Reproduces: `SWMMEngine::start()` seeds `stat_total_inflow_vol` with the node volume ([`SWMMEngine.cpp:1484`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L1484)) and `DefaultReportPlugin` prints it ([`DefaultReportPlugin.cpp:2492`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L2492)) |
| **Since** | 5.0.014, when the Total Inflow Volume column was added (`//(5.0.014 - LR)` in 5.0.022's `statsrpt.c`) |
| **Fix** | Subtract the node's initial volume when printing the column: [`IO-58_swmm530.patch`](IO-58_swmm530.patch), [`IO-58_swmm600.patch`](IO-58_swmm600.patch) Apply after BND-19 (its `Requires:` line). |

## The problem

The manual defines the Node Inflow Summary columns (Chapter 4, "Node Inflow"): "Total inflow volume", with the note "Total inflow consists of lateral inflow plus inflow from connecting links."

The test deck has one storage unit, SU1, with a constant area of 1000 ft² and an initial depth of 5 ft, so it starts with 5000 ft³ (0.0374 Mgal). It receives a lateral inflow of 1 cfs for the 2-hour run and has no inflowing links, so its total inflow is its lateral inflow, 7200 ft³ (0.0539 Mgal). 5.2.4, 5.3.0 and 6.0.0 report:

```
  SU1                  STORAGE       1.00     1.00     0  00:00      0.0539      0.0913      -0.001
```

The Total Inflow Volume, 0.0913 Mgal, is the lateral inflow plus the initial stored volume. EPA's `Storage_Shape_Test.inp` regression deck shows the extreme case: four storage units that start partly full, receive nothing (Maximum Total Inflow 0.00, Lateral Inflow Volume 0) and still report Total Inflow Volumes of 0.0274 and 0.0526 Mgal.

## Why it happens

The per-node inflow accumulator does two jobs. For the per-node continuity check it must hold everything that entered the node's balance, so it starts at the initial volume, just as the outflow accumulator ends with the final volume:

```c
// src/legacy/engine/massbal.c, massbal_open()
for (j = 0; j < Nobjects[NODE]; j++) NodeInflow[j] = Node[j].newVolume;

// src/legacy/engine/stats.c, stats_findMaxStats()
//     (Note: NodeInflow & NodeOutflow include any initial and final
//            stored volumes, respectively).
```

The report then prints the same accumulator as the inflow volume:

```c
// src/legacy/engine/statsrpt.c, writeNodeFlows()
fprintf(Frpt.file, "%12.3g", NodeStats[j].totLatFlow * Vcf);   // Lateral Inflow Volume
fprintf(Frpt.file, "%12.3g", NodeInflow[j] * Vcf);              // Total Inflow Volume
```

6.0.0 copies both steps.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-58_storage-initial-volume.inp`](IO-58_storage-initial-volume.inp) | SU1 (1000 ft², 5 ft initial depth), 1 cfs lateral inflow for 2 hours, no inflowing links, drained by an orifice |
| [`IO-58_test.c`](IO-58_test.c) | 5.2.4/5.3.0: runs the deck and compares SU1's Total and Lateral Inflow Volumes in the Node Inflow Summary |
| [`IO-58_test6.c`](IO-58_test6.c) | The same for 6.0.0 |

The test passes when the two volumes agree within 2 % (both are printed to 3 significant digits and accumulated slightly differently; the initial volume adds 69 %).

```sh
tools/run-test.sh IO-58            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-58 --patched  # 5.3.0, 6.0.0: PASS
```

**Without the fix** (all three print the same):

```
SU1 (no inflowing links)       10^6 gal
  lateral inflow volume        0.0539
  total inflow volume          0.0913
  initial stored volume        0.0374  (5000 ft3)
  total - lateral              0.0374
FAIL: SU1 has no inflowing links, but its Total Inflow Volume (0.0913) exceeds its Lateral Inflow Volume (0.0539) by 0.0374, its initial stored volume
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Node Inflow Summary row:
  SU1                  STORAGE       1.00     1.00     0  00:00      0.0539      0.0539      -0.001
...
  total - lateral              0.0000
PASS: SU1's Total Inflow Volume equals its Lateral Inflow Volume
```

## The fix

Keep each node's initial volume and subtract it from the printed column only:

```diff
 // objects.h, TNodeStats
    double        totLatFlow;
+   double        initVolume;
 // stats.c, stats_open()  (runs right after massbal_open())
         NodeStats[j].totLatFlow = 0.0;
+        NodeStats[j].initVolume = Node[j].newVolume;
 // statsrpt.c, writeNodeFlows()
-        fprintf(Frpt.file, "%12.3g", NodeInflow[j] * Vcf);
+        fprintf(Frpt.file, "%12.3g",
+            (NodeInflow[j] - NodeStats[j].initVolume) * Vcf);
```

The 6.0.0 patch records `ctx.nodes.volume` in `DefaultReportPlugin::prepare()`, which runs after the initial and hot-start state is set and before `start()` seeds `stat_total_inflow_vol`, and subtracts it in the same column.

`NodeInflow` itself is unchanged, so the Flow Balance Error column, the "Highest Continuity Errors" ranking and the storage units' % evaporation and % exfiltration losses do not change. Their denominator is meant to include the initial volume: 5.2.1 divided by `NodeInflow[j] + StorageStats[k].initVol`, which counted it twice, and 5.2.2 "corrected" that to `NodeInflow[j]`. Outfalls hold no volume, so the Outfall Loading Summary, which also prints `NodeInflow`, is unaffected.

**Effect on other models.** Run with base and patched builds, `extran10.inp` and `user2.inp` (storage units with initial depths) give identical reports: their initial volumes are below the third significant digit of the column. In `Storage_Shape_Test.inp` the four storage units that start partly full and receive no inflow change from 0.0274/0.0526 to 0 Mgal; nothing else changes, in either engine.
