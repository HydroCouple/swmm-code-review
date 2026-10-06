# IO-28: A transect is only built when an NC line or a known section header follows it

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Valid input is rejected with `ERROR 113: invalid roughness` and `ERROR 119: invalid cross section` for every conduit on the affected transect; neither message names the transect. With two `[TRANSECTS]` sections, a conduit silently gets the shape of a different transect (250 ft² full area becomes 150 ft² in the test). |
| **Reached from** | `[TRANSECTS]` written as the manual allows: an X1 line without its own NC line, `[TRANSECTS]` as the last section of the file, `[TRANSECTS]` followed by an unknown section (5.3.0), or more than one `[TRANSECTS]` section |
| **5.3.0** | `transect_readParams()` in [`src/legacy/engine/transect.c:138`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L138) and [`:150`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L150); `input_readData()` in [`src/legacy/engine/input.c:219`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L219) and [`:233`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L233) |
| **5.2.4** | Same code, [`src/solver/transect.c:136`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/transect.c#L136), [`src/solver/input.c:213`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/input.c#L213). 5.2.4 rejects unknown sections with ERROR 205, so that one path does not exist there. |
| **6.0.0** | Not affected: [`handle_transects()`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/LinksHandler.cpp#L544) stores each X1 block as it is read and builds all transects afterwards. |
| **Since** | Every SWMM 5 release. The unknown-section path since 5.3.0 (fork commit a2869102, which made unknown sections a warning). |
| **Fix** | Build the pending transect on every X1 line, on an unknown section and at end of file, at most once, under its own index: [`IO-28_swmm530.patch`](IO-28_swmm530.patch) Apply after NUM-23, NUM-24 (their `Requires:` lines). |

## The problem

The manual's remarks on `[TRANSECTS]` say:

> The first line in this section must always be a NC line. After that, the NC line is only needed when a transect has different Manning's n values than the previous one.

and section keywords "can appear in any arbitrary order in the input file". The legacy reader does not honour either rule. A transect's geometry tables (area, width, hydraulic radius) and its roughness are only built when something tells the reader that the transect has ended, and only two things do: the next NC line, and the header of a known section. So:

- **An X1 line without an NC line before it.** The previous transect is never built. Its stations are thrown away when the X1 line starts the next transect.
- **`[TRANSECTS]` as the last section in the file.** The last transect is never built.
- **`[TRANSECTS]` followed by a section the engine does not know** (5.3.0 skips these with a warning). The last transect is never built.
- **Two `[TRANSECTS]` sections.** At the end of the first section the reader builds "transect number *N*−1", where *N* is the total number of transects in the file, not the number read so far. The first section's last transect is built into the slot of the file's last transect. If the second section ends the file, that slot is never rebuilt and the conduit on the last transect runs with the wrong shape, without any message.

An unbuilt transect has roughness 0 and full area 0. `link_validate()` copies both into each conduit that uses the transect ([`link.c:1022-1024`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1022)) and stops the run with ERROR 113 and ERROR 119 for each of them. The messages name the conduit, not the transect, and nothing points at the layout of `[TRANSECTS]`.

## Why it happens

`transect_validate(j)` builds transect `j` from the station data the reader keeps in file-scope arrays. It is called for the previous transect on an NC line only; the X1 line goes straight to `setParams()`, which resets the station count:

```c
// src/legacy/engine/transect.c, transect_readParams()
      case 0:                                   // NC line
        // --- finish processing the previous transect
        transect_validate(index - 1);
        ...
      case 1:                                   // X1 line: no transect_validate()
        ...
        *count = index + 1;
        return setParams(index, id, x);         // ends with: Nstations = 0;
```

The only other call is in `input_readData()`, on a known section header. The unknown-section branch and the end of the file have none, and the index is the total transect count `Nobjects`, not the running count `Mobjects` that `transect_readParams()` keeps:

```c
// src/legacy/engine/input.c, input_readData()
            if (newsect >= 0)
            {
                if ( sect == s_TRANSECT )
                    transect_validate(Nobjects[TRANSECT]-1);
                sect = newsect;
                continue;
            }
            else
            {
                // --- unknown section: warn and skip until next known section
                ...
                sect = -1;
                continue;
            }
```

`transect_validate()` also has no guard against being called twice. On a second `[TRANSECTS]` section, its first NC line validates the first section's last transect again, after the first call has already appended the vertical end walls to the station list.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-28_one-nc.inp`](IO-28_one-nc.inp) | One NC line, then X1/GR for T1 and X1/GR for T2; `[REPORT]` follows |
| [`IO-28_transects-last.inp`](IO-28_transects-last.inp) | NC before each X1; `[TRANSECTS]` is the last section |
| [`IO-28_unknown-section.inp`](IO-28_unknown-section.inp) | NC before each X1; `[TRANSECTS]` is followed by `[MY_TOOL_DATA]` |
| [`IO-28_two-sections.inp`](IO-28_two-sections.inp) | T1 in a first `[TRANSECTS]` section, T2 in a second one at the end of the file |
| [`IO-28_test.c`](IO-28_test.c) | Runs the four decks through the legacy toolkit (5.2.4 and 5.3.0) and reads the Cross Section Summary from each report |
| [`IO-28_test6.c`](IO-28_test6.c) | The same through `swmm_engine_run()` (6.0.0) |

Every deck has conduit C1 on T1 and C2 on T2. Both transects are trapezoids 5 ft deep, so the expected values follow from the GR data: T1 is 20 ft wide at the bottom and 40 ft at the top (full area 150 ft²), T2 is 40 ft and 60 ft wide (250 ft²). The test checks full depth, full area and maximum width of both conduits. For 5.2.4 the unknown-section deck is skipped, because 5.2.4 rejects unknown sections by design.

```sh
tools/run-test.sh IO-28            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-28 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix**, 5.3.0 rejects three of the decks and runs the fourth with C2 on T1's shape (5.2.4 gives the same results, except for the skipped unknown-section deck):

```
Deck                     Error  Link   Depth    Area   Width
                                        (ft)   (ft2)    (ft)
one-nc                     119  (run rejected; the report says:)
                                ERROR 113: invalid roughness for Conduit C1.
                                ERROR 119: invalid cross section for Link C1.
transects-last             119  (run rejected; the report says:)
                                ERROR 113: invalid roughness for Conduit C2.
                                ERROR 119: invalid cross section for Link C2.
unknown-section            119  (run rejected; the report says:)
                                ERROR 113: invalid roughness for Conduit C2.
                                ERROR 119: invalid cross section for Link C2.
two-sections                 0  C1      5.00  150.00   40.00
                             0  C2      5.00  150.00   40.00   <-- wrong shape
FAIL: 4 of 4 valid decks rejected or with a wrong transect: one-nc (error 119, ERROR 113/119 in the report), transects-last (error 119, ERROR 113/119 in the report), unknown-section (error 119, ERROR 113/119 in the report), two-sections (runs, but a conduit has the wrong shape)
IO-28 5.3.0 base: FAIL
```

6.0.0 builds every transect from its own data:

```
one-nc                       0  C1      5.00  150.00   40.00
                             0  C2      5.00  250.00   60.00
...
two-sections                 0  C1      5.00  150.00   40.00
                             0  C2      5.00  250.00   60.00
PASS: every transect is built with its own shape, whatever line or section follows it
IO-28 6.0.0 base: PASS
```

**With the fix**, 5.3.0 gives the same table as 6.0.0 for all four decks, and the same full flows (538.37 cfs for C1 and 996.38 cfs for C2 in every deck):

```
one-nc                       0  C1      5.00  150.00   40.00
                             0  C2      5.00  250.00   60.00
transects-last               0  C1      5.00  150.00   40.00
                             0  C2      5.00  250.00   60.00
unknown-section              0  C1      5.00  150.00   40.00
                             0  C2      5.00  250.00   60.00
two-sections                 0  C1      5.00  150.00   40.00
                             0  C2      5.00  250.00   60.00
PASS: every transect is built with its own shape, whatever line or section follows it
IO-28 5.3.0 patched: PASS
```

## The fix

The pending transect is built wherever a transect can end: on an X1 line, on an unknown section header and at the end of the file, as well as on an NC line and a known section header as before. A flag that `setParams()` clears and `transect_validate()` sets makes each transect build exactly once. The section-end calls use the running count `Mobjects[TRANSECT]`:

```diff
       case 1:
 
+        // --- finish processing the previous transect (if no NC line did)
+        transect_validate(index - 1);
+
...
     if ( j < 0 || j >= Ntransects ) return;
+    if ( Validated ) return;
+    Validated = TRUE;
...
     Nstations = 0;
+    Validated = FALSE;
```

```diff
                 if ( sect == s_TRANSECT )
-                    transect_validate(Nobjects[TRANSECT]-1);
+                    transect_validate(Mobjects[TRANSECT]-1);
...
                 report_invokeWarningCallback(warnMsg);
+                if ( sect == s_TRANSECT )
+                    transect_validate(Mobjects[TRANSECT]-1);
                 sect = -1;
...
     }   /* End of while */
 
+    // --- finish processing transect data that ends the file
+    if ( sect == s_TRANSECT ) transect_validate(Mobjects[TRANSECT]-1);
```

Input that has one `[TRANSECTS]` section, an NC line before every X1 line and a known section after it builds exactly as before. **Effect on other models:** the four regression decks with transects (`user/user2.inp`, `update_v52/CoS-Reduced-Outlets.inp`, `extran/extran8a.inp`; `Example7-*.inp` use the same layout) all have that layout. For the first three, the patched 5.3.0 gives byte-identical `.rpt` files (apart from the run timestamps) and identical `.out` files.

A transect that inherits n by leaving out its NC line now gets the previous transect's channel n. If the previous transect has a meander factor, that n is the meander-adjusted value. This is the same leak as an `NC 0 0 0` line ([NUM-22](../../1-numerical/NUM-22-transect-nc-inherits-meander-adjusted-n/)) and is fixed there.
