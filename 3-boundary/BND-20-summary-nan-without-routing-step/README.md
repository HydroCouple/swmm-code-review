# BND-20: Summary tables print -nan when no routing step falls in the reporting period

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | 27 to 29 `-nan` values in the 5.2.4 and 5.3.0 report of the test model (Node Depth average depth, Storage average volume and percent full, Outfall flow frequency, every Flow Classification fraction, Pump utilization, Groundwater moisture and water table), 18 in 6.0.0 (Flow Classification). Tools that parse the .rpt fail on them. No warning. |
| **Reached from** | (A) `[EVENTS]` with the whole reporting period (`REPORT_START` to `END`) outside every event; (B) a toolkit client that calls `swmm_start()` and then `swmm_end()`/`swmm_report()` without a `swmm_step()`, e.g. a cancelled run |
| **5.3.0** | Divisions by `ReportStepCount`, `RoutingTimeSpan` and the runoff time in [`src/legacy/engine/statsrpt.c`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c): `writeGroundwater()` [L197](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L197), `writeNodeDepths()` [L346](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L346), `writeStorageVolumes()` [L547](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L547), `writeOutfallLoads()` [L642](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L642), `writeFlowClass()` [L813](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L813), `writePumpFlows()` [L897](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L897); also `stats_report()` [`stats.c:801`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/stats.c#L801) (0/0, not printed) |
| **5.2.4** | Same code, [`statsrpt.c:194`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L194), [`:343`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L343), [`:544`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L544), [`:639`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L639), [`:810`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L810), [`:894`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L894), [`stats.c:818`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/stats.c#L818) |
| **6.0.0** | Partly fixed: node and storage averages divide by `max(report_steps, 1)`, but the Flow Classification table still divides by `routing_stats.report_time` ([`DefaultReportPlugin.cpp:2956`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L2956)) |
| **Since** | Every release; the Flow Classification table has divided by the routing time (instead of the step count) since 5.2.0 |
| **Fix** | Divide by at least one step or one second, so statistics over no steps print 0: [`BND-20_swmm530.patch`](BND-20_swmm530.patch), [`BND-20_swmm600.patch`](BND-20_swmm600.patch) |

## The problem

SWMM collects the node and link statistics only for routing steps that end after `REPORT_START` and lie inside an `[EVENTS]` period. When no such step exists, the step count and the routing time are both 0, every accumulated statistic is 0, and the report prints 0/0:

```
  J1                   JUNCTION     -nan     0.00     5.00     0  00:00        0.12
  SU1                       -nan   -nan    0.0    0.0       0.000    0.0       0  00:00       0.00
  O1                     -nan      0.00      0.00       0.048
  C1                      1.00   -nan  -nan  -nan  -nan  -nan  -nan  -nan  -nan  -nan
  P1                       -nan           0      0.00      0.00      0.00     0.000      0.00    0.0    0.0
```

Two ways to get there:

- **A. Events.** `[EVENTS]` restrict routing to wet periods. If `REPORT_START` falls after the last event (or between two events and `END` falls before the next one), no step is counted. The review found it with the regression deck `events_example` and a later `REPORT_START_DATE`.
- **B. A run ended before its first step.** A toolkit client that opens and starts a run, then ends it without stepping (a cancelled run, a coupling driver that stops early) and asks for the report. Here the Groundwater Summary also divides by the runoff time, which is 0.

## Why it happens

```c
// src/legacy/engine/stats.c, stats_updateFlowStats() - called only when !BetweenEvents
if ( aDate < ReportStart ) return;
...
ReportStepCount++;
RoutingTimeSpan += tStep;
```

```c
// src/legacy/engine/statsrpt.c
NodeStats[j].avgDepth / ReportStepCount * UCF(LENGTH),            // writeNodeDepths()
avgVol = StorageStats[k].avgVol / (double)ReportStepCount;        // writeStorageVolumes()
x = 100.*flowCount/(double)ReportStepCount;                       // writeOutfallLoads()
LinkStats[j].timeInFlowClass[i] /= totalSeconds);                 // writeFlowClass(), = RoutingTimeSpan
pctUtilized = PumpStats[k].utilized / totalSeconds * 100.0;       // writePumpFlows(), = RoutingTimeSpan
double totalSeconds = NewRunoffTime / 1000.;                      // writeGroundwater()
```

None of these checks for zero. 6.0.0 added `std::max(report_steps, 1L)` to the node, storage and outfall divisions but not to the flow-class division:

```cpp
// src/engine/plugins/DefaultReportPlugin.cpp
const double total_secs = ctx.routing_stats.report_time;
...
std::fprintf(f, "  %4.2f", t / total_secs);
```

`stats_report()` also divides each node's non-convergence count by the total step count. With no step that is 0/0, but the NaN never wins the "highest" comparison, so nothing is printed.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-20_events.inp`](BND-20_events.inp) | Subcatchment with groundwater -> J1 -> C1 -> storage SU1 -> pump P1 -> J2 -> C2 -> outfall O1 (DYNWAVE); one event 00:00-02:00, `REPORT_START` 03:00, `END` 04:00 (scenario A) |
| [`BND-20_no-step.inp`](BND-20_no-step.inp) | The same model without `[EVENTS]` and with `REPORT_START` = `START` (scenario B) |
| [`BND-20_test.c`](BND-20_test.c) | A: full run of the events deck; B: `swmm_open`, `swmm_start`, `swmm_end`, `swmm_report`, `swmm_close` on the other deck. Counts report tokens that parse as NaN or infinity |
| [`BND-20_test6.c`](BND-20_test6.c) | The same through the 6.0.0 C API |
| [`BND-20_swmm530.patch`](BND-20_swmm530.patch), [`BND-20_swmm600.patch`](BND-20_swmm600.patch) | The fixes |

```sh
tools/run-test.sh BND-20            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-20 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 (identical):

```
A. [EVENTS] end before REPORT_START, full run:
      J1                   JUNCTION     -nan     0.00     5.00     0  00:00        0.12
      J2                   JUNCTION     -nan     0.00     2.00     0  00:00        0.14
      O1                   OUTFALL      -nan     0.00     1.00     0  00:00        0.13
      SU1                  STORAGE      -nan     0.00     3.00     0  00:00        1.01
      SU1                       -nan   -nan    0.0    0.0       0.000    0.0       0  00:00       0.00
      O1                     -nan      0.00      0.00       0.048
   error 0, 27 NaN/inf values in the report
B. swmm_start() then swmm_end() with no swmm_step():
      S1                       0.00     0.00     0.00     0.00     0.00     -nan     -nan     0.00     0.00
      J1                   JUNCTION     -nan     0.00     5.00     0  00:00        0.00
      J2                   JUNCTION     -nan     0.00     2.00     0  00:00        0.00
      O1                   OUTFALL      -nan     0.00     1.00     0  00:00        0.00
      SU1                  STORAGE      -nan     0.00     3.00     0  00:00        0.00
      SU1                       -nan   -nan    0.0    0.0       0.000    0.0       0  00:00       0.00
   error 0, 29 NaN/inf values in the report
FAIL: the status report contains 56 NaN/inf values with no routing step in the reporting period (A: 27, B: 29; errors 0, 0)
BND-20 5.3.0 base: FAIL
```

(The test prints the first six affected lines of each report; the others are the outfall System row, both Flow Classification rows and the pump row.)

6.0.0:

```
A. [EVENTS] end before REPORT_START, full run:
      C1                      1.00   -nan  -nan  -nan  -nan  -nan  -nan  -nan  -nan  -nan
      C2                      1.00   -nan  -nan  -nan  -nan  -nan  -nan  -nan  -nan  -nan
   error 0, 18 NaN/inf values in the report
B. swmm_engine_start() then swmm_engine_end() with no swmm_engine_step():
      C1                      1.00   -nan  -nan  -nan  -nan  -nan  -nan  -nan  -nan  -nan
      C2                      1.00   -nan  -nan  -nan  -nan  -nan  -nan  -nan  -nan  -nan
   error 0, 18 NaN/inf values in the report
FAIL: the status report contains 36 NaN/inf values with no routing step in the reporting period (A: 18, B: 18; errors 0, 0)
BND-20 6.0.0 base: FAIL
```

**With the fix**, both engines:

```
A. [EVENTS] end before REPORT_START, full run:
   error 0, 0 NaN/inf values in the report
B. swmm_start() then swmm_end() with no swmm_step():
   error 0, 0 NaN/inf values in the report
PASS: no NaN or infinite values in either report
BND-20 5.3.0 patched: PASS
BND-20 6.0.0 patched: PASS
```

and the rows read, identically in 5.3.0 and 6.0.0:

```
  J1                   JUNCTION     0.00     0.00     5.00     0  00:00        0.12
  SU1                      0.000    0.0    0.0    0.0       0.000    0.0       0  00:00       0.00
  O1                     0.00      0.00      0.00       0.048
  C1                      1.00   0.00  0.00  0.00  0.00  0.00  0.00  0.00  0.00  0.00
```

## The fix

Every statistic divided here is a sum over the counted steps, so it is 0 when the count is 0. Dividing by at least one step (or one second) prints that 0 and leaves every other result bit-identical:

```diff
-            NodeStats[j].avgDepth / ReportStepCount * UCF(LENGTH),
+            NodeStats[j].avgDepth / MAX(ReportStepCount, 1) * UCF(LENGTH),
```

```diff
     double totalSeconds = RoutingTimeSpan;

     if ( RouteModel != DW ) return;
+    if ( totalSeconds <= 0.0 ) totalSeconds = 1.0;   // no step in reporting period
```

The 5.3.0 patch applies this to the six places listed above and skips the non-convergence frequency in `stats_report()` when no routing step was taken. The 6.0.0 patch does the same for `total_secs` in the Flow Classification table, matching the `max(report_steps, 1)` guards 6.0.0 already has.

Effect on other models: none unless the step count or the routing time is 0; with any counted step the divisor is unchanged.

The rows above still show a whole-run outfall volume (0.048) next to statistics of an empty reporting period; that is [BND-19](../BND-19-report-start-mixes-whole-run-volumes/README.md).
