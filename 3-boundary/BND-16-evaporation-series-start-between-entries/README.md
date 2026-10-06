# BND-16: An evaporation time series uses the next entry's rate when the run starts between two entries

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | When `[EVAPORATION] TIMESERIES` is used and the simulation starts after one entry and before the next, the run uses the later entry's rate from the start date until that entry's date. In the test (monthly series, run starting Jan 15) the rate is 0.20 in/day instead of 0.10 in/day for 17 days and the evaporation loss is 5.198 in instead of 3.500 in. There is no warning. |
| **Reached from** | `[EVAPORATION] TIMESERIES` whose first entry is before `START_DATE`/`START_TIME` and with no entry exactly at the start. This is the usual case when a long daily or monthly potential evaporation record is reused for a shorter run. |
| **5.3.0** | `climate_initState()` in [`src/legacy/engine/climate.c:985`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/climate.c#L985) |
| **5.2.4** | Same code, [`src/solver/climate.c:620`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/climate.c#L620) |
| **6.0.0** | Reproduces: `SWMMEngine::initHydrology()` copies the legacy start-up in [`src/engine/core/SWMMEngine.cpp:8869`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L8869) |
| **Since** | 5.1.008, which made time-series evaporation a step function that can change within a day |
| **Fix** | Start on the last entry at or before the start date: [`BND-16_swmm530.patch`](BND-16_swmm530.patch), [`BND-16_swmm600.patch`](BND-16_swmm600.patch) |

## The problem

With `[EVAPORATION] TIMESERIES`, SWMM treats the series as a step function: each value applies from its own date until the next entry's date. Values are not interpolated. That holds during the run, but not at the start. If the start date falls between two entries, SWMM begins with the later entry's rate and keeps it until that entry's date is reached.

The test deck has an evaporation series with one value per month:

| Date | Rate (in/day) |
|---|---|
| Dec 1, 2019 | 0.05 |
| Jan 1, 2020 | 0.10 |
| Feb 1, 2020 | 0.20 |
| Mar 1, 2020 | 0.30 |

The run goes from Jan 15 to Feb 10, 2020. 6 in of rain in the first hour fill the 6 in of depression storage on a 10-acre impervious subcatchment, so water is always available to evaporate. The rate should be 0.10 in/day until Feb 1 and 0.20 in/day after it, for an evaporation loss of 17 x 0.10 + 9 x 0.20 = 3.50 in. All three engines use 0.20 in/day from Jan 15, and the loss is 5.198 in, 49% too high:

```
  Runoff Quantity Continuity     acre-feet        inches
  **************************     ---------       -------
  Total Precipitation ......         5.000         6.000
  Evaporation Loss .........         4.332         5.198
  Infiltration Loss ........         0.000         0.000
  Surface Runoff ...........         0.000         0.000
  Final Storage ............         0.668         0.802
```

Storage nodes and conduit evaporation use the same rate, so they are affected in the same way. A series with daily values that starts mid-day is affected for the rest of that day. With a monthly series and a mid-month start it is wrong for up to a month.

## Why it happens

`climate_initState()` reads the first entry of the series. If it is before the start date, it calls `setNextEvapDate(StartDate)`, which scans forward to the first entry on or after the start date and stores that entry's date and rate in `NextEvapDate` and `NextEvapRate`. That rate becomes the starting rate:

```c
// src/legacy/engine/climate.c, climate_initState()
        // --- initialize NextEvapDate & NextEvapRate to first entry of
        //     time series whose date <= the simulation start date
        table_getFirstEntry(&Tseries[Evap.tSeries],
                            &NextEvapDate, &NextEvapRate);
        if (NextEvapDate < StartDate)
        {
            setNextEvapDate(StartDate);
        }
        Evap.rate = NextEvapRate / UCF(EVAPRATE);

        // --- find the next time evaporation rates change after this
        setNextEvapDate(NextEvapDate);
```

The comment describes the intended entry, the one "whose date <= the simulation start date", but `setNextEvapDate()` returns the first entry whose date is `>=` the start date. During the run, `setEvap()` switches to `NextEvapRate` only when the date reaches `NextEvapDate`:

```c
// src/legacy/engine/climate.c, setEvap()
    case TIMESERIES_EVAP:
        if (theDate >= NextEvapDate)
            Evap.rate = NextEvapRate / UCF(EVAPRATE);
        break;
```

So every entry after the first is applied from its own date onward, except the one after the start date, which is applied early. If no entry falls between the start and end dates, `NextEvapRate` keeps the first entry's rate and the run uses that instead.

6.0.0 copies this start-up in `SWMMEngine::initHydrology()` (`if (cs.next_evap_date < start_date) setNextEvapDate(start_date);`), with the same result.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-16_mid-series-start.inp`](BND-16_mid-series-start.inp) | Monthly evaporation series (Dec 0.05, Jan 0.10, Feb 0.20, Mar 0.30 in/day), run from Jan 15 to Feb 10, 10-acre impervious subcatchment with 6 in of rain held in 6 in of depression storage |
| [`BND-16_test.c`](BND-16_test.c) | Runs the deck through the legacy toolkit. Records the subcatchment evaporation rate (`swmm_getValue(swmm_SUBCATCH_EVAP)`) before and after Feb 1 and reads the evaporation loss from the report. Expects 0.10 in/day before Feb 1, 0.20 in/day after it and a loss of 3.50 in (within 0.05 in). |
| [`BND-16_test6.c`](BND-16_test6.c) | The same check for 6.0.0 with `swmm_subcatch_get_evap()` and `swmm_get_runoff_total()` |

```sh
tools/run-test.sh BND-16            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-16 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, all three engines print:

```
Series: Dec 1 0.05, Jan 1 0.10, Feb 1 0.20, Mar 1 0.30 in/day; run Jan 15 - Feb 10
                                       observed        expected
Rate Jan 15 2:00 - Feb 1 (min, max)  0.2000 0.2000   0.1000 in/day
Rate Feb 1 - Feb 10 (min, max)       0.2000 0.2000   0.2000 in/day
Runoff balance: evaporation             5.198        3.500 in
Runoff balance: final storage           0.802        2.500 in
FAIL: from Jan 15 to Feb 1 the series' Feb 1 rate is used instead of the Jan 1 rate: 0.2000 in/day instead of 0.1000, evaporation loss 5.198 in instead of 3.500 in
BND-16 5.3.0 base: FAIL
```

**With the fix**, 5.3.0 and 6.0.0 print the same:

```
Series: Dec 1 0.05, Jan 1 0.10, Feb 1 0.20, Mar 1 0.30 in/day; run Jan 15 - Feb 10
                                       observed        expected
Rate Jan 15 2:00 - Feb 1 (min, max)  0.1000 0.1000   0.1000 in/day
Rate Feb 1 - Feb 10 (min, max)       0.2000 0.2000   0.2000 in/day
Runoff balance: evaporation             3.499        3.500 in
Runoff balance: final storage           2.501        2.500 in
PASS: the rate in effect at the start is the Jan 1 entry's 0.10 in/day and the Feb 1 entry applies from Feb 1; evaporation loss 3.499 in
BND-16 5.3.0 patched: PASS
BND-16 6.0.0 patched: PASS
```

The 0.001 in short of 3.500 is the first 15-minute step, which starts with a dry surface.

## The fix

5.3.0, in `climate_initState()`: count the entries after the first one that are on or before the start date, rewind the series and step forward to the last of them. That entry's rate is the starting rate, and the existing `setNextEvapDate(NextEvapDate)` then finds the next entry. Rewinding with `table_getFirstEntry()` and stepping with `table_getNextEntry()` works for series given in the input file and for series read from an external file.

```diff
         if (NextEvapDate < StartDate)
         {
-            setNextEvapDate(StartDate);
+            // --- move to the last entry on or before StartDate: its rate
+            //     is in effect until the next entry's date
+            n = 0;
+            while (table_getNextEntry(&Tseries[Evap.tSeries], &d, &e) &&
+                   d <= StartDate) n++;
+            table_getFirstEntry(&Tseries[Evap.tSeries],
+                                &NextEvapDate, &NextEvapRate);
+            for (i = 0; i < n; i++)
+                table_getNextEntry(&Tseries[Evap.tSeries],
+                                   &NextEvapDate, &NextEvapRate);
         }
         Evap.rate = NextEvapRate / UCF(EVAPRATE);
```

6.0.0 makes the same change in `SWMMEngine::initHydrology()`, where the series is an array:

```diff
-                if (cs.next_evap_date < ctx_.options.start_date)
-                    setNextEvapDate(ctx_.options.start_date);
+                if (cs.next_evap_date < ctx_.options.start_date) {
+                    while (cs.evap_ts_pos + 1 < static_cast<int>(tbl.x.size()) &&
+                           tbl.x[static_cast<std::size_t>(cs.evap_ts_pos + 1)] <= ctx_.options.start_date)
+                        ++cs.evap_ts_pos;
+                    cs.next_evap_date = tbl.x[static_cast<std::size_t>(cs.evap_ts_pos)];
+                    cs.next_evap_rate = tbl.y[static_cast<std::size_t>(cs.evap_ts_pos)];
+                }
```

Only runs that start strictly between two entries change. Effect on other models: none of the regression decks uses time-series evaporation. On variants of the test deck run with both patched CLIs, a start on an entry (Jan 1) and a start before the first entry (Nov 20) give the same results as before (evaporation loss 3.499 in, final storage 2.501 in). With an extra entry of 0.15 in/day at Jan 15 06:00 and a start at Jan 15 00:00, the loss goes from 4.348 in to 4.336 in, which matches 6 h at 0.10, 16.75 days at 0.15 and 9 days at 0.20 in/day.
