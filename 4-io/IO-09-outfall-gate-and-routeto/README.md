# IO-09: An outfall's flap gate is dropped when the line also names a RouteTo subcatchment

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Silent wrong results. An outfall given both a flap gate and a RouteTo subcatchment runs without the gate, so a high receiving-water stage flows back into the system. In the test, 5.8 cfs flows back through the outfall conduit and the upstream junction fills to its 10-ft rim; with the gate nothing flows. No error or warning, and any text in the Gated column is accepted. |
| **Reached from** | `[OUTFALLS]` lines with all optional columns: `Name Elev Type (StageData) Gated RouteTo` |
| **5.3.0** | `outfall_readParams()` in [`src/legacy/engine/node.c:1378`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L1378) and [`node.c:1385`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L1385) |
| **5.2.4** | Same code, [`src/solver/node.c:1392`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L1392) and [`node.c:1399`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L1399) |
| **6.0.0** | Not affected: the `[OUTFALLS]` handler reads the Gated column and then the RouteTo column whenever they are present ([`src/engine/input/handlers/NodesHandler.cpp:244`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/NodesHandler.cpp#L244)) |
| **Since** | 5.1.008, which added the RouteTo column (2015) |
| **Fix** | Read each optional column whenever the line is long enough: [`IO-09_swmm530.patch`](IO-09_swmm530.patch) |

## The problem

The input reference gives the outfall line as

```
Name  Elev  FREE        (Gated)  (RouteTo)
Name  Elev  FIXED       Stage   (Gated)  (RouteTo)
...
```

with Gated "YES or NO depending on whether a flap gate is present that prevents reverse flow" and RouteTo an optional subcatchment that receives the outfall's discharge. A line with both columns, `O1 99 FIXED 102 YES S1`, is read with the RouteTo subcatchment but without the gate.

In the test, junction J1 (invert 100 ft) drains through conduit C1 to outfall O1, whose fixed stage is 102 ft. O1 has a flap gate, so nothing may flow back into J1. With `O1 99 FIXED 102 YES` the run is correct: C1 carries no flow and J1 stays dry. Adding the RouteTo subcatchment `S1` to the same line makes 5.2.4 and 5.3.0 drop the gate: 5.82 cfs flows back through C1, J1 fills to its 10-ft maximum depth and floods, and the routing continuity error is −4.3 %. Nothing is reported, and `O1 99 FIXED 102 XYZ S1` is accepted the same way.

## Why it happens

`outfall_readParams()` sets `n` to the position just after the type's own data (4 for FREE and NORMAL, 5 for FIXED, TIDAL and TIMESERIES), so the Gated column is token `n-1` and RouteTo is token `n`. It then tests the token count for equality:

```c
// src/legacy/engine/node.c, outfall_readParams()
    if ( ntoks == n )
    {
        m = findmatch(tok[n-1], NoYesWords);               // flap gate
        if ( m < 0 ) return error_setInpError(ERR_KEYWORD, tok[n-1]);
        x[5] = m;
    }

    if ( ntoks == n+1)
    {
        m = project_findObject(SUBCATCH, tok[n]);
        if ( m < 0 ) return error_setInpError(ERR_NAME, tok[n]);
        x[6] = m;
    }
```

When RouteTo is present, `ntoks` is `n+1`, so the first block is skipped: the gate keeps the default `x[5] = 0` set a few lines above, and the Gated token is neither parsed nor checked. The RouteTo block was added in 5.1.008 below the existing gate block, which had been written for lines that ended with the gate.

6.0.0 walks the optional columns with a running index (`if (tok.size() > next) { ... has_flap_gate ...; ++next; }`, then RouteTo), so it reads both. Two differences in 6.0.0's handler, outside this issue: a Gated value other than YES, TRUE or 1 is read as NO without an error, and a RouteTo subcatchment whose `[SUBCATCHMENTS]` section comes after `[OUTFALLS]` is dropped without an error (the lookup at [`NodesHandler.cpp:252`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/NodesHandler.cpp#L252) runs before the subcatchments exist and the row is not deferred).

## How to reproduce

| File | What it is |
|---|---|
| [`IO-09_gate.inp`](IO-09_gate.inp) | J1 drains through C1 to outfall O1 (FIXED stage 2 ft above J1's invert) with `O1 99 FIXED 102 YES` |
| [`IO-09_gate-routeto.inp`](IO-09_gate-routeto.inp) | The same deck with `O1 99 FIXED 102 YES S1` |
| [`IO-09_test.c`](IO-09_test.c) | Runs both decks through the legacy toolkit and records the largest C1 flow and J1 depth; both must be 0 |
| [`IO-09_test6.c`](IO-09_test6.c) | The same check through the 6.0.0 API |

```sh
tools/run-test.sh IO-09            # 5.2.4, 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-09 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0 print the same):

```
Outfall O1 (stage 2 ft above J1's invert) with a flap gate:
  [OUTFALLS] line             error    max |C1 flow|     max J1 depth
  O1 99 FIXED 102 YES             0        0.000 cfs         0.000 ft
  O1 99 FIXED 102 YES S1          0        5.822 cfs        10.000 ft
  (correct: 0 cfs and 0 ft for both lines)
FAIL: the flap gate is ignored: 5.822 cfs flows back through C1 and J1 fills to 10.000 ft
IO-09 5.3.0 base: FAIL
```

6.0.0 gives 0.000 cfs and 0.000 ft for both lines (`IO-09 6.0.0 base: PASS`).

**With the fix**, 5.3.0 matches:

```
  O1 99 FIXED 102 YES             0        0.000 cfs         0.000 ft
  O1 99 FIXED 102 YES S1          0        0.000 cfs         0.000 ft
  (correct: 0 cfs and 0 ft for both lines)
PASS: the flap gate stops backflow with and without a RouteTo subcatchment
IO-09 5.3.0 patched: PASS
```

## The fix

Read each optional column whenever the line reaches it:

```diff
-    if ( ntoks == n )
+    if ( ntoks >= n )
     {
         m = findmatch(tok[n-1], NoYesWords);               // flap gate
...
-    if ( ntoks == n+1)
+    if ( ntoks >= n+1 )
     {
         m = project_findObject(SUBCATCH, tok[n]);
```

An invalid Gated token on a line with RouteTo is now rejected with ERROR 205, as it already was without RouteTo. Extra tokens after RouteTo no longer cause both optional columns to be skipped; they are ignored, as in the other sections.

**Effect on other models.** Lines without RouteTo are read as before. Of the regression decks, only `update_v5111/catchment_as_outfall.inp` has an outfall with a RouteTo column, and its Gated value is NO, which is also the default, so its results do not change.
