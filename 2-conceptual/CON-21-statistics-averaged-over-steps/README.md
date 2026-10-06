# CON-21: The summary averages are means over routing steps, not over time

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | With a variable routing step, Average Depth (Node Depth Summary), Avg Volume and Avg Pcnt Full (Storage Volume Summary), Flow Freq and Avg Flow (Outfall Loading Summary) and Avg Flow (Pumping Summary, 5.2.4 and 5.3.0) give peak conditions too much weight, because the step is shortest at the peak. In the test a 40-cfs pulse on a 0.2-cfs base flow makes them 22 % to 43 % high. The outfall's Avg Flow × Flow Freq × duration comes to 1.026 Mgal against the Total Volume of 0.803 Mgal printed on the same row. In the regression deck Example7-Final, average node depths are up to 71 % high (J6: 0.24 ft instead of 0.14) and the outfall's Avg Flow is 43.15 cfs instead of 29.50. No warning. |
| **Reached from** | Any run whose routing step varies: dynamic wave with `VARIABLE_STEP` > 0, and the shortened last step of any run. With a constant step the averages are right |
| **5.3.0** | `stats_updateNodeStats()` adds one sample per step at [`src/legacy/engine/stats.c:541`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/stats.c#L541) (depth), [`:576`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/stats.c#L576) (storage volume) and [`:597-599`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/stats.c#L597-L599) (outfall flow), and `stats_updateLinkStats()` at [`:674`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/stats.c#L674) (pump flow). `statsrpt.c` divides by step counts at [`:346`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L346), [`:547`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L547), [`:638-646`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L638-L646) and [`:898-900`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L898-L900) |
| **5.2.4** | Same code, [`src/solver/stats.c:558`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/stats.c#L558), [`:593`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/stats.c#L593), [`:614`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/stats.c#L614), [`:691`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/stats.c#L691) and [`statsrpt.c:343`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L343), [`:544`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L544), [`:635-639`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L635-L639), [`:897`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L897) |
| **6.0.0** | Reproduces for node depth, storage volume and outfall flow: [`src/engine/core/SWMMEngine.cpp:5219`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L5219), [`:5227`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L5227), [`:5284`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L5284), divided by step counts in [`DefaultReportPlugin.cpp:2450`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L2450), [`:2693`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L2693) and [`:2784-2786`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L2784-L2786). The depth sum is documented as "length units × seconds" ([`NodeData.hpp:650`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/data/NodeData.hpp#L650)). The pump average is already volume / running time |
| **Since** | Every release in the repository history. 5.2.0 made the Flow Classification fractions time-weighted (`++timeInFlowClass[k]` became `+= tStep`) but left these averages |
| **Fix** | Accumulate value × step length and divide by time: [`CON-21_swmm530.patch`](CON-21_swmm530.patch) (after [API-04](../../6-api/API-04-statistics-getters-raw-accumulators/), [BND-19](../../3-boundary/BND-19-report-start-mixes-whole-run-volumes/) and [BND-20](../../3-boundary/BND-20-summary-nan-without-routing-step/)), [`CON-21_swmm600.patch`](CON-21_swmm600.patch) (after BND-19 and BND-20) |

## The problem

The status report's summary tables give averages: the average depth at each node, the average volume and percent full of each storage unit, and for each outfall the percent of time it discharges and its average flow while it does. The Pumping Summary gives each pump's average flow while it runs. The report chapter of the engine manual describes them as averages over the run, for example "Percent of time that outfall discharges" and "Average volume of water in the facility". The code computes the plain mean of one value per routing step.

With a constant routing step the two are the same. With a variable step they are not, and the error is systematic. Dynamic wave's Courant condition shortens the step when velocities are high, so a storm peak is sampled many more times per hour than the dry weather around it, and every average is pulled toward the peak.

The test model routes a 0.2-cfs base flow with a 30-minute 40-cfs pulse through a junction, a storage unit and a pipe to an outfall, with `VARIABLE_STEP 0.75`. The same inflow goes through an ideal pump to a second outfall. The step falls from 30 s to as little as 4 s while the pulse passes:

| | Reported | Time average | Error |
|---|---|---|---|
| J1 Average Depth (ft) | 0.77 | 0.594 | +30 % |
| SU1 Avg Volume (1000 ft³) | 18.034 | 14.742 | +22 % |
| O1 Flow Freq (%) | 92.47 | 91.52 | +1 % |
| O1 Avg Flow (cfs) | 6.87 | 5.43 | +27 % |
| P1 Avg Flow (cfs) | 7.37 | 5.17 | +43 % |

The outfall row contradicts itself. The 6-hour run at 6.87 cfs for 92.47 % of the time comes to 1.026 Mgal, while the same row's Total Volume is 0.803 Mgal. The pump's 7.37 cfs over 6 hours would be 1.191 Mgal, but it pumped 0.835 Mgal.

## Why it happens

```c
// src/legacy/engine/stats.c, stats_updateNodeStats(), once per routing step
    NodeStats[j].avgDepth += newDepth;
    ...
        StorageStats[k].avgVol += newVolume;
    ...
        if ( Node[j].inflow >= MIN_RUNOFF_FLOW )
        {
            OutfallStats[k].avgFlow += Node[j].inflow;
            OutfallStats[k].maxFlow = MAX(OutfallStats[k].maxFlow, Node[j].inflow);
            OutfallStats[k].totalPeriods++;
        }

// stats_updateLinkStats(), for a running pump
            PumpStats[k].avgFlow += q;
```

```c
// src/legacy/engine/statsrpt.c
            NodeStats[j].avgDepth / ReportStepCount * UCF(LENGTH),        // writeNodeDepths()
            avgVol = StorageStats[k].avgVol / (double)ReportStepCount;      // writeStorageVolumes()
            x = 100.*flowCount/(double)ReportStepCount;                     // writeOutfallLoads()
                x = OutfallStats[k].avgFlow*UCF(FLOW)/flowCount;
            avgFlow /=  PumpStats[k].totalPeriods;                           // writePumpFlows()
```

`ReportStepCount`, `flowCount` (= `totalPeriods`) and `PumpStats.totalPeriods` count routing steps. The same functions already keep the time: `stats_updateFlowStats()` adds every step to `RoutingTimeSpan`, the Flow Classification Summary divides `tStep`-weighted sums by it (since 5.2.0), and a running pump adds `q*tStep` to `PumpStats.volume` and `tStep` to `PumpStats.utilized`. Only the averages above use the counts. 6.0.0 has the same accumulation and division for nodes, storage units and outfalls. Its pump average is `stat_pump_volume / on_time`, and the 6.0.0 test shows it correct.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-21_variable-step.inp`](CON-21_variable-step.inp) | J1 → C1 → storage SU1 → C2 → outfall O1, and wet well W1 → ideal pump P1 → outfall O2. Both inflows are 0.2 cfs with a 40-cfs pulse from 0:45 to 1:15. Dynamic wave, `ROUTING_STEP 30`, `VARIABLE_STEP 0.75`, 6 hours |
| [`CON-21_test.c`](CON-21_test.c) | Steps the run through the legacy toolkit and forms time averages of the end-of-step values that `stats.c` samples, each weighted by its step length: J1 depth, SU1 volume, the outfall inflow (C2's flow) and the fraction of time it is at least 0.001 cfs, and P1's flow while above 0.001 cfs. Then it compares them with the report, allowing the column's rounding plus 0.5 % |
| [`CON-21_test6.c`](CON-21_test6.c) | The same check against 6.0.0 |

```sh
tools/run-test.sh CON-21            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-21 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 (identical output):

```
                                 report  time avg.  report error
J1 Average Depth (ft)             0.770      0.594    +29.6 %   <--
SU1 Avg Volume (1000 ft3)        18.034     14.742    +22.3 %   <--
O1 Flow Freq (%)                 92.470     91.524     +1.0 %   <--
O1 Avg Flow (cfs)                 6.870      5.429    +26.5 %   <--
P1 Avg Flow (cfs)                 7.370      5.170    +42.6 %   <--
FAIL: the summary averages are not time averages (marked rows)
```

6.0.0:

```
                                 report  time avg.  report error
J1 Average Depth (ft)             0.770      0.594    +29.6 %   <--
SU1 Avg Volume (1000 ft3)        18.034     14.740    +22.4 %   <--
O1 Flow Freq (%)                 92.470     91.525     +1.0 %   <--
O1 Avg Flow (cfs)                 6.870      5.428    +26.6 %   <--
P1 Avg Flow (cfs)                 5.170      5.169     +0.0 %
FAIL: the summary averages are not time averages (marked rows)
```

**With the fix**, 5.3.0 (6.0.0 prints the same report values):

```
                                 report  time avg.  report error
J1 Average Depth (ft)             0.590      0.594     -0.7 %
SU1 Avg Volume (1000 ft3)        14.740     14.742     -0.0 %
O1 Flow Freq (%)                 91.530     91.524     +0.0 %
O1 Avg Flow (cfs)                 5.430      5.429     +0.0 %
P1 Avg Flow (cfs)                 5.170      5.170     +0.0 %
PASS: the summary averages are time averages over the run
```

The remaining −0.7 % on J1 is the two-decimal rounding of 0.594 to 0.59. With the fix the outfall row is consistent: 5.43 cfs × 91.53 % × 6 h = 0.803 Mgal, its Total Volume.

## The fix

Multiply each sample by its step length and divide by time. For node depths and storage volumes the divisor is the reporting period, `RoutingTimeSpan`. For an outfall it is the time the outfall discharged, kept in a new `OutfallStats.flowTime`, which is also the numerator of Flow Freq. A pump's average flow is its pumped volume over its running time, both of which `stats.c` already keeps.

```diff
-    NodeStats[j].avgDepth += newDepth;
+    NodeStats[j].avgDepth += newDepth * tStep;
 ...
-            OutfallStats[k].avgFlow += Node[j].inflow;
+            OutfallStats[k].avgFlow += Node[j].inflow * tStep;
             OutfallStats[k].maxFlow = MAX(OutfallStats[k].maxFlow, Node[j].inflow);
             OutfallStats[k].totalPeriods++;
+            OutfallStats[k].flowTime += tStep;
```

```diff
-            NodeStats[j].avgDepth / MAX(ReportStepCount, 1) * UCF(LENGTH),
+            NodeStats[j].avgDepth / (RoutingTimeSpan > 0.0 ? RoutingTimeSpan : 1.0) * UCF(LENGTH),
 ...
-            flowCount = OutfallStats[k].totalPeriods;
+            flowCount = OutfallStats[k].flowTime;
 ...
-        avgFlow = PumpStats[k].avgFlow;
-        if ( PumpStats[k].totalPeriods > 0 )
-            avgFlow /=  PumpStats[k].totalPeriods;
+        avgFlow = PumpStats[k].volume;
+        if ( PumpStats[k].utilized > 0.0 )
+            avgFlow /=  PumpStats[k].utilized;
```

The 5.3.0 statistics getters (`swmm_getNodeStats()`, `swmm_getStorageStats()`, `swmm_getOutfallStats()`, `swmm_getPumpStats()`) return these averages since API-04, so the patch divides them the same way there. The 6.0.0 patch makes the same changes in `updateStatistics()` and `DefaultReportPlugin`, with a new `NodeData::stat_outfall_time`. The patches are written on top of API-04, BND-19 and BND-20, which change the same lines (the getters, the guards against a zero step count, and the outfall volume column).

**Effect on other models.** Only the averaged columns change. The `.out` files, continuity errors and all other report values are identical, apart from what the prerequisite BND-19 changes in decks with a later `REPORT_START`. Of the 73 regression decks, 7 change at the printed precision: CoS-Reduced-Inlets, CoS-Reduced-Outlets, Example3, Example7-Final, Example7-Inlets, Storage_Shape_Test and gate_control_2. The rest have a constant routing step, or steps that vary too little to show at two or three decimals. The largest changes are in the dynamic-wave decks with `VARIABLE_STEP 0.75`:

| Deck | Value | Before | After |
|---|---|---|---|
| Example7-Final | J6 Average Depth (ft) | 0.24 | 0.14 |
| Example7-Final | J1 Average Depth (ft) | 1.65 | 1.11 |
| Example7-Final | O1 Avg Flow (cfs) | 43.15 | 29.50 |
| Example3 | PUMP1 Avg Flow (cfs) | 0.51 | 0.50 |

In Example7-Final, 16 node depths drop and Avg Flow × Flow Freq × duration at O1 goes from 465,507 ft³ (+46 % against the row's Total Volume of 2.379 Mgal) to 318,154 ft³ (+0.03 %). Patched 5.3.0 and patched 6.0.0 print identical values in every changed column of the seven decks.

## Notes

- `[REPORT] AVERAGES YES` has the same weighting: `output_updateAvgResults()` ([`output.c:853`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L853)) adds one sample per routing step, and `output_saveAvgResults()` divides by the number of steps (`Nsteps`), so each period's average in the `.out` file is a step mean. The patches here do not change it. A time-weighted version needs the step length passed to `output_updateAvgResults()` and a step that straddles a report time split between two periods. [BND-18](../../3-boundary/BND-18-averages-not-reset-at-report-start/) and [CRASH-11](../../5-crashes/CRASH-11-averages-report-out-of-bounds/) also change these functions.
- `PumpStats.avgFlow` is still accumulated but no longer read.
