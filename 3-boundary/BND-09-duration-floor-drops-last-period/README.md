# BND-09: The run stops 1 s early and loses its last reporting period for many end times

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | The simulation ends one second before END_TIME, so the final reporting period is missing from the `.out` file, the report's time series and any interface file. No warning. 5.2.4: 59 of 120 whole-minute run lengths after a 00:00 start; 5.3.0 and 6.0.0: 10 of them (00:13, 00:26, 00:33, 00:49, ...) plus whole-hour runs such as 01:00 to 08:00. |
| **Reached from** | START_DATE/TIME and END_DATE/TIME in `[OPTIONS]`; in 5.3.0 also `swmm_setValueExpanded(swmm_SYSTEM, swmm_STARTDATE/swmm_ENDDATE, ...)`, in 6.0.0 `swmm_options_set_start_date/set_end_date()` |
| **5.3.0** | `project_readInput()` in [`src/legacy/engine/project.c:173`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L173); the same formula in `setSystemValue()` at [`swmm5.c:3242`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L3242) and [`swmm5.c:3270`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L3270) |
| **5.2.4** | Same defect with a different formula that fails for more end times: [`src/solver/project.c:165`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L165) |
| **6.0.0** | Reproduces: [`OptionsHandler.cpp:722`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/OptionsHandler.cpp#L722) copies 5.3.0's formula for parity, and [`SimulationOptions::totalDurationMs()`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SimulationOptions.hpp#L831), used after the dates are set through the API, has 5.2.4's |
| **Since** | Every 5.x release. Fork commit 57cf5fa9 ("Addressing TotalDuration precision bug #152", January 2024) changed which end times fail but kept the `floor()` |
| **Fix** | Round to the nearest second instead of truncating: [`BND-09_swmm530.patch`](BND-09_swmm530.patch), [`BND-09_swmm600.patch`](BND-09_swmm600.patch) |

## The problem

A run from 00:00 to 01:03 with a 1-minute reporting step should write 63 reporting periods. 5.2.4, 5.3.0 and 6.0.0 write 62: the routing clock stops at 01:02:59 and the 01:03 report time is never reached. Nothing in the report says so.

Which end times fail depends on the version. For runs that start at 00:00 and end on a whole minute of the same day, the formula is one second short for 704 of the 1439 end times in 5.2.4 (00:02, 00:03, 00:05, ..., 01:03, 10:00, ...) and for 82 of them in 5.3.0 and 6.0.0 (00:13, 00:26, 00:33, 00:49, 00:52, 01:03, ...). Among runs that start and end on whole hours of the same day, 55 of the 276 start/end pairs fail in 5.3.0 (01:00 to 08:00, 03:00 to 04:00, 05:00 to 06:00, ...). These counts come from evaluating each version's formula in double precision; the test below runs 122 of the cases through each engine.

Besides the missing period, every end-of-run state (final storage, hot start file, the interface file's last record) is taken one second early. In 5.2.4, 31 of the 73 regression decks have a duration the formula truncates; running two of them shows the lost period (extran9: 39 periods instead of 40, user1: 419 instead of 420). None of the 73 is affected in 5.3.0 or 6.0.0.

## Why it happens

Times of day are stored as fractions of a day (seconds / 86400), and the duration is the product of a difference of fractions and 86400, truncated:

```c
// src/legacy/engine/project.c, project_readInput()   (5.3.0)
// --- compute total duration of simulation in seconds
TotalDuration = floor((EndDate - StartDate) * SECperDAY + (EndTime - StartTime) * SECperDAY);

// src/solver/project.c   (5.2.4)
TotalDuration = floor((EndDateTime - StartDateTime) * SECperDAY);
```

For END_TIME 01:03, `EndTime` is 3780/86400 and `(EndTime - 0) * 86400` is 3779.9999999999995; for 00:13 it is 779.9999999999999. `floor()` turns these into 3779 and 779. 5.2.4 subtracts the combined serials (about 43831.04), whose rounding error is about 1e-11 day, so the product misses by a few 1e-7 s and falls below the whole second for about half of all end times; 5.3.0's change removed that large-magnitude cancellation but kept `floor()`.

The routing loop then stops at `TotalDuration` (`execRouting()` clips the last step to `TotalDuration - NewRoutingTime`), and `output_saveResults()` is only called when `NewRoutingTime >= ReportTime`. The last report time is the nominal end, one second later, so it is never written.

6.0.0 reproduces 5.3.0's formula on purpose; the comment there calls the result "exact whole-second ms":

```cpp
// src/engine/input/handlers/OptionsHandler.cpp
opt.total_duration_ms =
    std::floor((end_date_part - start_date_part) * 86400.0 +
               (end_time_part - start_time_part) * 86400.0) * 1000.0;

// src/engine/core/SimulationOptions.hpp, fallback after swmm_options_set_*_date()
return std::floor((end_date - start_date) * 86400.0) * 1000.0;
```

## How to reproduce

| File | What it is |
|---|---|
| [`BND-09_end0103.inp`](BND-09_end0103.inp) | One junction with a 1 cfs dry weather flow, one conduit, one outfall; KINWAVE, 60 s routing step, 1-minute reports, 00:00 to 01:03 |
| [`BND-09_test.c`](BND-09_test.c) | Legacy toolkit (5.2.4 and 5.3.0). Rewrites START_TIME/END_TIME of the deck for every whole minute from 00:01 to 02:00 after a 00:00 start, plus 00:00 to 10:00 and 01:00 to 08:00, runs each case and reads the number of periods from the `.out` file's closing records. With 5.3.0 it repeats the sweep with the end date set through `swmm_setValueExpanded()` |
| [`BND-09_test6.c`](BND-09_test6.c) | The same sweep for 6.0.0, from the input file and through `swmm_options_set_end_date()`; it also checks the time the last routing step ends at |

```sh
tools/run-test.sh BND-09            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-09 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (selected rows; the test lists every failing case):

```
---- 5.2.4 ----
Start  End    Set by      Periods  Expected
00:00  00:13  input file       13        13
00:00  00:18  input file       17        18  <-- last period missing
00:00  01:03  input file       62        63  <-- last period missing
00:00  10:00  input file      599       600  <-- last period missing
01:00  08:00  input file      420       420
FAIL: 59 of 122 runs do not write the last reporting period

---- 5.3.0 ----
00:00  00:13  input file       12        13  <-- last period missing
00:00  00:18  input file       18        18
00:00  01:03  input file       62        63  <-- last period missing
00:00  10:00  input file      600       600
01:00  08:00  input file      419       420  <-- last period missing
input file: 11 of 122 runs miss the last period (END_TIME, or START-END): 00:13 00:26 00:33 00:49 00:52 01:03 01:06 01:38 01:44 01:55 01:00-08:00
setValue: 11 of 122 runs miss the last period (END_TIME, or START-END): 00:13 00:26 00:33 00:49 00:52 01:03 01:06 01:38 01:44 01:55 01:00-08:00
FAIL: 22 of 244 runs do not write the last reporting period

---- 6.0.0 ----
Start  End    Set by      Periods  Expected  Last step ends (s)  Expected (s)
00:00  00:13  input file       12        13               779.0         780.0  <-- last period missing
00:00  01:03  input file       62        63              3779.0        3780.0  <-- last period missing
01:00  08:00  input file      419       420             25199.0       25200.0  <-- last period missing
00:00  00:18  API              17        18              1079.0        1080.0  <-- last period missing
00:00  10:00  API             599       600             35999.0       36000.0  <-- last period missing
input file: 11 of 122 runs end 1 s early (END_TIME, or START-END): 00:13 00:26 00:33 00:49 00:52 01:03 01:06 01:38 01:44 01:55 01:00-08:00
API: 60 of 122 runs end 1 s early (END_TIME, or START-END): 00:02 00:03 00:05 00:06 00:09 ...
FAIL: 71 of 244 runs stop 1 s before END_TIME and do not write the last reporting period
```

6.0.0 fails on the same cases as 5.3.0 when the dates come from the input file, and on 5.2.4's cases when they are set through the API.

**With the fix**, every run writes all its periods and ends at END_TIME:

```
---- 5.3.0 ----
00:00  01:03  input file       63        63
01:00  08:00  input file      420       420
00:00  01:03  setValue         63        63
PASS: all 244 runs write every reporting period up to END_TIME

---- 6.0.0 ----
00:00  01:03  input file       63        63              3780.0        3780.0
01:00  08:00  input file      420       420             25200.0       25200.0
00:00  10:00  API             600       600             36000.0       36000.0
PASS: all 244 runs reach END_TIME and write every reporting period
```

## The fix

Add half a second before the `floor()`, so the product, which is within about 1e-6 s of the whole number of seconds, rounds to it:

```diff
         // --- compute total duration of simulation in seconds
-        TotalDuration = floor((EndDate - StartDate) * SECperDAY + (EndTime - StartTime) * SECperDAY);
+        //     (rounded, since the day fractions rarely multiply out exactly)
+        TotalDuration = floor((EndDate - StartDate) * SECperDAY + (EndTime - StartTime) * SECperDAY + 0.5);
```

The 5.3.0 patch makes the same change in the two API setters in `swmm5.c`; the 6.0.0 patch makes it in `OptionsHandler.cpp` and in `SimulationOptions::totalDurationMs()`. Both patched engines give the same period counts and end times.

**Effect on other models.** The result only changes where the old value was a second short. For all 73 regression decks the 5.3.0/6.0.0 formula already gives a whole number of seconds (checked by evaluating it for each deck's dates), so the patch changes none of their results. A duration given with fractions of a second (a decimal-hour END_TIME such as 0.0001) is now rounded to the nearest second instead of truncated.
