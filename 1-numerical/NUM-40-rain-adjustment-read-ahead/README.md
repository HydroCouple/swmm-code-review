# NUM-40: The monthly rainfall adjustment is applied when a rain record is read ahead, not in the month the rain falls

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Wrong rainfall with no warning when `[ADJUSTMENTS] RAINFALL` is used. The first rain record of the run is never adjusted, and the first record after a dry spell that crosses into a new month gets the previous month's factor. In the test, two 1 in storms under factors of 2.0 (January) and 3.0 (February) fall as 1.000 and 2.000 in instead of 2.000 and 3.000 in: 3.000 in of precipitation instead of 5.000 in. |
| **Reached from** | Any input file with an `[ADJUSTMENTS]` `RAINFALL` line whose factors are not all 1.0, with rain from a time series or a rain file; also RDII rainfall |
| **5.3.0** | `convertRainfall()` in [`src/legacy/engine/gage.c:710`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L710), called by `getFirstRainfall()` and `getNextRainfall()` |
| **5.2.4** | Same code, [`src/solver/gage.c:702`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gage.c#L702) |
| **6.0.0** | Fixed already: the record rates carry no monthly factor, and the factor of the current month is applied at each runoff step ([`src/engine/core/SWMMEngine.cpp:2105`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L2105)) and to reported rainfall ([`src/engine/hydrology/Gage.cpp:447`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Gage.cpp#L447)) |
| **Since** | 5.1.007, which introduced `[ADJUSTMENTS]` (EPA commit cb7eb068) |
| **Fix** | Use the factor of the month of each record's own date: [`NUM-40_swmm530.patch`](NUM-40_swmm530.patch) |

## The problem

`[ADJUSTMENTS] RAINFALL` gives twelve monthly multipliers for the rainfall of each month, typically to scale a historical record for climate-change scenarios. The engine applies the factor to a rain record when it reads the record, not when the rain falls, and it reads records ahead:

- The first record of the series is read when the run starts, before any month's factor has been set. It is multiplied by 1.0.
- Every later record is read as soon as the record before it becomes current, which may be days or weeks earlier. A storm on 10 February that follows a dry spell starting on 15 January gets January's factor.

In the test deck, the factors are 2.0 for January and 3.0 for February, and there are two 1-hour storms of 1 in each: 31 January 06:00 and 1 February 06:00. The January storm is the first record and falls at 1.0 in/hr instead of 2.0; the February storm is read on 31 January and falls at 2.0 in/hr instead of 3.0. Total Precipitation is 3.000 in instead of 5.000 in, and the continuity table reports no error, because the precipitation total is booked from the same wrong rates.

For a continuous simulation the error is smaller but systematic: it hits the first record of the run and the first record of every month that follows a dry gap across the month boundary. In EPA's regression deck `swc4.inp` (three years of hourly rain, factors 0.91 to 1.34), Total Precipitation is 130.745 in where the factors give 130.652 in.

## Why it happens

`convertRainfall()` folds the current month's factor into the rate it returns:

```c
// src/legacy/engine/gage.c, convertRainfall()
return r1 * Gage[j].unitsFactor * Gage[j].scaleFactor * Adjust.rainFactor;
```

`Adjust.rainFactor` is set only by `climate_setState()`, from the date of the current runoff step:

```c
// src/legacy/engine/climate.c, climate_setState()
Adjust.rainFactor = Adjust.rain[datetime_monthOfYear(theDate) - 1];
```

Its initial value is 1.0 (`project.c:982`). `gage_initState()` runs from `project_init()`, before the first `climate_setState()`, and calls `getFirstRainfall()` and, when the first record starts the run, `getNextRainfall()` ([gage.c:309](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L309)). So those records are multiplied by 1.0. Afterwards, `gage_setState()` reads the next non-zero record as soon as the current one becomes current ([gage.c:415](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L415)):

```c
// src/legacy/engine/gage.c, gage_setState()
Gage[j].rainfall = Gage[j].nextRainfall;
if ( !getNextRainfall(j) ) Gage[j].nextDate = NO_DATE;   // read now, used later
```

The next record's rate is then fixed with the factor of the month of the current record. The RDII calculation calls `gage_setState()` the same way after setting `Adjust.rainFactor` from its own date ([rdii.c:1231](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rdii.c#L1231)), so RDII rainfall has the same error.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-40_two-months.inp`](NUM-40_two-months.inp) | One impervious subcatchment, factors 2.0 (Jan) and 3.0 (Feb), 1 in storms on 31 Jan 06:00 and 1 Feb 06:00 |
| [`NUM-40_test.c`](NUM-40_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0), records the peak rainfall on S1 on each day and reads Total Precipitation from the report |
| [`NUM-40_test6.c`](NUM-40_test6.c) | The same check through the 6.0.0 API |

```sh
tools/run-test.sh NUM-40            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh NUM-40 --patched  # 5.3.0 with the fix, and 6.0.0: PASS
```

**Without the fix** (5.2.4 and 5.3.0 print the same):

```
Storm                 rain on S1 (in/hr)   expected
31 Jan (factor 2.0)        1.000             2.000
 1 Feb (factor 3.0)        2.000             3.000
Total Precipitation (in)        3.000             5.000
FAIL: the storms fell at 1.000 and 2.000 in/hr (total 3.000 in) instead of 2.000 and 3.000 in/hr (5.000 in)
NUM-40 5.2.4 base: FAIL
NUM-40 5.3.0 base: FAIL
```

6.0.0 already gives the right answer:

```
31 Jan (factor 2.0)        2.000             2.000
 1 Feb (factor 3.0)        3.000             3.000
Total Precipitation (in)        5.000             5.000
PASS: each storm is scaled by the factor of the month it falls in (2.000 + 3.000 = 5.000 in)
NUM-40 6.0.0 base: PASS
```

**With the fix**, 5.3.0 prints the same as 6.0.0, and the two reports' runoff continuity tables are identical (5.000 in precipitation, 5.027 in runoff, -0.549 %):

```
31 Jan (factor 2.0)        2.000             2.000
 1 Feb (factor 3.0)        3.000             3.000
Total Precipitation (in)        5.000             5.000
PASS: each storm is scaled by the factor of the month it falls in (2.000 + 3.000 = 5.000 in)
NUM-40 5.3.0 patched: PASS
```

## The fix

Pass the record's own date to `convertRainfall()` and apply the factor of that month. Both readers have the date at hand (`Gage[j].startDate` for the first record, `Gage[j].nextDate` for the others):

```diff
-double convertRainfall(int j, double r)
+double convertRainfall(int j, double r, DateTime aDate)
 ...
-    return r1 * Gage[j].unitsFactor * Gage[j].scaleFactor * Adjust.rainFactor;
+    // --- apply the monthly adjustment of the month the record falls in
+    //     (records are read ahead, before that month's factor is current)
+    return r1 * Gage[j].unitsFactor * Gage[j].scaleFactor *
+           Adjust.rain[datetime_monthOfYear(aDate) - 1];
```

The factor now belongs to the month in which a record starts. 6.0.0 uses the month of the runoff step instead; the two differ only for a record that itself spans a month boundary (for example a daily total recorded at 12:00 on the last day of a month), where 6.0.0 switches factor part-way through the record.

`Adjust.rainFactor` is no longer read after this change. Its assignments in `climate.c`, `rdii.c` and `project.c` are left in place to keep the patch to one file.

**Effect on other models.** Only decks with `[ADJUSTMENTS] RAINFALL` change. Of the regression decks, `swc4.inp` and `swc6.inp` have one (three years of hourly rain from a rain file). The patched 5.3.0 gives the same totals as 6.0.0 and as summing each record times its month's factor directly from the rain file:

| Deck | Total Precipitation, 5.3.0 | patched 5.3.0 | 6.0.0 | sum of record x factor | Surface Runoff, 5.3.0 -> patched |
|---|---|---|---|---|---|
| swc4 | 130.745 in | 130.652 in | 130.652 in | 130.652 in | 40.495 -> 40.461 in |
| swc6 | 87.467 in | 87.468 in | 87.468 in | 87.468 in | 29.912 -> 29.922 in |

Runoff continuity errors are unchanged in swc4 (-0.377 %) and move from -0.896 % to -0.893 % in swc6. Decks without monthly rainfall factors give bit-identical results, since all factors are 1.0.
