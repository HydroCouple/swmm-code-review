# IO-35: REPORT_START_DATE is ignored when REPORT_START_TIME is not given

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | The requested report window is silently replaced by the whole run. The .out file and the statistics taken from saved periods (such as Max Reported Depth) start at the simulation start instead of REPORT_START_DATE. In the test, the .out file holds 30 hourly periods from 01/01 01:00 instead of 7 from 01/02 00:00. No warning |
| **Reached from** | `[OPTIONS]` with `REPORT_START_DATE` and no `REPORT_START_TIME`, as in hand-written or script-generated decks (the EPA GUI always writes both) |
| **5.3.0** | `setDefaults()` in [`src/legacy/engine/project.c:923`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L923), used by `project_readInput()` at [`project.c:158`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L158) |
| **5.2.4** | Same code, [`src/solver/project.c:889`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L889) and [`project.c:150`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L150) |
| **6.0.0** | Not affected: a date given alone is combined with a 00:00:00 time ([`src/engine/input/handlers/OptionsHandler.cpp:727`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/OptionsHandler.cpp#L727)) |
| **Since** | Every release in the repository (initial commit, 2014) |
| **Fix** | Default REPORT_START_TIME to 00:00:00: [`IO-35_swmm530.patch`](IO-35_swmm530.patch) |

## The problem

The input reference gives REPORT_START_DATE ("Date on which reporting of results begins") a default of the start date, and REPORT_START_TIME ("Time of day on REPORT_START_DATE at which reporting begins") a default of 00:00:00. A deck that runs from 01/01/2020 00:00 to 01/02/2020 06:00 with a 1-hour report step and `REPORT_START_DATE 01/02/2020` should therefore report 7 periods, 01/02 00:00 to 06:00.

5.2.4 and 5.3.0 report all 30 hours instead, from the first report step after the simulation start. Adding `REPORT_START_TIME 00:00:00`, the documented default, gives the expected 7 periods. Nothing in the report says the date was not used.

## Why it happens

Both parts of the report start default to `NO_DATE`, day −693594 (1 January 0001):

```c
// src/legacy/engine/project.c, setDefaults()
   ReportStartDate = NO_DATE;
   ReportStartTime = NO_DATE;

// src/legacy/engine/project.c, project_readInput()
    ReportStart   = ReportStartDate + ReportStartTime;
    ReportStart   = MAX(ReportStart, StartDateTime);
```

With only the date given, `ReportStart` is 01/02/2020 minus 693594 days, about 1900 years before the simulation, and the `MAX()` replaces it with the simulation start. With neither given, or with only the time, the sum is also before the start, which is the intended "report from the start" result. Only the date-without-time case is wrong: the time's default should be 0, like START_TIME's and END_TIME's.

6.0.0 keeps the date and time parts separately, with 0 for a missing time, and adds them when either was given.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-35_date-only.inp`](IO-35_date-only.inp) | 01/01/2020 00:00 to 01/02/2020 06:00, 1-hour report step, `REPORT_START_DATE 01/02/2020` |
| [`IO-35_date-and-time.inp`](IO-35_date-and-time.inp) | The same with `REPORT_START_TIME 00:00:00` |
| [`IO-35_test.c`](IO-35_test.c) | Runs both decks through the legacy toolkit and reads each .out file's period count and first period date; both decks must give 7 periods from 01/02/2020 00:00 |
| [`IO-35_test6.c`](IO-35_test6.c) | The same check through the 6.0.0 API |

```sh
tools/run-test.sh IO-35            # 5.2.4, 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-35 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0):

```
Reporting periods in the .out file (asked for: from 01/02/2020):
  deck            error  periods   first period
  date-only           0       30   01/01/2020 01:00
  date-and-time       0        7   01/02/2020 00:00
  (correct: 7 periods, the first at 01/02/2020 00:00, for both decks)
FAIL: date-only reports 30 periods from 01/01/2020 01:00 instead of 7 from 01/02/2020 00:00
IO-35 5.3.0 base: FAIL
```

6.0.0 gives 7 periods from 01/02/2020 00:00 for both decks (`IO-35 6.0.0 base: PASS`).

**With the fix:**

```
  date-only           0        7   01/02/2020 00:00
  date-and-time       0        7   01/02/2020 00:00
  (correct: 7 periods, the first at 01/02/2020 00:00, for both decks)
PASS: reporting starts on REPORT_START_DATE whether or not REPORT_START_TIME is given
IO-35 5.3.0 patched: PASS
```

## The fix

```diff
    ReportStartDate = NO_DATE;
-   ReportStartTime = NO_DATE;
+   ReportStartTime = 0.0;              // 00:00:00 on REPORT_START_DATE
```

When REPORT_START_DATE is omitted, `NO_DATE` plus any time is still before the simulation start, so reporting begins with the simulation as before; the 5.3.0 API setter for the report start (`swmm5.c:3275`) sets both parts itself and is unaffected.

**Effect on other models.** Every one of the 48 regression decks that sets REPORT_START_DATE also sets REPORT_START_TIME, so none changes.
