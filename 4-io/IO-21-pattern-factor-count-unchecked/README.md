# IO-21: A time pattern with a missing or extra factor is accepted without a message

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A `[PATTERNS]` entry with too few factors uses 1.0 for each missing one; one with more than 24 drops the rest. A single missing value in an HOURLY pattern of 0.5 factors doubles the dry weather flow for the last hour of every day (10 cfs instead of 5 cfs in the test). No error or warning is written. |
| **Reached from** | Any `[PATTERNS]` entry whose factor count differs from 12 (MONTHLY), 7 (DAILY) or 24 (HOURLY, WEEKEND), typically a hand-edited or generated input file; used by `[DWF]` and `[INFLOWS]` |
| **5.3.0** | `inflow_readDwfPattern()` in [`src/legacy/engine/inflow.c:424`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inflow.c#L424); defaults set in `inflow_initDwfPattern()`, [`inflow.c:383`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inflow.c#L383) |
| **5.2.4** | Same code: [`src/solver/inflow.c:443`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/inflow.c#L443) |
| **6.0.0** | Reproduces: `handle_patterns()` in [`InflowsHandler.cpp:89`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/InflowsHandler.cpp#L89) appends whatever factors it finds, and `InflowSolver::refreshPatterns()` pads to 24 with 1.0 |
| **Since** | Every 5.x release |
| **Fix** | Report error 237 for a pattern whose factor count does not match its type: [`IO-21_swmm530.patch`](IO-21_swmm530.patch), [`IO-21_swmm600.patch`](IO-21_swmm600.patch) |

## The problem

The input reference gives each pattern type a fixed number of factors:

```
Name  MONTHLY  Factor1  Factor2  ...  Factor12
Name  DAILY    Factor1  Factor2  ...  Factor7
Name  HOURLY   Factor1  Factor2  ...  Factor24
Name  WEEKEND  Factor1  Factor2  ...  Factor24
```

and allows a pattern to continue over several lines. None of the three engines checks the count. A pattern that lost a value (a line wrapped wrongly, a value deleted by mistake) is accepted: every factor after the gap moves one slot earlier and the last slot keeps its default of 1.0. A pattern with values to spare keeps the first 24 and drops the rest.

In the test a junction has a 10 cfs DWF with an HOURLY pattern of 0.5 factors. With all 24 factors the lateral inflow at 23:30 is 5.0 cfs. With 23, the run is accepted and gives 10.0 cfs from 23:00 to midnight every day. With 25, the run is accepted as well.

## Why it happens

```c
// src/legacy/engine/inflow.c
void inflow_initDwfPattern(int patternIndex)
{
    for (i=0; i<24; i++) Pattern[patternIndex].factor[i] = 1.0;
    Pattern[patternIndex].count = 0;
    ...
}

int inflow_readDwfPattern(char* tok[], int ntoks)
{
    ...
    // --- start reading pattern factors from rest of line
    while ( ntoks > n && Pattern[j].count < 24 )
    {
        ...
        Pattern[j].count++;
        n++;
    }
    return 0;                     // tokens beyond the 24th are ignored
}
```

`Pattern[j].count` is only used by the toolkit getters. `getPatternFactor()` reads `factor[hour]`, `factor[month]` or `factor[day]` whatever the count. 6.0.0 keeps all factors in a vector, and `refreshPatterns()` copies the first 24 into a table initialised with 1.0.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-21_hourly23.inp`](IO-21_hourly23.inp) | J1 - C1 - O1, 10 cfs DWF at J1 with HOURLY pattern P1 of 0.5 factors over two lines, 23 factors; 24-hour KINWAVE run |
| [`IO-21_hourly24.inp`](IO-21_hourly24.inp) | The same with all 24 factors (control) |
| [`IO-21_hourly25.inp`](IO-21_hourly25.inp) | The same with 25 factors (13 on the first line) |
| [`IO-21_test.c`](IO-21_test.c) | Legacy toolkit (5.2.4 and 5.3.0): runs each deck, records the error code and J1's lateral inflow at 23:30 |
| [`IO-21_test6.c`](IO-21_test6.c) | The same for 6.0.0 |

The test requires the 23- and 25-factor decks to be refused with an error, and the 24-factor deck to run and give 5.0 cfs at 23:30.

```sh
tools/run-test.sh IO-21            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-21 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (all three engines print the same):

```
Deck                  Factors  Error code  J1 lateral inflow at 23:30 (cfs)
IO-21_hourly23.inp         23           0      10.000  <-- accepted without a message
IO-21_hourly24.inp         24           0       5.000
IO-21_hourly25.inp         25           0       5.000  <-- accepted without a message
FAIL: 2 of 3 decks handled wrongly; a pattern with a missing or extra factor runs without a message (missing factor -> 1.0)
```

**With the fix**:

```
---- 5.3.0 ----
Deck                  Factors  Error code  J1 lateral inflow at 23:30 (cfs)
IO-21_hourly23.inp         23         237   (not run)
IO-21_hourly24.inp         24           0       5.000
IO-21_hourly25.inp         25         200   (not run)
PASS: patterns with 23 or 25 hourly factors are refused, the complete one runs as given

---- 6.0.0 ----
IO-21_hourly23.inp         23           5   (not run)
IO-21_hourly24.inp         24           0       5.000
IO-21_hourly25.inp         25           5   (not run)
PASS: patterns with 23 or 25 hourly factors are refused, the complete one runs as given
```

The reports say:

```
5.3.0, 23 factors:  ERROR 237: wrong number of factors for Time Pattern P1
5.3.0, 25 factors:  ERROR 237: wrong number of factors for Time Pattern P1 at line 41 of [PATTERN] section:
                    P1              0.5 0.5 0.5 0.5 0.5 0.5 0.5 0.5 0.5 0.5 0.5 0.5
6.0.0, both:        ERROR 237: wrong number of factors for Time Pattern P1.
```

(5.3.0 returns 200, "one or more errors in input file", for an error found while reading, and the error's own code for one found during validation; 6.0.0 returns its parse-error code 5.)

## The fix

5.3.0: a 25th factor is reported where it is read, since only 24 can be stored, and every pattern's count is checked once the whole input has been read:

```diff
+    // --- check that each time pattern has the number of factors its
+    //     type requires (12 monthly, 7 daily, 24 hourly or weekend)
+    for ( i=0; i<Nobjects[TIMEPATTERN]; i++ )
+    {
+        j = Pattern[i].type;
+        if ( j >= 0 && Pattern[i].count != (j == MONTHLY_PATTERN ? 12 :
+                                            j == DAILY_PATTERN ? 7 : 24) )
+            report_writeErrorMsg(ERR_PATTERN_FACTORS, Pattern[i].ID);
+    }
```

```diff
+    // --- more than 24 factors cannot be stored
+    if ( ntoks > n ) return error_setInpError(ERR_PATTERN_FACTORS, tok[0]);
     return 0;
```

The new error 237 is added to `error.h` and `error.txt`. 6.0.0 gets the same check in `resolve_cross_references()`, next to the existing check that pattern names exist, and the same error code in `ErrorCodes.hpp/.cpp`.

**Effect on other models.** Input files whose patterns are complete are not affected. An input file that relied on short patterns stops with error 237 and must be completed; this is the intent. The regression suite has one pattern, which is complete. Among the input files under the 5.2.4 and 6.0.0 source trees (33 with patterns), the only pattern that is not complete line by line is in the 6.0.0 unit-test deck `namecase_multiline_exempt.inp`: four case-variant lines of 6 HOURLY factors, which 6.0.0 merges into one pattern of 24, so it still opens.

Not changed here: 6.0.0 also accepts a non-numeric factor as 1.0 (`to_double(tok, 1.0)`) and the type keyword repeated on continuation lines, both of which 5.3.0 rejects with error 211; that unit-test deck relies on the second.
