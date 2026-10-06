# NUM-44: In leap years, month/day dates for street sweeping and DAYOFYEAR rules are matched one day early

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | In every leap year, a control rule or sweeping season keyed on a month/day after Feb 28 acts one day early. `DAYOFYEAR >= 03/01` fires on Feb 29, `DAYOFYEAR = 12/31` holds on Dec 30, and Dec 31 is outside every sweeping season, including the default season (01/01 to 12/31). Nothing in the report points to it. In a continuous simulation this recurs one year in four. |
| **Reached from** | `[OPTIONS]` SWEEP_START / SWEEP_END; `[CONTROLS]` premises on `SIMULATION DAYOFYEAR`; simulations that include a leap year after Feb 28 |
| **5.3.0** | Dates converted with 1947 in `project_readOption()` [`src/legacy/engine/project.c:563`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L563) and `getPremiseValue()` [`src/legacy/engine/controls.c:1396`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1396); compared with the current date's day of year in `getVariableValue()` [`controls.c:1861`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1861) and `runoff_execute()` [`runoff.c:212`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L212) |
| **5.2.4** | Same code: [`project.c:546`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L546), [`controls.c:872`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/controls.c#L872), [`controls.c:1314`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/controls.c#L1314), [`runoff.c:208`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/runoff.c#L208) |
| **6.0.0** | Reproduces: month/day is converted with a non-leap year in [`OptionsHandler.cpp:411`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/OptionsHandler.cpp#L411) and [`Controls.cpp:971`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/controls/Controls.cpp#L971), and compared with `datetime::dayOfYear()` in [`Controls.cpp:621`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/controls/Controls.cpp#L621) and [`SWMMEngine.cpp:2934`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L2934) |
| **Since** | SWEEP_START/SWEEP_END: SWMM 5.0 (present in the 5.0.022 code of the repository's first commit). DAYOFYEAR premise: 5.1.011 |
| **Fix** | Compare in one 365-day calendar: [`NUM-44_swmm530.patch`](NUM-44_swmm530.patch), [`NUM-44_swmm600.patch`](NUM-44_swmm600.patch) |

## The problem

SWEEP_START, SWEEP_END and the right-hand side of a `SIMULATION DAYOFYEAR` premise are given as month/day (MM/DD). SWMM turns each into a day number by appending the year 1947 and taking the day of year, so 03/01 becomes 60 and 12/31 becomes 365. During the run it compares these numbers with the day of year of the current date. In a leap year that is 60 for Feb 29, 61 for Mar 1 and 366 for Dec 31. So in leap years:

- every MM/DD after 02/28 matches the day before: `DAYOFYEAR >= 03/01` is true on Feb 29, `DAYOFYEAR = 12/31` on Dec 30;
- Dec 31 (366) is beyond every converted date: it is never inside a sweeping season, including the default one (01/01 to 12/31), and no rule can name it. The numeric form of DAYOFYEAR is limited to 1 to 365, and 02/29 is rejected as an invalid date.

In 2021 everything happens on the right day. The test decks run a few days around Mar 1 and Dec 31 in 2020 and in 2021, with two rules (R1: `DAYOFYEAR >= 03/01` sets orifice OR1 to 0.5; R2: `DAYOFYEAR = 12/31` sets OR2 to 0.5) and daily sweeping of a buildup of 1 lb/ac/day. The report's control action log of the 2020 decks from 5.3.0:

```
   02/29/2020: 00:00:00 Link OR1 setting changed to   0.50 by Control R1
   12/30/2020: 00:30:00 Link OR2 setting changed to   0.50 by Control R2
   12/31/2020: 00:00:00 Link OR2 setting changed to   1.00 by Control R2
```

OR2 is set for 12/31 on Dec 30, and reset on Dec 31. In the March deck the sweeping season ends on 03/01, so Mar 1 is not swept in 2020 (1 lb removed instead of 2). In the December deck the default season leaves Dec 31 unswept (0 lb instead of 1).

## Why it happens

```c
// src/legacy/engine/project.c, project_readOption()
      // --- day of year when street sweeping begins or when it ends
      //     (year is arbitrarily set to 1947 so that the dayOfYear
      //      function can be applied)
      case SWEEP_START:
      case SWEEP_END:
        sstrncpy(strDate, s2, 24);
        sstrcat(strDate, "/1947", 25);
        ...
        m = datetime_dayOfYear(aDate);            // 03/01 -> 60, 12/31 -> 365

// src/legacy/engine/controls.c, getPremiseValue()
    case r_DAYOFYEAR:
        sstrncpy(strDate, token, 6);
        sstrcat(strDate, "/1947", 25);
        if (datetime_strToDate(strDate, value))
            *value = datetime_dayOfYear(*value);  // same conversion
        else if (!getDouble(token, value) || *value < 1 || *value > 365)
            return error_setInpError(ERR_DATETIME, token);

// src/legacy/engine/controls.c, getVariableValue()
    case r_DAYOFYEAR:
        return datetime_dayOfYear(CurrentDate);   // 61 for 03/01/2020, 366 for 12/31/2020

// src/legacy/engine/runoff.c, runoff_execute()
    day = datetime_dayOfYear(currentDate);
    ...
        if ( day >= SweepStart && day <= SweepEnd ) canSweep = TRUE;
```

The stored dates use a 365-day calendar and the current date uses the real one. 6.0.0 stores the dates the same way (year 1947 in `Controls.cpp`, year 2001 in `OptionsHandler.cpp`, and `InpWriter` and the API getters convert them back with 2001), and compares them with `datetime::dayOfYear()` of the current date in `Controls.cpp` and in the sweeping blocks of `SWMMEngine.cpp` and `SurfaceQuality2D.cpp`.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-44_2020-mar.inp`](NUM-44_2020-mar.inp) | 02/28/2020 00:30 to 03/01/2020 12:00; rules R1 and R2; sweeping season 01/01 to 03/01, swept every day with 100% removal, TSS buildup 1 lb/ac/day on 1 ac, no rain |
| [`NUM-44_2021-mar.inp`](NUM-44_2021-mar.inp) | The same in 2021 |
| [`NUM-44_2020-dec.inp`](NUM-44_2020-dec.inp) | 12/30/2020 00:30 to 12/31/2020 12:00; the same rules and buildup; default sweeping season |
| [`NUM-44_2021-dec.inp`](NUM-44_2021-dec.inp) | The same in 2021 |
| [`NUM-44_test.c`](NUM-44_test.c) | Steps each deck through the legacy toolkit, records the date (as the rules saw it) on which OR1 and OR2 are 0.5, reads "Sweeping Removal" from the report, and checks them against the calendar |
| [`NUM-44_test6.c`](NUM-44_test6.c) | The same through the 6.0.0 C API (`swmm_link_get_target_setting()`, `swmm_get_current_time()`) |

Expected in both years: OR1 goes to 0.5 on 03/01; OR2 is 0.5 on 12/31 only; the March season sweeps on 02/29 (2020) and 03/01, removing 2 lb in 2020 and 1 lb in 2021; the December deck sweeps on 12/31 and removes 1 lb. Removal is checked to ±0.25 lb; aligning the hourly steps with the sweep times adds up to about 0.06 lb, while one missing sweep is 1 lb.

```sh
tools/run-test.sh NUM-44            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-44 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same as 5.3.0):

```
          OR1 = 0.5 from    OR2 = 0.5 on            Sweeping removal (lb)
Case      seen    expected  seen        expected    seen     expected
2020 mar  02/29   03/01     never       never        1.062    2.000
2021 mar  03/01   03/01     never       never        1.062    1.000
2020 dec  -       -         12/30       12/31        0.000    1.000
2021 dec  -       -         12/31       12/31        1.062    1.000
FAIL: month/day dates are matched on the wrong day; 2020 mar: DAYOFYEAR >= 03/01 fired on 02/29/2020; 2020 mar: sweeping removed 1.062 lb, not 2.0; 2020 dec: DAYOFYEAR = 12/31 held on 12/30; 2020 dec: sweeping removed 0.000 lb, not 1.0
NUM-44 5.2.4 base: FAIL
NUM-44 5.3.0 base: FAIL
```

6.0.0 shows the same dates. Its single sweeps remove 1.021 lb where 5.3.0 removes 1.062 lb, with or without the fix; the difference is one hour of buildup and has nothing to do with this issue.

```
2020 mar  02/29   03/01     never       never        1.021    2.000
2020 dec  -       -         12/30       12/31        0.000    1.000
NUM-44 6.0.0 base: FAIL
```

**With the fix**, 5.3.0:

```
          OR1 = 0.5 from    OR2 = 0.5 on            Sweeping removal (lb)
Case      seen    expected  seen        expected    seen     expected
2020 mar  03/01   03/01     never       never        2.062    2.000
2021 mar  03/01   03/01     never       never        1.062    1.000
2020 dec  -       -         12/31       12/31        1.062    1.000
2021 dec  -       -         12/31       12/31        1.062    1.000
PASS: DAYOFYEAR 03/01 and 12/31 and the sweeping season match the same calendar days in the leap and the non-leap year
NUM-44 5.3.0 patched: PASS
```

and 6.0.0, with the same dates:

```
2020 mar  03/01   03/01     never       never        2.062    2.000
2021 mar  03/01   03/01     never       never        1.021    1.000
2020 dec  -       -         12/31       12/31        1.021    1.000
2021 dec  -       -         12/31       12/31        1.021    1.000
PASS: DAYOFYEAR 03/01 and 12/31 and the sweeping season match the same calendar days in the leap and the non-leap year
NUM-44 6.0.0 patched: PASS
```

## The fix

Keep the stored day numbers, which the 5.3.0 toolkit values `swmm_SWEEPSTART`/`swmm_SWEEPEND` and 6.0.0's `InpWriter`, API getters and GeoPackage files already use, and convert the current date into the same 365-day calendar before comparing. A new `datetime_dayOfYear365()` returns the day a date's month and day have in a non-leap year. In a leap year it subtracts 1 from Mar 1 on and gives Feb 29 the value 59.5, between Feb 28 and Mar 1:

```c
double datetime_dayOfYear365(DateTime date)
{
    int year, month, day;
    double doy = datetime_dayOfYear(date);
    datetime_decodeDate(date, &year, &month, &day);
    if ( isLeapYear(year) && month > 2 ) doy -= 1.0;
    else if ( month == 2 && day == 29 ) doy -= 0.5;
    return doy;
}
```

```diff
     case r_DAYOFYEAR:
-        return datetime_dayOfYear(CurrentDate);
+        return datetime_dayOfYear365(CurrentDate);
```

```diff
-    int      day;                      // day of calendar year
+    double   day;                      // day of calendar year
 ...
-    day = datetime_dayOfYear(currentDate);
+    day = datetime_dayOfYear365(currentDate);
```

The 6.0.0 patch adds `datetime::dayOfYear365()` to `DateTime.hpp` and uses it for `SIM_DAYOFYEAR` and for the sweeping season in `SWMMEngine.cpp` and in the 2-D surface quality module (`SurfaceQuality2D.cpp`), which copies the same rule.

What changes for users, in leap years only:

- MM/DD dates after 02/28 act on the right day. Dec 31 is inside a season that ends on 12/31, and `DAYOFYEAR = 12/31` holds on Dec 31.
- Feb 29 is after 02/28 and before 03/01: it is in a season that ends on 03/01 or starts on 02/28 or earlier, but not in one that ends on 02/28 or starts on 03/01. No `=` premise matches it, and 02/29 is still not accepted as a premise value.
- A numeric DAYOFYEAR value N (1 to 365) now means the same day of the 365-day calendar as the MM/DD form, so from N = 60 on it acts one day later in leap years than before (`DAYOFYEAR = 60` is Mar 1 in every year). The two forms of the same date stay equivalent, as they are today.

Non-leap years and leap-year dates before Feb 29 give exactly the same day numbers as before. None of the 73 decks in the regression suite uses DAYOFYEAR or street sweeping (the two with land uses, `Example1.inp` and `events_example.inp`, have no sweep interval and run in January and February 1998), and the 6.0.0 test decks that sweep run in January 2026, so no regression result changes.
