# NUM-28: A time series looked up back in time interpolates from a stale bracket

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | When one series is read at times that go backwards, a lookup that lands in the series' first interval or before its first entry interpolates from the wrong pair of points. In the test the inflows are off by up to 10 cfs for 17 one-minute routing steps and the External Inflow volume is 1.948 instead of 1.756 acre-ft (+11 %). No warning. |
| **Reached from** | A time series read by two consumers on different clocks, e.g. an `EXT` buildup function (read at the end of each runoff step) and an `[INFLOWS]` entry (read at the start of each routing step) |
| **5.3.0** | `table_tseriesLookup()` in [`src/legacy/engine/table.c:779-792`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/table.c#L779-L792) |
| **5.2.4** | Same code, [`src/solver/table.c:774-787`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/table.c#L774-L787) |
| **6.0.0** | Not affected: `table_tseries_lookup_cursor()` in [`src/engine/data/TableData.hpp:290`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/data/TableData.hpp#L290) seeks forwards and backwards from an index ([`TableData.hpp:219`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/data/TableData.hpp#L219)) and gives the correct values |
| **Since** | Every release in the repository's history (unchanged since its initial commit, 2014) |
| **Fix** | Collapse the bracket onto the first entry when rewinding: [`NUM-28_swmm530.patch`](NUM-28_swmm530.patch) |

## The problem

`table_tseriesLookup()` keeps a bracket `[x1, x2]` per time series and moves it forwards as the simulation clock advances. A series has one bracket and one entry cursor, shared by everything that reads it. When two consumers read the same series on different clocks, lookups go back and forth in time. The function handles a backward lookup by rewinding to the first entry, but the rewind is incomplete, and lookups that land between the first two entries, or before the first entry, are interpolated from the wrong points.

The usual way to get there is an `EXT` buildup function and a node inflow that use the same series. The buildup rate is read at the end of each runoff step ([`landuse.c:723`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/landuse.c#L723), `getDateTime(NewRunoffTime)`), which runs ahead of the routing clock, while the inflow is read at the start of each routing step ([`routing.c:230`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L230)). So the first routing lookup after each runoff step goes back by up to a runoff step.

In the test, two series are each read by an `EXT` buildup function (18-minute runoff steps) and by a FLOW inflow (1-minute routing steps), with no rain:

- `TS1` starts at 0:00. From 0:01 to 0:14, J1 receives a constant 10 cfs (the value of TS1's second entry) instead of the ramp from 0.67 to 9.33 cfs.
- `TS2` starts at 0:05. From 0:06 to 0:17, J2 receives values on a straight line from TS2's first entry (0:05, 0 cfs) to its entry at 0:20 (30 cfs), skipping the entries at 0:10 and 0:15: 18 cfs instead of 9 cfs at 0:14.

The routing continuity table reports 1.948 acre-ft of External Inflow; the integral of the two series is 76,500 ft3 = 1.756 acre-ft.

Only lookups that end up in the series' first interval or before its first entry are wrong. A backward lookup that lands further into the series rewinds and searches forward correctly. Reading a series at monotonic times, which is what a single consumer does, is not affected.

## Why it happens

```c
// src/legacy/engine/table.c, table_tseriesLookup()
    // --- x lies before current time bracket:
    //     move to start of time series
    if ( table->x1 == table->x2 || x < table->x1 )
    {
        table_getFirstEntry(table, &(table->x1), &(table->y1));
        if ( x < table->x1 )
        {
            if ( extend == TRUE ) return table->y1;
            else return 0;
        }
    }

    // --- x lies beyond current time bracket:
    //     update start of next time bracket
    table->x1 = table->x2;
    table->y1 = table->y2;

    // --- get end of next time bracket
    while ( table_getNextEntry(table, &(table->x2), &(table->y2)) )
    {
        // --- x lies within the bracket
        if ( x <= table->x2 )
            return table_interpolate(x, table->x1, table->y1,
                                        table->x2, table->y2);
```

The rewind sets `x1`/`y1` to the first entry and the cursor to it, but leaves `x2`/`y2` at the old bracket's right end. Two things then go wrong:

1. **Lookup in the first interval.** The code falls through to `x1 = x2`, which is meant for a forward move. The left end becomes the old right end (later in time), `getNextEntry` returns the second entry as `x2`, and `x <= x2` holds, so the result is an extrapolation through the old right end and the second entry. The bracket is now reversed (`x1 > x2`). The next lookup rewinds again, and this time `x1 = x2` makes both ends the second entry, so `table_interpolate()` returns their mean, which is the second entry's value. TS1 stays at 10 cfs until the routing clock passes 0:15.
2. **Lookup before the first entry.** The early return leaves the bracket as `[first entry, old right end]`. Later lookups that fall inside that range pass the first test of the function and are interpolated across every entry in between. TS2 is interpolated from (0:05, 0) to (0:20, 30) until the next runoff step moves the bracket.

The cursor sharing itself is long-standing design (5.x forbids sharing a rain gage's series, ERROR 158, but nothing else). The defect is that the rewind does not leave a consistent bracket.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-28_shared-series.inp`](NUM-28_shared-series.inp) | No rain; TS1 and TS2 each drive an `EXT` buildup function (18-minute runoff steps) and a FLOW inflow at J1 / J2 (1-minute routing steps, kinematic wave) |
| [`NUM-28_test.c`](NUM-28_test.c) | Legacy toolkit test: compares J1's and J2's lateral inflow at every routing step with the series interpolated at the step's start time, and the report's External Inflow volume with the integral of the series |
| [`NUM-28_test6.c`](NUM-28_test6.c) | The same checks through the 6.0.0 API (`swmm_node_get_lateral_inflow`, `swmm_get_routing_total`) |
| [`NUM-28_swmm530.patch`](NUM-28_swmm530.patch) | The fix for 5.3.0 |

```sh
tools/run-test.sh NUM-28            # 5.2.4 and 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh NUM-28 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same):

```
  t (min)   J1 inflow    TS1   J2 inflow    TS2   (cfs)
        0      0.0000  0.0000      0.0000  0.0000
        1     10.0000  0.6667      0.0000  0.0000
        5     10.0000  3.3333      0.0000  0.0000
        8     10.0000  5.3333      6.0000  3.0000
       10     10.0000  6.6667     10.0000  5.0000
       14     10.0000  9.3333     18.0000  9.0000
       16     10.6667 10.6667     22.0000 14.0000
       17     11.3333 11.3333     24.0000 18.0000
       20     13.3333 13.3333     29.9999 30.0000
       40      6.6666  6.6667      0.0000  0.0000
       61      2.0000  2.0000      0.0000  0.0000
       80     21.6666 21.6667      0.0000  0.0000
Routing steps with a wrong inflow: 17, largest error 10.0000 cfs at 15 min
External Inflow volume: report 1.948 acre-ft, integral of TS1 + TS2 1.756 acre-ft
FAIL: 17 routing steps use a wrong series value (up to 10.00 cfs off at 15 min); External Inflow 1.948 acre-ft instead of 1.756
NUM-28 5.2.4 base: FAIL
NUM-28 5.3.0 base: FAIL
```

**6.0.0, and 5.3.0 with the fix** (the inflow table is identical in both):

```
  t (min)   J1 inflow    TS1   J2 inflow    TS2   (cfs)
        0      0.0000  0.0000      0.0000  0.0000
        1      0.6667  0.6667      0.0000  0.0000
        5      3.3333  3.3333      0.0000  0.0000
        8      5.3333  5.3333      3.0000  3.0000
       10      6.6667  6.6667      5.0000  5.0000
       14      9.3333  9.3333      9.0000  9.0000
       16     10.6667 10.6667     14.0001 14.0000
       17     11.3333 11.3333     18.0001 18.0000
...
Routing steps with a wrong inflow: 0, largest error 0.0001 cfs at 16 min
External Inflow volume: report 1.756 acre-ft, integral of TS1 + TS2 1.756 acre-ft
PASS: every routing step applies the series value at its start time and the External Inflow volume matches the integral
NUM-28 5.3.0 patched: PASS
NUM-28 6.0.0 base: PASS
```

The remaining 0.0001 cfs is the 1 ms that `getDateTime()` adds to every lookup time.

## The fix

After rewinding, make the bracket start and end at the first entry. The forward step `x1 = x2` that follows then leaves `x1` at the first entry, and the search continues from there; the early return before the first entry leaves a zero-width bracket, which the next call rewinds again.

```diff
         table_getFirstEntry(table, &(table->x1), &(table->y1));
+
+        // --- collapse the bracket onto the first entry so that the search
+        //     below starts there and not from the old bracket's end
+        table->x2 = table->x1;
+        table->y2 = table->y1;
         if ( x < table->x1 )
```

Forward lookups never reach this branch, so models whose series are read at monotonic times are unchanged. 6.0.0 needs no change.

**Effect on other models.** None of the 73 regression decks in the review's suite reads one series from a runoff-clock and a routing-clock consumer (checked by listing every series used by more than one section, `EXT` buildup or outfall stage). `Example2.inp`, `extran5.inp` and `control_rules_test.inp` (series-driven inflows and control rules) give byte-identical `.rpt` (apart from the run times) and `.out` files with the patched 5.3.0 CLI.
