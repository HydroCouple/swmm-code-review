# IO-02: Clock-time and step options accept nan, huge and negative values

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A malformed time option is accepted without an error. `START_TIME nan` gives a run whose summary tables show NaN and a start time of `-596523:..`; `END_TIME 1e30` (or `inf`) makes the run never finish; `REPORT_STEP -0.25` is read as a report step of 23:45:00, and `REPORT_STEP -1e10` as 6:00:00, through int overflow (undefined behaviour). These are malformed inputs, so the practical impact is low, but nothing tells the user. |
| **Reached from** | [OPTIONS] `START_TIME`, `END_TIME`, `REPORT_START_TIME`, `WET_STEP`, `DRY_STEP`, `REPORT_STEP`, `RULE_STEP` written as a bare number (decimal hours) |
| **5.3.0** | `project_readOption()` in [`src/legacy/engine/project.c:518`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L518-L523) (times) and [`:584-601`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L584-L601) (steps), on top of `datetime_strToTime()` in [`datetime.c:351`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/datetime.c#L351-L356) |
| **5.2.4** | Same code: [`src/solver/project.c:501`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L501) and [`:567-590`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L567-L590) |
| **6.0.0** | Reproduces: `handle_options()` stores the value of `parse_time_seconds()` unchecked ([`OptionsHandler.cpp:284`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/OptionsHandler.cpp#L284-L285), [`:314-328`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/OptionsHandler.cpp#L314-L328)); a NaN start time or a huge end time runs on past the end date, and a negative report step is used as it is |
| **Since** | Every release |
| **Fix** | Range-check the parsed value before it is stored or converted: [`IO-02_swmm530.patch`](IO-02_swmm530.patch), [`IO-02_swmm600.patch`](IO-02_swmm600.patch) |

## The problem

The input reference gives `START_TIME`, `END_TIME` and `REPORT_START_TIME` as a time of day (`HH:MM:SS`) and the `*_STEP` options as intervals. The legacy reader also takes a bare number as decimal hours, and then checks nothing about it. The test runs a 6-hour kinematic-wave model (one conduit, 1 cfs inflow, `REPORT_STEP 00:15:00`) with one option changed per deck:

| Deck | Option | 5.2.4 and 5.3.0 | 6.0.0 |
|---|---|---|---|
| `IO-02_start-time-nan.inp` | `START_TIME nan` | Accepted. The start date is NaN; the report prints `Starting Date ... 01/01/2020 -596523:` and node maxima "at" `-596523:-14` with depth `-nan`; exit code 0 | Accepted; runs on past 1 simulated day (stopped by the test) |
| `IO-02_end-time-1e30.inp` | `END_TIME 1e30` | Accepted; the run would last 4.2e28 days (still running after 1 simulated day, stopped by the test) | Same; the report header shows `Ending Date 01/01/0000` |
| `IO-02_report-step-neg.inp` | `REPORT_STEP -0.25` | Accepted as 23:45:00 (24 h minus 15 min), then cut to the 6-hour run length: one report period instead of 24 | Accepted as -0.25 s |
| `IO-02_report-step-huge.inp` | `REPORT_STEP -1e10` | Signed int overflow (undefined behaviour); without sanitizers the step wraps to a positive number and the report says `Report Time Step 06:00:00` | Accepted as -1e10 s; the output and report writers convert it to int (undefined behaviour) |

A run of the `REPORT_STEP -0.25` deck extended to 2 days reports `Report Time Step ......... 23:45:00` (5.3.0 without sanitizers). `END_TIME inf` behaves like `1e30`, and `nan` anywhere behaves like `START_TIME nan`.

## Why it happens

`datetime_strToTime()` first tries the whole token as a decimal number of hours:

```c
// src/legacy/engine/datetime.c, datetime_strToTime()
    *t = strtod(s, &endptr);
    if ( *endptr == 0 )
    {
        *t /= 24.0;
        return 1;
    }
```

Any value `strtod()` reads is a valid time: `nan`, `inf`, `1e30`, `-0.25`. The time-of-day options keep it as it is:

```c
// src/legacy/engine/project.c, project_readOption()
      case START_TIME:
        if ( !datetime_strToTime(s2, &StartTime) )
```

so `StartDate + StartTime` is NaN, or (for `END_TIME 1e30`) 4.2e28 days after the start. NaN then reaches `(int)` conversions in `datetime_decodeTime()` ([`datetime.c:236`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/datetime.c#L236)) every time a date is printed.

The steps are converted to seconds in `int`:

```c
// src/legacy/engine/project.c, project_readOption(), WET/DRY/REPORT/RULE_STEP
        datetime_decodeTime(aTime, &h, &m, &s);
        h += 24*(int)aTime;
        s = s + 60*m + 3600*h;
        ...
        else if ( s <= 0 ) return error_setInpError(ERR_NUMBER, s2);
```

`datetime_decodeTime()` returns the time of day of its argument, which for a negative value counts from the previous midnight: -0.25 h is 23:45 on the day before, `(int)aTime` is 0, and the step becomes 85,500 s, which passes `s <= 0`. For `-1e10` h, `24*(int)aTime` overflows (undefined behaviour); on x86 the sum wraps and the step comes out positive. Above 596,523 hours `3600*h` overflows the same way. Finally, `project_validate()` cuts a report step longer than the run down to the run length ([`project.c:176-178`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L176-L178)), which is why the 6-hour test deck shows 21,600 s.

6.0.0 reads all of these options with `parse_time_seconds()` (a bare number is seconds there, not hours; see [IO-04](../IO-04-step-options-three-unit-conventions/)) and stores the result without any check except for `ROUTING_STEP`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-02_valid.inp`](IO-02_valid.inp) | Control: 01/01/2020 0:00 to 6:00, `REPORT_STEP 00:15:00`, one conduit, kinematic wave, 30 s routing step |
| [`IO-02_start-time-nan.inp`](IO-02_start-time-nan.inp) | The control deck with `START_TIME nan` |
| [`IO-02_end-time-1e30.inp`](IO-02_end-time-1e30.inp) | The control deck with `END_TIME 1e30` |
| [`IO-02_report-step-neg.inp`](IO-02_report-step-neg.inp) | The control deck with `REPORT_STEP -0.25` |
| [`IO-02_report-step-huge.inp`](IO-02_report-step-huge.inp) | The control deck with `REPORT_STEP -1e10` |
| [`IO-02_test.c`](IO-02_test.c) | Opens each deck with the legacy toolkit and, if it opens, runs it for at most 1 simulated day, printing the step count, last elapsed time, report step and start date. Correct: the control deck ends at 6:00 with a 900 s report step, every other deck is refused by `swmm_open` |
| [`IO-02_test6.c`](IO-02_test6.c) | The same check with the 6.0.0 API |

```sh
tools/run-test.sh IO-02            # 5.2.4, 5.3.0 and 6.0.0: CRASH (undefined behaviour), and FAIL
tools/run-test.sh IO-02 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same as 5.3.0, with its own line numbers). The sanitizer stops the legacy test at the int overflow of the last deck, so there is no final FAIL line:

```
---- IO-02 on 5.3.0 (base) ----
Deck                         open  steps  elapsed (d)  report step s       start date
IO-02_valid.inp                 0    719       0.2497            900       43831.0000
../src/src/legacy/engine/datetime.c:236:12: runtime error: nan is outside the range of representable values of type 'int'
../src/src/legacy/engine/swmm5.c:3579:17: runtime error: nan is outside the range of representable values of type 'int'
IO-02_start-time-nan.inp        0      0       0.0000            900              nan  <-- accepted
IO-02_end-time-1e30.inp         0   2881       1.0003            900       43831.0000  <-- still running, stopped
IO-02_report-step-neg.inp       0    719       0.2497          21600       43831.0000  <-- accepted
../src/src/legacy/engine/project.c:593:16: runtime error: signed integer overflow: 24 * -416666666 cannot be represented in type 'int'
IO-02 5.3.0 base: CRASH
---- IO-02 on 6.0.0 (base) ----
Deck                         open  steps  elapsed (d)  report step s       start date
IO-02_valid.inp                 0    720       0.2500            900       43831.0000
../src/src/engine/controls/../core/DateTime.hpp:128:30: runtime error: nan is outside the range of representable values of type 'int'
...
../src/src/engine/hydrology/Inflow.cpp:451:39: runtime error: nan is outside the range of representable values of type 'int'
IO-02_start-time-nan.inp        0   2881       1.0003            900              nan  <-- still running, stopped
IO-02_end-time-1e30.inp         0   2881       1.0003            900       43831.0000  <-- still running, stopped
IO-02_report-step-neg.inp       0    720       0.2500             -0       43831.0000  <-- accepted
../src/src/engine/plugins/DefaultOutputPlugin.cpp:412:32: runtime error: -1e+10 is outside the range of representable values of type 'int'
../src/src/engine/plugins/DefaultReportPlugin.cpp:844:32: runtime error: -1e+10 is outside the range of representable values of type 'int'
IO-02_report-step-huge.inp      0    720       0.2500   -10000000000       43831.0000  <-- accepted
FAIL: 4 of 5 decks handled wrongly; an invalid START_TIME, END_TIME or REPORT_STEP is accepted instead of raising an input error
IO-02 6.0.0 base: CRASH
```

The legacy engine returns 0 instead of the final elapsed time, so its last non-zero value is one 30-s step short of 0.25 d; the test allows for that. 6.0.0 reads the bare `-0.25` as seconds, printed as `-0`.

**With the fix:**

```
---- IO-02 on 5.3.0 (patched) ----
Deck                         open  steps  elapsed (d)  report step s       start date
IO-02_valid.inp                 0    719       0.2497            900       43831.0000
IO-02_start-time-nan.inp      200      -            -              -                -
IO-02_end-time-1e30.inp       200      -            -              -                -
IO-02_report-step-neg.inp     200      -            -              -                -
IO-02_report-step-huge.inp    200      -            -              -                -
PASS: nan, out-of-range and overflowing time options are refused at swmm_open, and the control deck runs 6 h with a 900 s report step
IO-02 5.3.0 patched: PASS
---- IO-02 on 6.0.0 (patched) ----
...
IO-02_report-step-huge.inp      5      -            -              -                -
PASS: nan, out-of-range and overflowing time options are refused at swmm_engine_open, and the control deck runs 6 h with a 900 s report step
IO-02 6.0.0 patched: PASS
```

and the reports name the option (5.3.0 shown; 6.0.0 gives the same codes):

```
  ERROR 213: invalid date/time nan at line 8 of [OPTION] section:
  ERROR 213: invalid date/time 1e30 at line 12 of [OPTION] section:
  ERROR 211: invalid number -0.25 at line 13 of [OPTION] section:
  ERROR 211: invalid number -1e10 at line 13 of [OPTION] section:
```

## The fix

5.3.0: the three time-of-day options must lie between 0:00:00 and 24:00:00, and a step must be non-negative and fit in an int number of seconds before it is converted. Both tests are written so that NaN fails them:

```diff
       case START_TIME:
-        if ( !datetime_strToTime(s2, &StartTime) )
+        if ( !datetime_strToTime(s2, &StartTime) ||
+             !(StartTime >= 0.0 && StartTime <= 1.0) )  // 0:00 to 24:00
```

```diff
         {
             return error_setInpError(ERR_DATETIME, s2);
         }
+
+        // --- reject NaN, negative steps and steps whose seconds overflow an int
+        if ( !(aTime >= 0.0 && aTime * SECperDAY < 2147483647.0) )
+            return error_setInpError(ERR_NUMBER, s2);
         datetime_decodeTime(aTime, &h, &m, &s);
```

The existing `s <= 0` / `s < 0` tests still decide whether a zero step is allowed (only for `RULE_STEP`). `END_TIME` and `REPORT_START_TIME` get the same check as `START_TIME`. A time of day written as `24:00:00` is still accepted.

6.0.0: `handle_options()` applies the same limits to the value from `parse_time_seconds()` (0 to 86,400 s for the times; above 0 and below 2^31 s for the steps, 0 allowed for `RULE_STEP`) and does not store a rejected value, so the report written on a failed open does not print it.

**What changes for users.** A deck that wrote a time of day above 24 hours (for example `END_TIME 30`, meaning 6:00 the next day) is now refused; none of the 73 regression decks does this (all their `START_TIME`, `END_TIME` and `REPORT_START_TIME` values are clock times between 0:00:00 and 23:59:59). Valid options are read as before, so results do not change. With the patched 5.3.0 and 6.0.0 engines the 69 regression decks that finish within the time limit give the same exit codes, error messages and continuity errors as the unpatched engines.

## Notes

- With [IO-01](../IO-01-nan-inf-accepted-as-numbers/)'s patch, `datetime_strToTime()` itself rejects `nan` and `inf`; this patch also rejects them on its own, through the range checks.
- A clock string with a huge hour, such as `END_TIME 600000:00:00`, overflows earlier, in `datetime_encodeTime()` (`hour * 3600` in int), before these checks run. That is not fixed here.
- Which unit a bare number means (hours for these options, seconds for `ROUTING_STEP`, seconds for everything in 6.0.0) is [IO-04](../IO-04-step-options-three-unit-conventions/).
