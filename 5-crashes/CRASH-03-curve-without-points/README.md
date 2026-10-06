# CRASH-03: A curve or time series without data points crashes the input check

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Null pointer dereference while the input is checked: the run stops with a segmentation fault and an empty report. The curve or series need not be used by any object. 6.0.0 does not crash but accepts the model without a message and runs a storage node on an empty storage curve, or an inflow on an empty series. |
| **Reached from** | A `[CURVES]` line with only a name and type and no points after it (`CV1 Storage`), or a `[TIMESERIES]` whose only line stops after the time (`TS1 01/01/2020 00:00`) |
| **5.3.0** | `table_validate()` in [`src/legacy/engine/table.c:315`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/table.c#L315) calls `table_getNextEntry()`, which dereferences a NULL `thisEntry` at [`table.c:387`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/table.c#L387) |
| **5.2.4** | Same code, [`src/solver/table.c:310`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/table.c#L310) and [`table.c:382`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/table.c#L382) |
| **6.0.0** | No crash, but no error either: the table check in `resolve_cross_references()` ([`src/engine/input/PostParseResolver.cpp:3532`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/PostParseResolver.cpp#L3532)) only tests the order of the points |
| **Since** | Curves: 5.2.0 (commit [930e74ed](https://github.com/USEPA/Stormwater-Management-Model/commit/930e74edcdadfb9f50d52cba833cc9165d8ee6b5)), which let a curve's first line hold only its name and type; 5.1.015 rejected such a line with ERROR 203. Time series: older; 5.1.015 has the same parser and the same `table_validate()` (not run here) |
| **Fix** | Reject a table without data: [`CRASH-03_swmm530.patch`](CRASH-03_swmm530.patch), [`CRASH-03_swmm600.patch`](CRASH-03_swmm600.patch) |

## The problem

Since 5.2.0 the first line of a curve may hold only its name and type, with the points on the lines that follow:

```c
// src/legacy/engine/table.c, table_readCurve()
        Curve[j].curveType = m;
        if (ntoks == 2) return 0;
```

If no points follow, the curve exists with no entries. This happens when a user declares a curve and has not entered its points yet, or when a script or GUI writes an empty curve. `project_validate()` checks every curve and time series with `table_validate()` before anything else, and that function crashes on a table with no entries. The run stops while the input is read, with nothing in the report.

A time series can end up empty the same way. `table_readTimeseries()` reads date, time and value tokens in turn and stops at the end of the line. When the line ends after the time, it returns without an error and without adding an entry ([`table.c:158`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/table.c#L158)). If that is the series' only line, the series is empty and `table_validate()` crashes on it too.

## Why it happens

```c
// src/legacy/engine/table.c, table_validate()
    // --- retrieve the first data entry in the table
    result = table_getFirstEntry(table, &x1, &y1);

    // --- return error condition if external file has no valid data
    if ( !result && table->file.mode == USE_FILE )
        return ERR_TABLE_FILE_READ;

    // --- retrieve successive table entries and check for non-increasing x-values
    while ( table_getNextEntry(table, &x2, &y2) )
```

For a table without entries, `table_getFirstEntry()` returns FALSE and does not set `table->thisEntry`, which `table_init()` left NULL. The FALSE result is only acted on for file-based tables. `table_getNextEntry()` then reads `table->thisEntry->next`:

```c
// src/legacy/engine/table.c, table_getNextEntry()
    entry = table->thisEntry->next;
```

which is a read at address 0x10 (the offset of `next`). The lookup functions (`table_lookup()`, `table_getStorageVolume()`, ...) check for an empty table; this loop does not. (The bank-area scan in `exfil.c:109-111` has the same unchecked pattern, but it runs only after validation, so the fix here also keeps it away from an empty curve.)

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-03_empty-curve.inp`](CRASH-03_empty-curve.inp) | Storage node SU1 with the tabular curve CV1, declared as `CV1 Storage` with no points |
| [`CRASH-03_empty-timeseries.inp`](CRASH-03_empty-timeseries.inp) | Inflow at J1 from TS1, whose only line is `TS1 01/01/2020 00:00` |
| [`CRASH-03_test.c`](CRASH-03_test.c) | Opens both decks with `swmm_open()` and expects ERROR 171 (curve) and ERROR 173 (time series) |
| [`CRASH-03_test6.c`](CRASH-03_test6.c) | Opens both decks with the 6.0.0 API, runs them if they are accepted, and expects the same errors |
| [`CRASH-03_swmm530.patch`](CRASH-03_swmm530.patch), [`CRASH-03_swmm600.patch`](CRASH-03_swmm600.patch) | The fixes |

```sh
tools/run-test.sh CRASH-03            # 5.2.4 and 5.3.0: CRASH, 6.0.0: FAIL
tools/run-test.sh CRASH-03 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.3.0 stops on the first deck (5.2.4 the same, at `table.c:382`):

```
../src/src/legacy/engine/table.c:387:31: runtime error: member access within null pointer of type 'TTableEntry' (aka 'struct TableEntry')
    #0 ... in table_getNextEntry .../src/legacy/engine/table.c:387:31
    #1 ... in table_validate .../src/legacy/engine/table.c:315:13
    #2 ... in project_validate .../src/legacy/engine/project.c:208:16
    #3 ... in swmm_open .../src/legacy/engine/swmm5.c:671:9
CRASH-03 5.2.4 base: CRASH
CRASH-03 5.3.0 base: CRASH
```

The time-series deck stops at the same place when run on its own (`openswmm-legacy CRASH-03_empty-timeseries.inp ...`). Without the sanitizers, the review's AddressSanitizer-only build of 5.3.0 reports `SEGV on unknown address 0x000000000010` in `table_getNextEntry`.

6.0.0 accepts and runs both:

```
CRASH-03_empty-curve.inp:
    swmm_engine_open returned 0
    the run returned 0
CRASH-03_empty-timeseries.inp:
    swmm_engine_open returned 0
    the run returned 0
FAIL: a table without data is not rejected (curve accepted, time series accepted)
CRASH-03 6.0.0 base: FAIL
```

**With the fix:**

```
CRASH-03_empty-curve.inp:
    swmm_open returned 171
    report: ERROR 171: Curve CV1 has invalid or out of sequence data.
CRASH-03_empty-timeseries.inp:
    swmm_open returned 173
    report: ERROR 173: Time Series TS1 has its data out of sequence.
PASS: the curve and the time series without data are rejected with ERROR 171 and 173, without a crash
CRASH-03 5.3.0 patched: PASS
```

```
CRASH-03_empty-curve.inp:
    swmm_engine_open returned 5
    report: ERROR 171: Curve CV1 has invalid or out of sequence data.
CRASH-03_empty-timeseries.inp:
    swmm_engine_open returned 5
    report: ERROR 173: Time Series TS1 has its data out of sequence.
PASS: the curve and the time series without data are rejected with ERROR 171 and 173
CRASH-03 6.0.0 patched: PASS
```

## The fix

5.3.0: return an error from `table_validate()` when an in-memory table has no first entry.

```diff
     if ( !result && table->file.mode == USE_FILE )
         return ERR_TABLE_FILE_READ;
 
+    // --- return error condition if table has no data at all
+    //     (project_validate reports ERROR 171 for a curve, 173 for a series)
+    if ( !result ) return ERR_TIMESERIES_SEQUENCE;
+
```

`project_validate()` writes ERROR 171 ("Curve %s has invalid or out of sequence data") for any error from a curve and passes a time series' code to `report_writeTseriesErrorMsg()`, which writes ERROR 173 for `ERR_TIMESERIES_SEQUENCE` (it adds a date only for `ERR_CURVE_SEQUENCE`, and an empty series has none). No existing message says "no data"; 171 covers it for curves, and 173 is the nearest for a series. A new code would be clearer if the maintainers prefer one.

6.0.0: the table check that mirrors legacy `table_validate()` gets the same test for curves and inline time series (file-based series are loaded separately):

```diff
     for (const auto& tbl : ctx.tables.tables) {
         const bool is_ts = (tbl.type == TableType::TIMESERIES);
+        // A curve or inline time series with no data at all is rejected
+        // like legacy table_validate does (ERROR 171 / 173).
+        if (tbl.x.empty() && tbl.file_path.empty()) {
+            ctx.errors.push_back(format_error(
+                is_ts ? ERR_TIMESERIES_SEQUENCE : ERR_CURVE_SEQUENCE, tbl.id));
+            continue;
+        }
```

**Effect on other models.** Only tables without any data are affected, which legacy could not open before. All 73 regression decks give the same `swmm_engine_open()` result with and without the 6.0.0 patch (72 open; `ncdc_format.inp` fails in both, for an unrelated reason).

## Notes

- The time-series trigger comes from a separate parser leniency: `table_readTimeseries()` silently drops a date or time at the end of a line that has no value after it. With other entries in the series, the line's incomplete entry is lost without a message rather than causing a crash. That is not fixed here.
