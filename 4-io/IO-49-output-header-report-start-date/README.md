# IO-49: The .out header's report start date is wrong when REPORT_START is later than START

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A reader that builds the time axis from the binary output header (`SMO_getStartDate` + `SMO_getTimes(SMO_reportStep)`, the only date information the legacy reader API exposes) dates every result one report step too early in 5.2.4 and 5.3.0 (322 of 480 test runs) and up to 89 steps too early in 6.0.0 (399 of 480). No warning; the .rpt and the dates stored with each period are right. |
| **Reached from** | Any input file with `REPORT_START_DATE`/`REPORT_START_TIME` later than the start by more than one report step |
| **5.3.0** | `output_open()` in [`src/legacy/engine/output.c:397`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L397) |
| **5.2.4** | Same code, [`src/solver/output.c:387`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/output.c#L387) |
| **6.0.0** | Reproduces, differently: `DefaultOutputPlugin::writeHeader()` writes `START_DATE` whatever `REPORT_START` is ([`src/engine/plugins/DefaultOutputPlugin.cpp:411`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultOutputPlugin.cpp#L411)), so the error grows with the gap |
| **Since** | 5.0.014 (the formula carries a `5.0.014 - LR` tag in the first commit of the EPA repository) |
| **Fix** | Count whole seconds to the first saved period and round up: [`IO-49_swmm530.patch`](IO-49_swmm530.patch), [`IO-49_swmm600.patch`](IO-49_swmm600.patch) |

## The problem

The binary output file does not store the date of period 0 in its header. It stores a "report start date" one report step before it, and readers date period *k* (0-based) as

    header date + (k + 1) * REPORT_STEP

`output_open()` says so in its own comment ("make saved starting report date one reporting period prior to the date of the first reported result"). The legacy reader API has no per-period date function: `SMO_getStartDate()` and `SMO_getTimes(SMO_reportStep)` are all a client gets.

When `REPORT_START` is later than `START`, the header date is usually wrong:

- **5.2.4 and 5.3.0** write a date one report step too early in most cases. With `START 00:00`, `REPORT_START 01:00` and a 15-minute step, the first saved period is 01:00, so the header should say 00:45. It says 00:30, and every result is dated 15 minutes early. With a 60-minute step the header says 23:00 the previous day.
- **6.0.0** always writes `START`, so the same deck gives 00:00 and every result is dated 3 steps (45 minutes) early. With a 1-minute step and `REPORT_START 01:00`, 59 steps early.

Two of the regression decks show it. `update_v52/drop_grate_inlet_17.inp` (START 00:00, REPORT_START 01:00, step 15 min): 5.3.0 header 00:30, 6.0.0 header 00:00, should be 00:45. `update_v5111/bioretention.inp` (START 17:14, REPORT_START 18:40, step 5 min, first saved period 18:44): 5.3.0 header 18:34, 6.0.0 header 17:14, should be 18:39.

The .rpt file and the date stored in front of each period are not affected. 6.0.0's own reader has `swmm_output_get_period_time()`, which reads the stored dates; readers that use it get the right times.

## Why it happens

```c
// src/legacy/engine/output.c, output_open()
z = (double)ReportStep/86400.0;
if ( StartDateTime + z > ReportStart ) z = StartDateTime;
else
{
    z = floor((ReportStart - StartDateTime)/z) - 1.0;
    z = StartDateTime + z*(double)ReportStep/86400.0;
}
```

`output_saveResults()` writes the first report time `j * REPORT_STEP` (plus SWMM's 1 ms) that is not before `ReportStart`. With `D` seconds from `START` to `REPORT_START`, that is `j = ceil(D / REPORT_STEP)`, and the header should be `START + (j - 1) * REPORT_STEP`. The code takes `floor()` instead, which is wrong in two ways:

- **REPORT_START between report times** (e.g. 00:20 with a 15-minute step): `floor(1.33) - 1 = 0` instead of `ceil(1.33) - 1 = 1`. One step early.
- **REPORT_START on a report time** (e.g. 01:00 with a 15-minute step): the times are day fractions added to the date (day 43831) and lose precision there: the quotient is 3.99999999977, not 4, so `floor()` gives 3 and the header is again one step early. Whether the quotient lands just below or just above the whole number depends on the dates, which is why some aligned cases are right.

6.0.0 does not compute anything:

```cpp
// src/engine/plugins/DefaultOutputPlugin.cpp, DefaultOutputPlugin::writeHeader()
// Report start date and step
writeReal8(ctx.options.start_date);
writeInt4(static_cast<int>(ctx.options.report_step));
```

## How to reproduce

| File | What it is |
|---|---|
| [`IO-49_report-start.inp`](IO-49_report-start.inp) | One junction with a constant 1 cfs inflow, one conduit, one outfall; START 00:00, END 03:00 |
| [`IO-49_test.c`](IO-49_test.c) | 5.2.4 and 5.3.0: rewrites `REPORT_START_TIME` and `REPORT_STEP` for steps of 1, 5, 15 and 60 min and every report start from 00:01 to 02:00 (480 runs), and checks in each .out that header date + `REPORT_STEP` equals the stored date of the first period (to 1 s) |
| [`IO-49_test6.c`](IO-49_test6.c) | The same runs through `swmm_engine_run()` |
| [`IO-49_swmm530.patch`](IO-49_swmm530.patch), [`IO-49_swmm600.patch`](IO-49_swmm600.patch) | The fixes |

```sh
tools/run-test.sh IO-49            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-49 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (selected rows; "Error" is header minus correct header, in report steps):

```
5.2.4 and 5.3.0 (identical):
Step  Rpt start  First period  Header      Should be   Error (steps)
  1m  00:02      00:02         00:00     00:01     -1
  1m  01:00      01:00         00:58     00:59     -1
 15m  00:20      00:30         00:00     00:15     -1
 15m  01:00      01:00         00:30     00:45     -1
 60m  01:00      01:00         23:00-1d  00:00     -1
 60m  01:30      02:00         00:00     01:00     -1
  REPORT_STEP  1 min: 58
  REPORT_STEP  5 min: 103
  REPORT_STEP 15 min: 101
  REPORT_STEP 60 min: 60
FAIL: in 322 of 480 runs header date + REPORT_STEP is not the first saved period's date

6.0.0:
  1m  00:02      00:02         00:00     00:01     -1
  1m  01:00      01:00         00:00     00:59     -59
 15m  00:20      00:30         00:00     00:15     -1
 15m  01:00      01:00         00:00     00:45     -3
 60m  01:00      01:00         00:00     00:00     +0
 60m  01:30      02:00         00:00     01:00     -1
  REPORT_STEP  1 min: 119
  REPORT_STEP  5 min: 115
  REPORT_STEP 15 min: 105
  REPORT_STEP 60 min: 60
FAIL: in 399 of 480 runs header date + REPORT_STEP is not the first saved period's date
```

**With the fix**, both engines give the same headers:

```
  1m  00:02      00:02         00:01     00:01     +0
  1m  01:00      01:00         00:59     00:59     +0
 15m  00:20      00:30         00:15     00:15     +0
 15m  01:00      01:00         00:45     00:45     +0
 60m  01:00      01:00         00:00     00:00     +0
 60m  01:30      02:00         01:00     01:00     +0
  REPORT_STEP  1 min: 0
  REPORT_STEP  5 min: 0
  REPORT_STEP 15 min: 0
  REPORT_STEP 60 min: 0
PASS: in all 480 runs header date + REPORT_STEP is the first saved period's date
IO-49 5.3.0 patched: PASS
IO-49 6.0.0 patched: PASS
```

## The fix

Count the seconds to `REPORT_START` with `datetime_timeDiff()`, which works in whole seconds, and round up:

```diff
-        z = floor((ReportStart - StartDateTime)/z) - 1.0;
+        // --- the first saved period is the first report time at or
+        //     after ReportStart (count whole seconds, not day fractions)
+        z = ceil(datetime_timeDiff(ReportStart, StartDateTime) /
+                 (double)ReportStep) - 1.0;
         z = StartDateTime + z*(double)ReportStep/86400.0;
```

The 6.0.0 patch computes the same value in `writeHeader()` with `datetime::timeDiff()` and the same arithmetic, so both engines write the same double.

Only the header date of .out files with a late `REPORT_START` changes; no simulation result, no stored period date and no .rpt line changes. In 5.3.0 the header moves one step later wherever it was wrong; in 6.0.0 it moves to the first saved period minus one step. Readers that relied on the stored per-period dates are unaffected.
