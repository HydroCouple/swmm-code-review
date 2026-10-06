# IO-15: [GROUNDWATER] rows with the 10 required fields are rejected

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A `[GROUNDWATER]` row that gives the ten required fields and leaves out the optional ones, as the input reference allows, stops the run with `ERROR 203: too few items`. Users must add a `*` (or a value) for the optional threshold elevation Egwt to get it read. |
| **Reached from** | Any `[GROUNDWATER]` row with exactly 10 tokens, `Subcat Aquifer Node Esurf A1 B1 A2 B2 A3 Dsw` |
| **5.3.0** | `gwater_readGroundwaterParams()` requires 11 tokens, [`src/legacy/engine/gwater.c:220`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L220) |
| **5.2.4** | Same code, [`src/solver/gwater.c:202`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gwater.c#L202) |
| **6.0.0** | Reproduces, copied for parity: [`src/engine/input/handlers/HydrologyHandler.cpp:421`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/HydrologyHandler.cpp#L421) |
| **Since** | 5.1.000, which added the optional Ebot, Egw and Umc fields and raised the check from 10 to 11 tokens (5.0.022 accepted 10) |
| **Fix** | Require 10 tokens: [`IO-15_swmm530.patch`](IO-15_swmm530.patch), [`IO-15_swmm600.patch`](IO-15_swmm600.patch) |

## The problem

The input reference gives the format of a `[GROUNDWATER]` row as

```
Subcat Aquifer Node Esurf A1 B1 A2 B2 A3 Dsw (Egwt Ebot Egw Umc)
```

and says of Egwt, the threshold groundwater elevation: "Leave blank (or enter *) to use the elevation of the receiving node's invert." Ebot, Egw and Umc are optional overrides of the aquifer's values. A row with the ten required fields is therefore complete:

```
[GROUNDWATER]
S1   AQ   J1   10   0.05   1.5   0   0   0   0
```

All three engines reject it:

```
  ERROR 203: too few items at line 44 of [GROUNDWATER] section:
```

Adding `*` as an 11th field is accepted and means exactly "blank": the row with `*` is read the same as the 10-field row should be. The groundwater decks of NUM-35, NUM-36, CON-07 and BND-11 in this review carry that workaround.

## Why it happens

```c
// src/legacy/engine/gwater.c, gwater_readGroundwaterParams()
//  Data format is:
//  subcatch  aquifer  node  surfElev  a1  b1  a2  b2  a3  fixedDepth +
//            (nodeElev  bottomElev  waterTableElev  upperMoisture )
    ...
    // --- check for enough tokens
    if ( ntoks < 11 ) return error_setInpError(ERR_ITEMS, "");
    ...
    // -- read in the flow parameters
    for ( i = 0; i < 7; i++ )                      // tok[3] .. tok[9]
    ...
    // --- read in optional depth parameters
    for ( i = 7; i < 11; i++)
    {
        x[i] = MISSING;
        m = i + 3;                                 // tok[10] .. tok[13]
        if ( ntoks > m && *tok[m] != '*' )
```

The required fields end at `tok[9]`, and the loop that follows already treats `tok[10]` onward as optional, so the check is one too strict. 5.0.022 checked `ntoks < 10`; the limit became 11 when 5.1.000 added the optional fields. 6.0.0's `handle_groundwater()` reproduces the check, with a comment citing legacy.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-15_ten-fields.inp`](IO-15_ten-fields.inp) | 10-acre aquifer draining to J1 for a day; `[GROUNDWATER]` row with the 10 required fields |
| [`IO-15_star.inp`](IO-15_star.inp) | The same with `*` as the 11th field (the workaround) |
| [`IO-15_test.c`](IO-15_test.c) | Runs both decks through the legacy toolkit; the 10-field deck must run and give the same Groundwater Flow (report's Groundwater Continuity table) as the deck with `*` |
| [`IO-15_test6.c`](IO-15_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh IO-15            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-15 --patched  # 5.3.0, 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 (6.0.0 prints `error 5`, and its report says `ERROR 203: too few items.`):

```
[GROUNDWATER] row                    Result
10 fields                            error 200
10 fields + *                        GW flow 7.055 ac-ft
FAIL: a [GROUNDWATER] row with the 10 required fields is rejected (error 200; the report says ERROR 203)
IO-15 5.2.4 base: FAIL
IO-15 5.3.0 base: FAIL
IO-15 6.0.0 base: FAIL
```

**With the fix**, both engines:

```
[GROUNDWATER] row                    Result
10 fields                            GW flow 7.055 ac-ft
10 fields + *                        GW flow 7.055 ac-ft
PASS: a 10-field [GROUNDWATER] row is accepted and equals the row with * (7.055 ac-ft)
IO-15 5.3.0 patched: PASS
IO-15 6.0.0 patched: PASS
```

## The fix

```diff
     // --- check for enough tokens
-    if ( ntoks < 11 ) return error_setInpError(ERR_ITEMS, "");
+    if ( ntoks < 10 ) return error_setInpError(ERR_ITEMS, "");
```

6.0.0: `if (tok.size() < 10)` in `handle_groundwater()`, with the comment updated. Rows with 11 or more fields are read exactly as before, and rows with 9 or fewer are still rejected with ERROR 203. Only input that used to stop the run is affected, so no existing model's results change.
