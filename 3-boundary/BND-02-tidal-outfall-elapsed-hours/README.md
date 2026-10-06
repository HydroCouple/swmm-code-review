# BND-02: A TIDAL outfall reads its curve at the hours since the run started, not at the hour of the day

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | The tide is shifted by the simulation's start time of day. In the test, a run starting at 06:00 gets low tide (0 ft) at 06:00 and the 4 ft high tide at 12:00, the reverse of the curve. Peak tailwater lands at the wrong time against the storm. Nothing warns the user. Runs that start at midnight are not affected. |
| **Reached from** | Any TIDAL outfall in a run whose START_TIME is not 00:00:00 |
| **5.3.0** | `outfall_setOutletDepth()` in [`src/legacy/engine/node.c:1433`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L1433) |
| **5.2.4** | Same code, [`src/solver/node.c:1449`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L1449) |
| **6.0.0** | Reproduces: `setAllOutfallDepths()` ports the same lookup in [`src/engine/hydraulics/Outfall.cpp:338`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Outfall.cpp#L338) |
| **Since** | The first commit of the repository (2014), so every release |
| **Fix** | Add the start time of day before taking the hour: [`BND-02_swmm530.patch`](BND-02_swmm530.patch), [`BND-02_swmm600.patch`](BND-02_swmm600.patch) |

## The problem

The input reference defines a tidal curve as "tidal height (i.e., outfall stage) versus hour of day over a complete tidal cycle" ([OUTFALLS] `Tcurve`; [CURVES] type `TIDAL`: "water surface elevation ... versus hour of the day"). SWMM instead looks the curve up at the number of hours elapsed since the start of the run, modulo 24. The two agree only when the run starts at midnight.

The test deck starts at 06:00 and has a gated TIDAL outfall with high tide 4 ft at 06:00, low tide at 12:00, 2 ft at 18:00 and low tide at midnight. Every engine gives the outfall 0 ft at 06:00, 4 ft at 12:00 and 0 ft at 18:00: the tide is six hours late, which for this curve swaps high and low water. The shift is the start time of day, so it can be anything up to 23 h 59 min.

## Why it happens

```c
// src/legacy/engine/node.c, outfall_setOutletDepth()
case TIDAL_OUTFALL:
    k = Outfall[i].tideCurve;
    table_getFirstEntry(&Curve[k], &x, &y);
    currentDate = NewRoutingTime / MSECperDAY;            // days since the START of the run
    x += ( currentDate - floor(currentDate) ) * 24.0;
    stage = table_lookup(&Curve[k], x) / UCF(LENGTH);
    break;

case TIMESERIES_OUTFALL:
    k = Outfall[i].stageSeries;
    currentDate = StartDateTime + NewRoutingTime / MSECperDAY;   // calendar date and time
```

`NewRoutingTime` is the time in milliseconds since the simulation started, so `currentDate` here is an elapsed time, not a date. The TIMESERIES case a few lines below adds `StartDateTime` to get the calendar time; the TIDAL case does not. 6.0.0 reproduces the legacy expression with `elapsed_days`.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-02_tide-from-0600.inp`](BND-02_tide-from-0600.inp) | A 24 h run from 06:00: junction J1 -> conduit -> gated TIDAL outfall O1 (invert 0) with curve 0 h: 0, 6 h: 4, 12 h: 0, 18 h: 2, 24 h: 0 ft. Nothing flows, so the outfall stage is the curve value. |
| [`BND-02_test.c`](BND-02_test.c) | Runs the deck through the legacy toolkit (5.2.4, 5.3.0) and compares the outfall head at every routing step with the curve at the clock time |
| [`BND-02_test6.c`](BND-02_test6.c) | The same through the 6.0.0 C API |

The tolerance is 0.01 ft; the curve is linear between points, so the expected value is exact.

```sh
tools/run-test.sh BND-02            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-02 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print (the first step of every third hour, and 07:00):

```
Clock   Expected   O1 head
        (ft)       (ft)
06:00    4.000     0.000
07:00    3.333     0.667
09:00    2.000     2.000
12:00    0.000     4.000
15:00    1.000     2.000
18:00    2.000     0.000
21:00    1.000     1.000
00:00    0.000     2.000
03:00    2.000     1.000
FAIL: the tide is out of phase with the clock: at 12:00 the outfall stage is 4.000 ft, the curve gives 0.000 ft
BND-02 5.2.4 base: FAIL
BND-02 5.3.0 base: FAIL
```

6.0.0 prints the same rows plus its final step at 06:00 the next day (0.000 ft for 4.000 ft), which it reports as the worst point:

```
FAIL: the tide is out of phase with the clock: at 06:00 the outfall stage is 0.000 ft, the curve gives 4.000 ft
BND-02 6.0.0 base: FAIL
```

**With the fix**, both engines follow the curve:

```
06:00    4.000     4.000
07:00    3.333     3.333
09:00    2.000     2.000
12:00    0.000     0.000
...
PASS: the outfall stage follows the tidal curve by hour of the day (largest difference 0.0000 ft)
BND-02 5.3.0 patched: PASS
BND-02 6.0.0 patched: PASS
```

## The fix

Add the start time of day to the elapsed time before taking the hour:

```diff
         table_getFirstEntry(&Curve[k], &x, &y);
-        currentDate = NewRoutingTime / MSECperDAY;
+
+        // --- the curve gives stage by hour of the day, so add the time of
+        //     day at which the simulation starts
+        currentDate = NewRoutingTime / MSECperDAY +
+                      ( StartDateTime - floor(StartDateTime) );
         x += ( currentDate - floor(currentDate) ) * 24.0;
```

Only the time-of-day part of `StartDateTime` is added, so a run that starts at 00:00:00 adds exactly 0 and its results are bit-identical to before. The 6.0.0 patch makes the same change with `ctx.options.start_date`.

The lookup still adds the curve's first hour (`table_getFirstEntry`), so a curve whose first point is not at hour 0 is read shifted by that first hour, as before. The patch leaves this alone: the manual does not say whether the first point is meant to mark midnight.

**Effect on other models.** None of the regression decks has a TIDAL outfall, so none changes.
