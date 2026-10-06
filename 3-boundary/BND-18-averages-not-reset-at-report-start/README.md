# BND-18: With AVERAGES YES and a late REPORT_START, the first saved period averages the whole run up to it

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | The first saved value of every node and link in the .out is the mean since `START`, not the mean of its own reporting period. In the test, C1 flow 0.604 cfs instead of 0.050 cfs and J1 depth 0.26 ft instead of 0.08 ft. 5.3.0 also carries the inflated depth into "Reported Max Depth" (0.26 ft in a node whose Maximum Depth is 0.08 ft). No warning. |
| **Reached from** | `[REPORT] AVERAGES YES` together with `REPORT_START_DATE`/`REPORT_START_TIME` later than the start |
| **5.3.0** | `output_saveResults()` in [`src/legacy/engine/output.c:481`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L481) |
| **5.2.4** | Same code, [`src/solver/output.c:470`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/output.c#L470) |
| **6.0.0** | Reproduces with the same numbers: the `REPORT_START` gate in `SWMMEngine::postOutputSnapshot()` ([`src/engine/core/SWMMEngine.cpp:6088`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L6088)) returns before `applyAvgResults()`, the only place the accumulators are reset |
| **Since** | 5.1.013, which added `AVERAGES` |
| **Fix** | Reset the averages when a period before `REPORT_START` is skipped: [`BND-18_swmm530.patch`](BND-18_swmm530.patch), [`BND-18_swmm600.patch`](BND-18_swmm600.patch) |

## The problem

With `AVERAGES YES`, each value SWMM saves for a node or link is the mean of the end-of-step values of the routing steps in that reporting period. A late `REPORT_START` is the usual way to leave a spin-up or an earlier storm out of the results.

The test deck has a storm inflow from 0:00 to 2:30 on a 0.05 cfs base flow, `REPORT_START` 03:00 and a 15-minute report step. In the first saved period, C1 carries 0.050 cfs at every one of its 180 routing steps; the .out says 0.604 cfs, the mean of all 2160 steps since 0:00, storm included. J1's depth is 0.26 ft instead of 0.08 ft. From the second period on, the values are right. 5.2.4, 5.3.0 and 6.0.0 give the same numbers.

5.3.0 takes "Reported Max Depth" in the Node Depth Summary from the averaged values (since commit 7266536f, see [CRASH-11](../../5-crashes/CRASH-11-averages-report-out-of-bounds/README.md)), so the report contradicts itself:

```
                                 Average  Maximum  Maximum  Time of Max    Reported
                                   Depth    Depth      HGL   Occurrence   Max Depth
  Node                 Type         Feet     Feet     Feet  days hr:min        Feet
  ---------------------------------------------------------------------------------
  J1                   JUNCTION     0.08     0.08    10.08     0  00:00        0.26
```

## Why it happens

Every routing step adds the current results to the averages, whether or not reporting has started. The averages are only cleared after they are written:

```c
// src/legacy/engine/swmm5.c, saveResults()
if (NewRoutingTime >= ReportTime)
{
    if (RptFlags.averages)
    {
        if (NewRoutingTime == ReportTime) output_updateAvgResults();
        output_saveResults(ReportTime);          // saves and re-sets averages to 0
        if (NewRoutingTime > ReportTime) output_updateAvgResults();
    }
    ...
}
else if (RptFlags.averages)
    output_updateAvgResults();
```

```c
// src/legacy/engine/output.c, output_saveResults()
DateTime reportDate = getDateTime(reportTime);
...
if ( reportDate < ReportStart ) return;          // nothing saved, nothing re-set
...
if ( RptFlags.averages ) output_saveAvgResults(Fout.file);   // ends with output_initAvgResults()
```

Before `REPORT_START`, `output_saveResults()` returns before `output_saveAvgResults()`, so `output_initAvgResults()` never runs and the sums and the step count `Nsteps` keep growing until the first saved period.

6.0.0 follows the same order: `accumulateAvgResults()` every step, and `avg_.reset()` only at the end of `applyAvgResults()`, which `postOutputSnapshot()` reaches only after the `REPORT_START` gate.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-18_avg-late-start.inp`](BND-18_avg-late-start.inp) | J1 -> C1 -> J2 -> C2 -> O1, storm inflow at J1 from 0:00 to 2:30 on a 0.05 cfs base, `AVERAGES YES`, `REPORT_START` 03:00, fixed 5 s routing step |
| [`BND-18_test.c`](BND-18_test.c) | Steps the run with the legacy toolkit, reads J1 depth and C1 flow after every routing step, and checks every saved .out period against the mean of the steps that ended in it (tolerance 0.001) |
| [`BND-18_test6.c`](BND-18_test6.c) | The same through the 6.0.0 C API |
| [`BND-18_swmm530.patch`](BND-18_swmm530.patch), [`BND-18_swmm600.patch`](BND-18_swmm600.patch) | The fixes |

```sh
tools/run-test.sh BND-18            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-18 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4, 5.3.0 and 6.0.0 print the same table):

```
Saved period   J1 depth (ft)       C1 flow (cfs)       Steps
               .out    step mean   .out    step mean
02:45-03:00    0.2595  0.0828      0.6040  0.0500       180  <-- wrong
03:00-03:15    0.0828  0.0828      0.0500  0.0500       180
03:15-03:30    0.0828  0.0828      0.0500  0.0500       180
Mean of all 2160 steps from START to 03:00: J1 depth 0.2595 ft, C1 flow 0.6040 cfs
FAIL: 1 of 5 saved periods are not the mean of their own routing steps (first period J1 depth 0.2595 ft, should be 0.0828 ft)
BND-18 5.3.0 base: FAIL
```

5.2.4 says "1 of 4": it also drops the last period, which is [BND-09](../BND-09-duration-floor-drops-last-period/README.md).

**With the fix**, 5.3.0 and 6.0.0:

```
02:45-03:00    0.0828  0.0828      0.0500  0.0500       180
03:00-03:15    0.0828  0.0828      0.0500  0.0500       180
03:15-03:30    0.0828  0.0828      0.0500  0.0500       180
Mean of all 2160 steps from START to 03:00: J1 depth 0.2595 ft, C1 flow 0.6040 cfs
PASS: every saved period is the mean of the routing steps in that period
BND-18 5.3.0 patched: PASS
BND-18 6.0.0 patched: PASS
```

## The fix

Clear the averages when a period before `REPORT_START` is skipped, as saving it would have:

```diff
     // --- initialize system-wide results
-    if ( reportDate < ReportStart ) return;
+    if ( reportDate < ReportStart )
+    {
+        // --- periods before ReportStart are not saved, so start the
+        //     averages of the first saved period afresh
+        if ( RptFlags.averages ) output_initAvgResults();
+        return;
+    }
```

The caller's order is unchanged, so a routing step that straddles the report time still goes to the next period. The 6.0.0 patch adds `if (ctx_.options.rpt_averages) avg_.reset();` to the same gate in `postOutputSnapshot()`.

Effect on other models: only runs with both `AVERAGES YES` and a late `REPORT_START` change, and only in their first saved period. None of the 73 regression decks uses `AVERAGES`. With `AVERAGES YES` added to `update_v52/drop_grate_inlet_17.inp` and `update_v5111/bioretention.inp` (both have a late `REPORT_START`), the patched engines change period 0 of the .out and nothing else: every other period and the whole .rpt are byte-identical, and for drop_grate_inlet_17 the patched 5.3.0 and 6.0.0 .out files are identical to each other.
