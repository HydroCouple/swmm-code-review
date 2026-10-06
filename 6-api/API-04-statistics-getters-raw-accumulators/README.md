# API-04: The statistics getters return raw accumulators: sums for averages, seconds for hours, volumes for depths, feet in SI projects

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | 5.3.0's `swmm_get*Stats()` and `swmm_getSystem*Totals()` return the internal running sums instead of the statistics their header documents. In the test, J1's "average depth" reads 577 ft (it is 0.76 ft), its "time flooded (hours)" 2,564 (0.71 h), the outfall's "average flow" 1,677 cfs (2.42 cfs), its TSS load 2,184 (0.136 lbs), the subcatchment's "precipitation (depth)" 13,613 (0.375 in); the flow-class "times" sum to 1 when the report was written and to the run length in seconds when it was not. An SI project gets feet, cfs and cubic feet. 6.0.0's `swmm_stat_*` getters have the unit part of the problem: hours come back as seconds (2,564 for 0.71 h), and an SI project gets 3.28 for a 1 m depth and 13,243 for 375 m³ of rain. Nothing warns. |
| **Reached from** | 5.3.0: `swmm_getSubcatchStats`, `swmm_getNodeStats`, `swmm_getStorageStats`, `swmm_getOutfallStats`, `swmm_getLinkStats`, `swmm_getPumpStats`, `swmm_getSystemRoutingTotals`, `swmm_getSystemRunoffTotals`. 6.0.0: the 23 `swmm_stat_*` getters of `openswmm_statistics.h`, `swmm_node_get_stat_time_flooded`, `swmm_link_get_stat_surcharge_time` |
| **5.3.0** | [`src/legacy/engine/swmm5_stats.c`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5_stats.c#L50) copies the accumulators (e.g. [`:72`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5_stats.c#L72) `avgDepth`, [`:136`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5_stats.c#L136) `avgFlow`, [`:242`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5_stats.c#L242) `rainfall`); the documented meanings are in [`include/openswmm/legacy/engine/openswmm_solver.h:677-823`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/include/openswmm/legacy/engine/openswmm_solver.h#L677-L823); `writeFlowClass()` divides the flow-class times in place, [`statsrpt.c:813`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L813) |
| **5.2.4** | Not affected: there are no statistics getters |
| **6.0.0** | Reproduces for units: [`src/engine/core/openswmm_statistics_impl.cpp`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_statistics_impl.cpp#L42) copies the internal columns, while [`openswmm_statistics.h`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/include/openswmm/engine/openswmm_statistics.h#L45-L55) documents project units and hours. Averages are not offered, so the sum-for-average part does not apply |
| **Since** | 5.3.0: `swmm5_stats.c` was added in fork commit 03ed283a (March 2026). 6.0.0: `openswmm_statistics_impl.cpp` in 4e29c886 (March 2026) |
| **Fix** | Convert in the getters as the report does: [`API-04_swmm530.patch`](API-04_swmm530.patch), [`API-04_swmm600.patch`](API-04_swmm600.patch) |

## The problem

5.3.0 added toolkit functions that return a structure of statistics per object (the numbers in the report's summary tables) and the system mass-balance totals. The header describes each field: "Average node depth", "Total time flooded (hours)", "Average flow rate", "Fraction of time utilized", "Total precipitation (depth)", "all in ft3 or m3", and so on. The functions return something else: the running sums that `stats.c` and `massbal.c` keep while the model runs, which the report turns into statistics only when it prints them.

| Field | Header says | Returned before the fix |
|---|---|---|
| node `avgDepth`, storage `avgVol` | average | sum over all routing steps |
| outfall `avgFlow`, pump `avgFlow` | average flow | sum of the flows of the steps with flow |
| node and link `time*` fields | hours | seconds |
| link `timeInFlowClass[]` | time | fraction if the report was written (dynamic wave, report enabled), seconds otherwise |
| pump `utilized` | fraction of time | seconds |
| subcatchment `precip`, runoff-total depths | depth | volume (ft³) |
| outfall `totalLoad[]` | load | flow x concentration x time (cfs x mg/L x s) |
| every length, flow and volume | project units | ft, cfs, ft³, also in SI projects |

The structures look like the report's numbers, so a program reading them gets values off by factors from 3 to several thousand without any sign of error.

6.0.0's statistics API (`swmm_stat_node_max_depth()` etc.) returns single values rather than averages, but copies the internal columns in the same way. The header states "project length units", "project flow units", "project volume units" and "hours"; the values are ft, cfs, ft³ and seconds. Two per-object getters, `swmm_node_get_stat_time_flooded()` and `swmm_link_get_stat_surcharge_time()`, convert nothing either and return seconds where their documentation says hours.

## Why it happens

```c
// src/legacy/engine/swmm5_stats.c, swmm_getNodeStats()
    stats->avgDepth            = NodeStats[index].avgDepth;
    stats->maxDepth            = NodeStats[index].maxDepth;
    ...
    stats->timeFlooded         = NodeStats[index].timeFlooded;
```

```c
// src/legacy/engine/stats.c, stats_updateNodeStats(), every routing step
    NodeStats[j].avgDepth += newDepth;
    ...
            NodeStats[j].timeFlooded += tStep;
```

```c
// src/legacy/engine/statsrpt.c, writeNodeDepths(): the report finishes the statistic
            NodeStats[j].avgDepth / ReportStepCount * UCF(LENGTH),
```

and in `writeFlowClass()` the report changes the accumulator itself:

```c
            fprintf(Frpt.file, "  %4.2f",
                LinkStats[j].timeInFlowClass[i] /= totalSeconds);
```

`swmm_end()` writes the statistics report, so after a normal run with dynamic wave routing the flow-class "times" are fractions; with `[REPORT] DISABLED YES` or another routing method they are seconds.

6.0.0:

```cpp
// src/engine/core/openswmm_statistics_impl.cpp
SWMM_ENGINE_API int swmm_stat_node_max_depth(SWMM_Engine engine, int idx, double* val) {
    ...
    if (val) *val = ctx.nodes.stat_max_depth[static_cast<std::size_t>(idx)];
```

while the per-object getter for the same column applies `to_display(ctx, openswmm::ucf::LENGTH, ...)`. The time columns accumulate `dt_routing` in seconds.

## How to reproduce

| File | What it is |
|---|---|
| [`API-04_us.inp`](API-04_us.inp) | US units, dynamic wave: S1 (0.375 in of rain) to J1, an extra inflow that floods J1 for 0.71 h, C1 and C2 to outfall O1; TSS washoff |
| [`API-04_si.inp`](API-04_si.inp) | The same layout in SI units (9.375 mm of rain on 4 ha) |
| [`API-04_test.c`](API-04_test.c) | Legacy API: polls depths, overflows, outfall inflow and concentration after every routing step, then compares 11 fields of the statistics structures with their definitions computed from the polled values (mean depth, flooded hours, sum of flow-class times = routed hours, mean outfall flow, load in lbs, rain depth, outflow volume in m³). 5.3.0 only (`#ifdef`); 5.2.4 prints PASS because the getters do not exist |
| [`API-04_test6.c`](API-04_test6.c) | The same idea for 9 values of the 6.0.0 statistics getters |
| [`API-04_swmm530.patch`](API-04_swmm530.patch), [`API-04_swmm600.patch`](API-04_swmm600.patch) | The fixes |

The test accepts an average taken per routing step, which is how 5.3.0's report averages, or per unit of time, which is what [CON-21](../../2-conceptual/CON-21-statistics-averaged-over-steps/)'s patch changes it to: either is an average; the raw sum is not.

```sh
tools/run-test.sh API-04            # 5.2.4 PASS, 5.3.0 FAIL, 6.0.0 FAIL
tools/run-test.sh API-04 --patched  # 5.3.0 PASS, 6.0.0 PASS
```

**Without the fix**, every field checked in 5.3.0 is wrong:

```
API-04_us.inp (US units), 757 routing steps
  statistic                                  getter       expected
  J1 avgDepth                               577.026       0.762253  ft     WRONG
  J1 timeFlooded                            2563.61       0.712115  hours  WRONG
  C1 sum of timeInFlowClass                       1              6  hours  WRONG
  O1 avgFlow                                1676.98        2.42338  cfs    WRONG
  O1 totalLoad TSS                          2184.17       0.136354  lbs    WRONG
  S1 precip                                   13613          0.375  in     WRONG
  RunoffTotals rainfall                       13613          0.375  in     WRONG
API-04_si.inp (SI units), 761 routing steps
  statistic                                  getter       expected
  J1 maxDepth                               3.28084              1  m      WRONG
  J1 volFlooded                              8296.8        234.965  m3     WRONG
  S1 precip                                   13243          9.375  mm     WRONG
  RoutingTotals outflow                     42097.7        1192.13  m3     WRONG
FAIL: 11 of 11 statistics differ from their definition (sums for averages, seconds for hours, volumes for depths, ft/cfs/ft3 in an SI project)
API-04 5.3.0 base: FAIL
```

The report of the same US run shows the expected values: J1 average depth 0.76 ft, O1 average flow 2.42 cfs and TSS load 0.136 lbs, S1 precipitation 0.37 in, J1 flooded 0.71 hours.

6.0.0:

```
API-04_us.inp (US units)
  statistic                                        getter     expected
  swmm_stat_node_time_flooded J1                  2563.61     0.712115  hours WRONG
  swmm_node_get_stat_time_flooded J1              2563.61     0.712115  hours WRONG
  swmm_stat_link_surcharge_time C1                2578.22     0 to 6    hours WRONG
  swmm_link_get_stat_surcharge_time C1            2578.22     0 to 6    hours WRONG
API-04_si.inp (SI units)
  statistic                                        getter     expected
  swmm_stat_node_max_depth J1                     3.28084            1  m     WRONG
  swmm_stat_node_max_depth_bulk J1                3.28084            1  m     WRONG
  swmm_stat_node_vol_flooded J1                    8296.8      234.965  m3    WRONG
  swmm_stat_link_vol_flow C2                      42095.1      1192.13  m3    WRONG
  swmm_stat_subcatch_precip S1                      13243          375  m3    WRONG
FAIL: 9 of 9 statistics are not in the documented units (seconds for hours, ft/ft3 in an SI project)
API-04 6.0.0 base: FAIL
```

**With the fix**:

```
API-04_us.inp (US units), 757 routing steps
  J1 avgDepth                              0.762253       0.762253  ft     ok
  J1 timeFlooded                           0.712115       0.712115  hours  ok
  C1 sum of timeInFlowClass                       6              6  hours  ok
  O1 avgFlow                                2.42338        2.42338  cfs    ok
  O1 totalLoad TSS                         0.136254       0.136354  lbs    ok
  S1 precip                                   0.375          0.375  in     ok
  RunoffTotals rainfall                       0.375          0.375  in     ok
API-04_si.inp (SI units), 761 routing steps
  J1 maxDepth                                     1              1  m      ok
  J1 volFlooded                             234.965        234.965  m3     ok
  S1 precip                                   9.375          9.375  mm     ok
  RoutingTotals outflow                     1192.21        1192.13  m3     ok
PASS: every statistic checked matches its definition, in the project's units
API-04 5.3.0 patched: PASS
```

```
  swmm_stat_node_time_flooded J1                 0.712115     0.712115  hours ok
  swmm_node_get_stat_time_flooded J1             0.712115     0.712115  hours ok
  swmm_stat_link_surcharge_time C1               0.716173     0 to 6    hours ok
  swmm_link_get_stat_surcharge_time C1           0.716173     0 to 6    hours ok
  swmm_stat_node_max_depth J1                           1            1  m     ok
  swmm_stat_node_max_depth_bulk J1                      1            1  m     ok
  swmm_stat_node_vol_flooded J1                   234.965      234.965  m3    ok
  swmm_stat_link_vol_flow C2                      1192.13      1192.13  m3    ok
  swmm_stat_subcatch_precip S1                    375.042          375  m3    ok
PASS: every statistic checked is in its documented unit and matches its definition
API-04 6.0.0 patched: PASS
```

The TSS load is 0.07 % below the test's value because SWMM converts milligrams to pounds with 2.203e-6 (`Ucf[MASS]`), not 1/453,592.37; the report has the same factor.

## The fix

**5.3.0.** Each field is converted as the report converts it (`statsrpt.c` and, for the totals, `massbal.c`), and returned in the project's units:

| Field | Conversion |
|---|---|
| node `avgDepth`, storage `avgVol` | ÷ `ReportStepCount`, x `UCF(LENGTH)` / `UCF(VOLUME)` |
| outfall and pump `avgFlow` | ÷ `totalPeriods` (steps with flow, as in the report), x `UCF(FLOW)` |
| all `time*` fields, pump `offCurveLow/High` | ÷ 3600 (hours) |
| pump `utilized` | ÷ routed time (`RoutingTimeSpan`), a fraction |
| subcatchment `precip` | ÷ subcatchment area, x `UCF(RAINDEPTH)` |
| runoff totals documented as depths | ÷ total subcatchment area (`TotalArea`), x `UCF(RAINDEPTH)` |
| outfall `totalLoad[]` | x `LperFT3` x `Pollut[p].mcf` (lbs or kg) |
| other lengths, flows, volumes | x `UCF(LENGTH)`, `UCF(FLOW)`, `UCF(VOLUME)` |
| dates, counts, `energy`, `maxRptDepth`, `pctError` | unchanged (already final) |

For example:

```diff
-    stats->avgDepth            = NodeStats[index].avgDepth;
-    stats->maxDepth            = NodeStats[index].maxDepth;
+    stats->avgDepth            = 0.0;
+    if (ReportStepCount > 0)
+        stats->avgDepth        = NodeStats[index].avgDepth / ReportStepCount *
+                                 UCF(LENGTH);
+    stats->maxDepth            = NodeStats[index].maxDepth * UCF(LENGTH);
```

`writeFlowClass()` prints `timeInFlowClass[i] / totalSeconds` instead of dividing the accumulator in place, so the getter returns the same hours whether or not the report was written; the printed numbers are unchanged. Three header comments described values the engine does not keep and now describe what is returned: `timeCourantCritical` is a number of routing steps (stats.c counts steps, so hours cannot be recovered), and the pump `offCurveLow/High` fields are times off the ends of the curve, not depths. The pump `minFlow` is still always 0; that is a defect of the accumulator in `stats.c`, which the report shares.

**6.0.0.** The 12 scalar and 11 bulk `swmm_stat_*` getters apply `to_display()` with the documented quantity (length, flow, volume) and divide the two time columns by 3600; `max_filling` is a ratio and unchanged. The bulk getters copy element by element instead of with `std::copy`. `swmm_node_get_stat_time_flooded()` and `swmm_link_get_stat_surcharge_time()` divide by 3600. The Python wrapper's docstring for the bulk flooded-time getter, which says seconds, is not part of the patch.

Model results and report files do not change: only values returned through these getters do. Programs that already rescaled the raw values themselves have to drop their own conversion.
