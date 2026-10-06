# IO-38: A time-series file without a final line break ends on its second-to-last value

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | After the last date in an external time-series file, series that hold their last value (outfall stage, control-rule `TIMESERIES` actions, air temperature) get the second-to-last value instead if the file's last line has no line break. In the test an outfall stage stays at 100.5 instead of 101.5 ft for the last hour. A related shortcut makes a file series read back in time after its end return 0: an inflow is missing for 13 minutes, External Inflow 0.586 instead of 0.699 acre-ft (−16 %). No warning. |
| **Reached from** | `[TIMESERIES] Name FILE "path"`, for every consumer read through `table_tseriesLookup()` |
| **5.3.0** | `table_tseriesLookup()` in [`src/legacy/engine/table.c:770-775`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/table.c#L770-L775), with `table_getNextFileEntry()` at [`table.c:827`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/table.c#L827) |
| **5.2.4** | Same code, [`src/solver/table.c:765-770`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/table.c#L765-L770) |
| **6.0.0** | Not affected: `load_external_timeseries_files()` in [`src/engine/input/PostParseResolver.cpp:177`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/PostParseResolver.cpp#L177) reads the whole file into memory before the run |
| **Since** | 5.2.2, which added the end-of-file branch (commit [c54b60f6](https://github.com/USEPA/Stormwater-Management-Model/commit/c54b60f60641453f7f43b4b244a837d666f848b8)) |
| **Fix** | Take the end-of-file shortcut only for dates after the last entry, and return that entry's value: [`IO-38_swmm530.patch`](IO-38_swmm530.patch) |

## The problem

A time series can be read from an external file, one `date time value` line per entry. Whether the last line ends with a line break should make no difference, and many editors and scripts do not add one. For series that are extended beyond their last entry, the manual says the last recorded value is used for later dates (Hydrology reference, air temperature). Outfall stage and control-rule actions are read the same way (`extend = TRUE`).

In the test, two outfalls take their stage from two files with the same three lines, 99.5, 100.5 and 101.5 ft at 0:00, 1:00 and 2:00. Only one ends with a line break. From 2:00 to the end of the 3-hour run, the outfall whose file ends with a line break is at 101.5 ft (depth 2.5 ft). The other stays at 100.5 ft (depth 1.5 ft) for all 360 routing steps of that hour.

The same branch has a second effect. Once a file series has reached end-of-file, any earlier date also takes the shortcut instead of rewinding the file. One series is read at earlier dates after later ones when two consumers on different clocks share it (see [NUM-28](../../1-numerical/NUM-28-timeseries-backward-lookup/)). In the test, a file series (ending with a line break) drives both an `EXT` buildup function, read at the end of each 18-minute runoff step, and the inflow at J1, read at the start of each 1-minute routing step. The runoff lookup at 1:30 passes the file's last entry (1:25) and reaches end-of-file. From 1:12 to 1:24 the inflow lookups are earlier than that, and all return 0 instead of values up to 10 cfs.

## Why it happens

`table_getNextFileEntry()` reads the file with `fgets()`. When the last line has no line break, the `fgets()` that returns it also hits the end of the file and sets the end-of-file indicator. The bracket is then [second-to-last entry, last entry], and `feof()` is already true. With a final line break, the last entry is read without hitting the end of the file, and the indicator is set only by the next, failing read, after the bracket has moved to [last entry, last entry].

`table_tseriesLookup()` tests `feof()` before anything else that could move the bracket:

```c
// src/legacy/engine/table.c, table_tseriesLookup()
    // --- x lies within current time bracket
    if ( table->x1 <= x
    &&   table->x2 >= x
    &&   table->x1 != table->x2 )
    return table_interpolate(x, table->x1, table->y1, table->x2, table->y2);

    // --- end of external time series file has been reached
    if ( table->file.mode == USE_FILE && feof(table->file.file) )
    {
        if (extend == TRUE) return table->y1;
        else return 0;
    }

    // --- x lies before current time bracket:
    //     move to start of time series
    if ( table->x1 == table->x2 || x < table->x1 )
    {
        table_getFirstEntry(table, &(table->x1), &(table->y1));
```

- After the last entry, it returns `y1`, the left end of the bracket. With a final line break that is the last entry; without one it is the entry before.
- For a date earlier than the bracket, it returns `y1` (or 0 with `extend = FALSE`, as for inflows) and never reaches the rewind below, which would have reset the file.

5.2.1 had no such branch. Its forward search returned the last value read, with or without a final line break, but every later lookup found the collapsed bracket, rewound the file and read it again from the start. The branch added in 5.2.2 avoids that re-reading.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-38_stage-file.inp`](IO-38_stage-file.inp) | Outfalls O1 and O2, stages from `IO-38_stage-no-newline.dat` and `IO-38_stage-newline.dat`; 1 cfs into each, dynamic wave, 10-second steps, 3 hours |
| [`IO-38_stage-no-newline.dat`](IO-38_stage-no-newline.dat), [`IO-38_stage-newline.dat`](IO-38_stage-newline.dat) | The same three stage entries; only the second ends with a line break |
| [`IO-38_shared-file-series.inp`](IO-38_shared-file-series.inp) | File series FLW drives an `EXT` buildup function (18-minute runoff steps) and the inflow at J1 (1-minute routing steps); no rain |
| [`IO-38_flow.dat`](IO-38_flow.dat) | FLW: 5 cfs from 0:00 to 0:45 (flat, so [NUM-28](../../1-numerical/NUM-28-timeseries-backward-lookup/) cannot change it), then 8, 10 and 0 cfs at 1:00, 1:15 and 1:25; ends with a line break |
| [`IO-38_test.c`](IO-38_test.c) | Legacy toolkit test: checks the outfall depths against the stage at every step, J1's inflow against FLW at every step, and the External Inflow volume against FLW's integral (30,450 ft3) |
| [`IO-38_test6.c`](IO-38_test6.c) | The same checks through the 6.0.0 API |
| [`IO-38_swmm530.patch`](IO-38_swmm530.patch) | The fix for 5.3.0 |

```sh
tools/run-test.sh IO-38            # 5.2.4 and 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh IO-38 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same):

```
A. outfall depths (ft)
  time (h)   O1 (no final newline)   O2 (final newline)   expected
     0.500                  1.0001               1.0001     1.0001
     1.500                  2.0001               2.0001     2.0001
     2.000                  1.5000               2.5000     2.5000
     2.003                  1.5000               2.5000     2.5000
     2.500                  1.5000               2.5000     2.5000
  steps with a wrong depth: O1 360, O2 0 (largest error 1.0000 ft)

B. J1 inflow (cfs)
  t (min)   J1 inflow       FLW
       54      6.8000    6.8000
       71      9.4667    9.4667
       72      0.0000    9.6000
       75      0.0000   10.0000
       80      0.0000    5.0000
       84      0.0000    1.0000
       86      0.0000    0.0000
  steps with a wrong inflow: 13 (largest error 10.0000 cfs at 75 min)
  External Inflow: report 0.586 acre-ft, integral of FLW 0.699 acre-ft
FAIL: wrong values after end-of-file: 360 steps of O1's depth and 0 of O2's (up to 1.00 ft off), 13 steps of J1's inflow (up to 10.00 cfs off), External Inflow 0.586 instead of 0.699 acre-ft
IO-38 5.2.4 base: FAIL
IO-38 5.3.0 base: FAIL
```

**With the fix** (6.0.0 prints the same numbers unpatched):

```
A. outfall depths (ft)
  time (h)   O1 (no final newline)   O2 (final newline)   expected
     0.500                  1.0001               1.0001     1.0001
     1.500                  2.0001               2.0001     2.0001
     2.000                  2.5000               2.5000     2.5000
     2.003                  2.5000               2.5000     2.5000
     2.500                  2.5000               2.5000     2.5000
  steps with a wrong depth: O1 0, O2 0 (largest error 0.0000 ft)

B. J1 inflow (cfs)
  t (min)   J1 inflow       FLW
       54      6.8000    6.8000
       71      9.4667    9.4667
       72      9.6000    9.6000
       75     10.0000   10.0000
       80      5.0000    5.0000
       84      1.0000    1.0000
       86      0.0000    0.0000
  steps with a wrong inflow: 0 (largest error 0.0000 cfs at 81 min)
  External Inflow: report 0.702 acre-ft, integral of FLW 0.699 acre-ft
PASS: file series give the last value after their end, whatever the final line break, and are read correctly back in time after end-of-file
IO-38 5.3.0 patched: PASS
IO-38 6.0.0 base: PASS
```

0.702 acre-ft is the sum over 1-minute steps; it is 0.4 % above the exact integral because the series ends with a steep drop.

## The fix

```diff
     // --- end of external time series file has been reached
-    if ( table->file.mode == USE_FILE && feof(table->file.file) )
+    //     (x2 is the file's last entry; earlier dates rewind below)
+    if ( table->file.mode == USE_FILE && feof(table->file.file)
+    &&   x > table->x2 )
     {
-        if (extend == TRUE) return table->y1;
+        if (extend == TRUE) return table->y2;
         else return 0;
     }
```

When the end-of-file indicator is set, `x2` is always the last entry read: either the read that returned the last line set it, or the bracket was already collapsed onto the last entry before the failed read. Returning `y2` is therefore the last value in both cases. Dates earlier than the bracket now fall through to the existing rewind, which calls `rewind()` on the file and clears the indicator. Dates after the last entry still take the shortcut, so the file is not re-read at every step after its end.

For a file that ends with a line break and is read forward, the result is unchanged, as O2 shows. The patch touches only file-based series, and none of the 73 regression decks reads a time series from a file (their `FILE` entries are rain gage files, which `gage.c` reads), so none of their results change. The patch applies together with [NUM-28](../../1-numerical/NUM-28-timeseries-backward-lookup/)'s, in either order, and needs no change in 6.0.0.
