# BND-19: With a late REPORT_START, summary rows mix whole-run volumes with reporting-period statistics

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | In the Node Inflow and Outfall Loading summaries, "Total Inflow Volume" and "Total Volume" count from `START`, while every other column on the row counts from `REPORT_START`. In the test, J1's total inflow volume is 3.600 x 10^6 ltr against a lateral inflow volume of 0.905 although J1 has no other inflow, and the outfall's TSS load and volume give 2.60 mg/L for water that carries 10 mg/L. 5.2.4 and 5.3.0 also divide storage evaporation and exfiltration losses (reporting period) by the whole-run inflow. No warning. |
| **Reached from** | Any input file with `REPORT_START_DATE`/`REPORT_START_TIME` later than the start, and flow before the reporting period |
| **5.3.0** | `writeNodeFlows()`, `writeStorageVolumes()` and `writeOutfallLoads()` in [`src/legacy/engine/statsrpt.c:392`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L392), [`:558`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L558), [`:655`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L655) print `NodeInflow[]` ([`massbal.c:583`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L583)); the other columns come from `stats_updateFlowStats()`, which starts at `REPORT_START` ([`stats.c:446`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/stats.c#L446)) |
| **5.2.4** | Same code: [`statsrpt.c:389`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L389), [`:555`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L555), [`:652`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L652), [`stats.c:460`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/stats.c#L460) |
| **6.0.0** | Reproduces with the same numbers: `DefaultReportPlugin::write_results()` prints `ctx.nodes.stat_total_inflow_vol`, accumulated from `START` ([`DefaultReportPlugin.cpp:2492`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L2492), [`:2787`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L2787)). Its storage table prints 0.0 for both loss percentages, so that column is not affected. |
| **Since** | The outfall "Total Volume" is in the first commit of the EPA repository (5.0.021); the Node Inflow Summary's "Total Inflow Volume" came with 5.1.000 (commit fa32a734) |
| **Fix** | Remember each node's inflow volume when the reporting period starts and subtract it in those columns: [`BND-19_swmm530.patch`](BND-19_swmm530.patch), [`BND-19_swmm600.patch`](BND-19_swmm600.patch) |

## The problem

`REPORT_START` restricts the summary statistics to the reporting period: peaks, times of peaks, lateral inflow volume, outfall flow frequency and average flow, pollutant loads, flooding and surcharge times all start counting there. Three columns do not:

- Node Inflow Summary, **Total Inflow Volume**
- Outfall Loading Summary, **Total Volume** (and the System total)
- Storage Volume Summary, the denominators of **Pcnt Evap Loss** and **Pcnt Exfil Loss** (5.2.4 and 5.3.0)

They print the routing mass balance's per-node inflow volume, which starts at `START`, so each of these rows describes two different periods.

The test deck routes a 3-hour inflow hydrograph at J1 (rising to 0.5 m3/s, carrying a constant 10 mg/L of TSS) through two conduits to outfall O1, with `REPORT_START` 02:00 and `END` 06:00. All three engines print:

```
Node Inflow Summary, J1:     Lateral Inflow Volume   0.905   Total Inflow Volume   3.600  (10^6 ltr)
Outfall Loading Summary, O1: Flow Freq 33.01 %  Avg Flow 0.197 CMS  Total Volume 3.599  TSS 9.343 kg
```

J1 has no inflow other than the lateral one, yet its total inflow volume is four times the lateral volume. At O1, 33 % of the 4-hour reporting period at an average 0.197 m3/s is 0.936 x 10^6 ltr, not 3.599, and 9.343 kg of TSS in 3.599 x 10^6 ltr is 2.60 mg/L, a quarter of the concentration the water carries. A user reading the outfall row would conclude the outfall discharged dilute water, or that the model loses pollutant mass.

The continuity error column ("Flow Balance Error") and the continuity tables are meant to cover the whole run and are correct as they are.

## Why it happens

```c
// src/legacy/engine/stats.c, stats_updateFlowStats()
// --- update stats only after reporting period begins
if ( aDate < ReportStart ) return;
```

```c
// src/legacy/engine/massbal.c, massbal_updateRoutingTotals() - every routing step
NodeInflow[j] += Node[j].inflow * tStep;
```

```c
// src/legacy/engine/statsrpt.c, writeNodeFlows()
fprintf(Frpt.file, "%12.3g", NodeStats[j].totLatFlow * Vcf);   // from REPORT_START
fprintf(Frpt.file, "%12.3g", NodeInflow[j] * Vcf);             // from START

// writeOutfallLoads()
x = 100.*flowCount/(double)ReportStepCount;                     // from REPORT_START
...
fprintf(Frpt.file, "%12.3f", NodeInflow[j] * Vcf);             // from START
```

`NodeInflow[]` exists for the node continuity check, which needs the whole run (it also starts from each node's initial volume). The report reuses it as the "total inflow" of the reporting period. 6.0.0 copies the design: `stat_total_inflow_vol` is the counterpart of `NodeInflow[]`, accumulated in `accumulateNodeRoutingTotals()` from the first step, while `updateStatistics()` returns before `REPORT_START`.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-19_rpt-start-2h.inp`](BND-19_rpt-start-2h.inp) | J1 -> C1 -> J2 -> C2 -> O1 (CMS), inflow at J1 from 0:00 to 3:00 with 10 mg/L TSS, `REPORT_START` 02:00, `END` 06:00, fixed 5 s routing step |
| [`BND-19_test.c`](BND-19_test.c) | Runs the deck with the legacy toolkit, reads the J1 and O1 rows from the .rpt and checks three identities to 3 %: J1 total inflow volume = J1 lateral inflow volume; O1 volume = average flow x frequency x 4 h; O1 TSS load / volume = 10 mg/L |
| [`BND-19_test6.c`](BND-19_test6.c) | The same through the 6.0.0 C API |
| [`BND-19_swmm530.patch`](BND-19_swmm530.patch), [`BND-19_swmm600.patch`](BND-19_swmm600.patch) | The fixes |

```sh
tools/run-test.sh BND-19            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-19 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4, 5.3.0 and 6.0.0 print the same):

```
Node Inflow Summary, J1:     Lateral Inflow Volume   0.905   Total Inflow Volume   3.600  (10^6 ltr)
Outfall Loading Summary, O1: Flow Freq 33.01 %  Avg Flow 0.197 CMS  Total Volume 3.599  TSS 9.343 kg
  Avg Flow x Freq x 4 h  =   0.936 10^6 ltr   (Total Volume 3.599)
  TSS load / Total Volume =   2.60 mg/L     (inflow concentration 10 mg/L)
  J1: total inflow volume differs from its lateral inflow volume
  O1: total volume differs from average flow x frequency x period
  O1: load / volume differs from the 10 mg/L carried by the flow
FAIL: 3 of 3 summary identities broken (J1 total/lateral inflow volume 3.600/0.905, O1 volume 3.599 vs avg flow x freq x period 0.936, O1 TSS 2.60 mg/L)
BND-19 5.3.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Node Inflow Summary, J1:     Lateral Inflow Volume   0.905   Total Inflow Volume   0.904  (10^6 ltr)
Outfall Loading Summary, O1: Flow Freq 33.01 %  Avg Flow 0.197 CMS  Total Volume 0.934  TSS 9.343 kg
  Avg Flow x Freq x 4 h  =   0.936 10^6 ltr   (Total Volume 0.934)
  TSS load / Total Volume =  10.00 mg/L     (inflow concentration 10 mg/L)
PASS: volumes, flows and loads in the summary rows all cover the reporting period
BND-19 5.3.0 patched: PASS
BND-19 6.0.0 patched: PASS
```

## The fix

On the first statistics step of a late reporting period, store each node's `NodeInflow` in a new `TNodeStats` field and subtract it where the report prints the volume:

```diff
     if ( aDate < ReportStart ) return;
     SysOutfallFlow = 0.0;

+    // --- if reporting starts after the simulation does, note the inflow
+    //     volume each node received before it (NodeInflow covers the run)
+    if ( ReportStepCount == 0 && ReportStart > StartDateTime )
+    {
+        for ( j=0; j<Nobjects[NODE]; j++ )
+            NodeStats[j].inflowBeforeRpt = NodeInflow[j];
+    }
```

```diff
-        fprintf(Frpt.file, "%12.3g", NodeInflow[j] * Vcf);
+        fprintf(Frpt.file, "%12.3g",
+            (NodeInflow[j] - NodeStats[j].inflowBeforeRpt) * Vcf);
```

The same subtraction goes into the storage loss denominators and the outfall volume. The flow balance error column keeps the whole-run volumes. The 6.0.0 patch keeps the snapshot in `ctx.routing_stats.inflow_before_rpt` (taken in `updateStatistics()` at the same point of the step) and subtracts it in the Node Inflow and Outfall Loading tables.

When `REPORT_START` equals `START` the new field stays 0 and every report is byte-identical. The stored volume includes the first half of the first reporting step (the mass balance accumulates in half steps), so the period volume is short by half a routing step of flow; in the test that is 0.904 against a lateral volume of 0.905.

Effect on other models: Example1, Example2 and Example4 (no late `REPORT_START`) give byte-identical .rpt and .out files. `update_v52/drop_grate_inlet_17.inp` (`REPORT_START` 01:00) changes only the volume columns, e.g. node 1 total inflow 1.6 -> 1.33 (its lateral inflow volume is 1.33), outfall 7 from 1.068 to 0.903 x 10^6 gal, System 1.576 -> 1.333; `update_v52/street_curb_inlet_9a.inp` changes the same columns. The patched 5.3.0 and 6.0.0 print identical volumes for both decks. `update_v5111/bioretention.inp` has a late start but no links, and with no links SWMM collects no node statistics at all, so its report does not change.
