# IO-03: Negative and NaN rainfall values in a time series or rain file are used as rain

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A rain record of -1 (a common missing-data flag) or -2.0 is accepted and applied as negative rainfall: Total Precipitation becomes -1.000 in instead of 1.000 in and the runoff continuity error -206 %. A `nan` in a rain file is read as a number and makes the precipitation total and the final storage `nan`; runoff is 0.192 in instead of 0.482 in. No error or warning is written. |
| **Reached from** | A rain gage `TIMESERIES` with a negative or `nan` value; a user-prepared rain file (`FILE`, standard format) with such a value |
| **5.3.0** | `getNextRainfall()` and `convertRainfall()` in [`src/legacy/engine/gage.c:674`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L674); `readStdLine()` in [`src/legacy/engine/rain.c:994`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rain.c#L994) |
| **5.2.4** | Same code, [`src/solver/gage.c:667`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gage.c#L667) and [`src/solver/rain.c:994`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/rain.c#L994) |
| **6.0.0** | Reproduces: `recordRate()` in [`src/engine/hydrology/Gage.cpp:216`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Gage.cpp#L216) and the rain file reader in [`src/engine/input/PostParseResolver.cpp:986`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/PostParseResolver.cpp#L986) accept any value; the continuity table even prints 0.000 % for the negative case |
| **Since** | Every release (the code is in the initial commit of EPA's repository, 5.0.022) |
| **Fix** | A negative or NaN record carries no rain; in a rain file it is counted as a missing period: [`IO-03_swmm530.patch`](IO-03_swmm530.patch), [`IO-03_swmm600.patch`](IO-03_swmm600.patch) |

## The problem

Precipitation cannot be negative, and the input reference describes a user-prepared rain file as holding "non-zero precipitation" readings. SWMM does not check either source. Two cases come up in practice:

- **Missing-data flags.** Exported gauge records often mark missing periods with -1 or -9999. SWMM recognises the missing-value codes of the NWS and Environment Canada formats (and counts them in the Rainfall File Summary), but a -1 in a time series or a user-prepared file is taken as rain of -1 in/hr.
- **`nan`.** Both the time series parser (`strtod`) and the rain file parser (`sscanf "%f"`) accept the text `nan`, so a gap written as `nan` by a spreadsheet or a script is read as a number.

In the test, one subcatchment (10 ac, 50 % impervious) gets 0.5 in/hr from 0:00 to 1:00 and from 2:00 (or 3:00) to the next hour; the valid records total 1.000 in. With a -2.0 in/hr record in between (time series) or two -1 records (rain file), the runoff continuity table reads:

```
  Total Precipitation ......        -0.833        -1.000
  Evaporation Loss .........         0.000         0.000
  Infiltration Loss ........         0.417         0.500
  Surface Runoff ...........         0.353         0.423
  Final Storage ............         0.017         0.020
  Continuity Error (%) .....      -206.013
```

The precipitation total includes the negative record, but the water balance of the subcatchment cannot follow it: the negative intensity empties the ponded depth and the rest is discarded. With a `nan` record, the ponded depths become NaN: infiltration and runoff come to 0.250 and 0.192 in instead of 0.500 and 0.482 in, and the report shows `nan` for Total Precipitation and Final Storage while the continuity error prints as 0.000 %. A user who does not read the continuity table gets no sign that anything is wrong.

## Why it happens

The gage reader skips exact zeros and nothing else:

```c
// src/legacy/engine/gage.c, getNextRainfall()
        else
        {
            k = Gage[j].tSeries;
            if ( k >= 0 )
            {
                if ( !table_getNextEntry(&Tseries[k],
                        &Gage[j].nextDate, &rNext) ) return 0;
                rNext = convertRainfall(j, rNext);
            }
            else return 0;
        }
    } while (rNext == 0.0);
```

`convertRainfall()` scales the value without looking at its sign. A user-prepared rain file goes through the same path: `rain.c` copies it into the binary rain interface file, and `readStdLine()` parses the value with `sscanf(line, "%s %d %d %d %d %d %f", ...)` ([rain.c:1070](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rain.c#L1070)) and saves whatever it read. Only the NWS and Canadian readers know missing-value codes.

The negative intensity then reaches the subarea routing, which clips the result instead of booking it ([subcatch.c:1125](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L1125)):

```c
// src/legacy/engine/subcatch.c, getSubareaRunoff()
    // --- do not allow ponded depth to go negative
    if ( subarea->depth < 0.0 ) subarea->depth = 0.0;
```

The rainfall total in the continuity table is accumulated from the gage rate, so it keeps the full negative amount. For a cumulative-type rain file a bad value also corrupts the next record, since `readStdLine()` takes the increment from the last value it read.

6.0.0 keeps the same rule: `nextNonzeroRecord()` skips records whose rate is exactly zero, and `recordRate()` and the rain file reader convert any value.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-03_series-negative.inp`](IO-03_series-negative.inp) | Gage on a time series 0.5, -2.0, 0.5, 0.0 in/hr (hourly), 6-hour run |
| [`IO-03_file-negative.inp`](IO-03_file-negative.inp) + [`IO-03_flagged.dat`](IO-03_flagged.dat) | Gage on a user-prepared rain file 0.5, -1, -1, 0.5 in/hr |
| [`IO-03_file-nan.inp`](IO-03_file-nan.inp) + [`IO-03_nan.dat`](IO-03_nan.dat) | Gage on a user-prepared rain file 0.5, nan, 0.5 in/hr |
| [`IO-03_test.c`](IO-03_test.c) | Runs the three decks through the legacy toolkit (5.2.4 and 5.3.0), tracks the smallest rainfall and any non-finite rainfall or runoff on S1 at each step, and reads Total Precipitation and the runoff continuity error from each report |
| [`IO-03_test6.c`](IO-03_test6.c) | The same check through the 6.0.0 API |

The test expects no negative or non-finite rainfall, 1.000 in of precipitation (the valid records) and a continuity error within 1 %. Rejecting the deck with an input error would also pass.

```sh
tools/run-test.sh IO-03            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-03 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 5.2.4 prints the same except continuity errors of -206.004 % and -205.998 %):

```
Deck              min rain   non-finite   Total Precip   Continuity
                  (in/hr)    values       (in)           Error (%)
series-negative     -2.000        0           -1.000       -206.013
file-negative       -1.000        0           -1.000       -206.006
file-nan             0.000       60              nan          0.000
expected             >= 0        0            1.000     within +-1
FAIL: invalid rainfall values are used as rain (series-negative: min rain -2.000 in/hr, 0 non-finite, precipitation -1.000 in, continuity -206.013 %; ...)
IO-03 5.2.4 base: FAIL
IO-03 5.3.0 base: FAIL
```

6.0.0 applies the same rainfall but prints a continuity error of 0.000 %:

```
series-negative     -2.000        0           -1.000          0.000
file-negative       -1.000        0           -1.000          0.000
file-nan             0.000       60              nan          0.000
IO-03 6.0.0 base: FAIL
```

**With the fix**, both engines print the same:

```
Deck              min rain   non-finite   Total Precip   Continuity
                  (in/hr)    values       (in)           Error (%)
series-negative      0.000        0            1.000         -0.215
file-negative        0.000        0            1.000         -0.230
file-nan             0.000        0            1.000         -0.215
expected             >= 0        0            1.000     within +-1
PASS: negative and NaN records carry no rain (1.000 in from the valid records, continuity closes)
IO-03 5.3.0 patched: PASS
IO-03 6.0.0 patched: PASS
```

The patched 5.3.0 report lists the flagged periods in the Rainfall File Summary (`file-negative`: 2 periods with precipitation, 2 missing; `file-nan`: 2 and 1). 6.0.0 has no missing-period count and prints 0 there.

## The fix

Treat a negative or NaN record as no rain, the way SWMM already treats the missing-value codes of the formats it knows. In the gage reader:

```diff
 {
     double r1;
+
+    // --- a negative or NaN record is not rainfall: it carries no rain
+    //     (getNextRainfall then skips it like an explicit zero)
+    if ( !(r >= 0.0) ) return 0.0;
     switch ( Gage[j].rainType )
```

and in the user-prepared rain file reader, so that the period is reported as missing and a cumulative gage keeps its last valid reading:

```diff
     PreviousDate = date2;
 
+    // --- a negative or NaN value is not a rainfall depth (e.g. a missing
+    //     data flag): count the period as missing instead of saving it
+    if ( !(x >= 0.0f) )
+    {
+        saveRainfall(date1, hour, minute, x, TRUE);
+        return 1;
+    }
+
     switch (RainType)
```

The comparisons are written as `!(x >= 0)` so that NaN takes the same branch as a negative value. The 6.0.0 patch makes the same two changes in `recordRate()` (where a cumulative gage also takes its increment from the last valid record) and in the user-prepared file reader of `PostParseResolver.cpp`.

The run still gives no warning for such values; a user who wants to know how many records were dropped from a rain file finds them in the Rainfall File Summary.

**Effect on other models.** Only records with a negative or NaN value change. None of the rain gage time series or rain files in the regression decks has one, so their results are unchanged.
