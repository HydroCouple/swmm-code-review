# IO-18: [SUBAREAS] accepts a PctZero above 100 and runs with a negative sub-area

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A PctZero above 100 is accepted and gives the impervious sub-area with depression storage a negative area. On 10 acres, 50 % impervious, with 0.25 in of rain and 0.125 in of infiltration: PctZero 150 gives 0.149 in of runoff and -0.024 in of final storage in 5.2.4 and 5.3.0. In 5.3.0, PctZero 400 gives 0.274 in of runoff (more than the 0.125 in left after infiltration) and -0.149 in of storage. The continuity error stays small (-0.045 % and -0.060 %), so nothing looks wrong. 6.0.0 reports 0.187 and 0.498 in of runoff with continuity errors of -25 % and -150 %. |
| **Reached from** | `[SUBAREAS]` PctZero (%Zero) greater than 100, e.g. a typo or a fraction entered as a percent the wrong way round |
| **5.3.0** | `subcatch_readSubareaParams()` in [`src/legacy/engine/subcatch.c:241`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L241) checks only for negative values; the area fraction is formed at [`subcatch.c:269`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L269) |
| **5.2.4** | Same code, [`src/solver/subcatch.c:221`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L221) and [`subcatch.c:249`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L249) |
| **6.0.0** | Reproduces with different numbers: `handle_subareas()` in [`src/engine/input/handlers/CatchmentHandler.cpp:227`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/CatchmentHandler.cpp#L227) stores PctZero with no check at all |
| **Since** | Every release |
| **Fix** | Reject PctZero > 100 with ERROR 211: [`IO-18_swmm530.patch`](IO-18_swmm530.patch), [`IO-18_swmm600.patch`](IO-18_swmm600.patch) |

## The problem

PctZero is the percent of a subcatchment's impervious area that has no depression storage. SWMM splits the impervious area with it into IMPERV0 (`fracImperv * PctZero/100`) and IMPERV1 (`fracImperv * (1 - PctZero/100)`). Above 100, IMPERV1's area fraction is negative. The three fractions still add up to 1, so nothing else catches it, and every flux of IMPERV1 changes sign: its rainfall, storage and runoff are negative and IMPERV0's are inflated to match.

With the test deck (10 ac, 50 % impervious, 0.25 in of rain, Horton infiltration taking 0.125 in):

| PctZero | Engine | Surface runoff (in) | Final storage (in) | Continuity error |
|---|---|---|---|---|
| 150 | 5.2.4, 5.3.0 | 0.149 | -0.024 | -0.045 % |
| 150 | 6.0.0 | 0.187 | 0.001 | -25.062 % |
| 400 | 5.3.0 | 0.274 | -0.149 | -0.060 % |
| 400 | 6.0.0 | 0.498 | 0.003 | -150.166 % |

The legacy engines produce more runoff than the rain minus infiltration and balance it with negative storage. The small continuity error gives no hint. 6.0.0 accepts the same input and its continuity error at least shows that something is wrong.

## Why it happens

```c
// src/legacy/engine/subcatch.c, subcatch_readSubareaParams()
// --- read in Mannings n, depression storage, & PctZero values
for (i = 0; i < 5; i++)
{
    if ( ! getDouble(tok[i+1], &x[i])  || x[i] < 0.0 )     // no upper bound for PctZero
        return error_setInpError(ERR_NAME, tok[i+1]);
}
...
    if ( ! getDouble(tok[7], &x[6]) || x[6] < 0.0 || x[6] > 100.0 )   // PctRouted is bounded
        return error_setInpError(ERR_NUMBER, tok[7]);
...
Subcatch[j].subArea[IMPERV0].fArea  = Subcatch[j].fracImperv * x[4] / 100.0;
Subcatch[j].subArea[IMPERV1].fArea  = Subcatch[j].fracImperv * (1.0 - x[4] / 100.0);
```

The percent routed on the same line is limited to 100 and PctZero is not. (The `ERR_NAME` code in the loop is [IO-20](../IO-20-subareas-bad-number-error-209/).) 6.0.0's `handle_subareas()` stores `to_double(tok[5])` with no check of any kind.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-18_pctzero-150.inp`](IO-18_pctzero-150.inp) | 10 ac, 50 % impervious, PctZero 150, 1 in/hr for 15 min, Horton infiltration |
| [`IO-18_test.c`](IO-18_test.c) | Opens the deck through the legacy toolkit (5.2.4 and 5.3.0). It passes if the input is rejected; if the run goes ahead it prints the runoff continuity table and fails. |
| [`IO-18_test6.c`](IO-18_test6.c) | The same with the 6.0.0 engine API |

```sh
tools/run-test.sh IO-18            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-18 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** 5.2.4 and 5.3.0 print:

```
PctZero = 150 accepted; the run gives (inches):
  Total precipitation    0.250
  Infiltration loss      0.125
  Surface runoff         0.149
  Final storage         -0.024
FAIL: PctZero = 150 is accepted and gives the impervious sub-area with depression storage a negative area (final storage -0.024 in)
IO-18 5.2.4 base: FAIL
IO-18 5.3.0 base: FAIL
```

and 6.0.0:

```
PctZero = 150 accepted; the run gives (inches):
  Total precipitation    0.250
  Infiltration loss      0.125
  Surface runoff         0.187
  Final storage          0.001
  Continuity error (%) -25.062
FAIL: PctZero = 150 is accepted and the run loses track of the water (runoff continuity error -25.062 %)
IO-18 6.0.0 base: FAIL
```

**With the fix**:

```
PctZero = 150 rejected, error code 200; the report says:
  ERROR 211: invalid number 150 at line 32 of [SUBAREA] section:
PASS: a PctZero above 100 is reported as an input error
IO-18 5.3.0 patched: PASS
PctZero = 150 rejected, error code 5; the engine says:
    ERROR 211: invalid number 150.
PASS: a PctZero above 100 is reported as an input error
IO-18 6.0.0 patched: PASS
```

The PctZero 400 rows of the table come from the same deck with 400 in place of 150, run with the unpatched command-line programs.

## The fix

Reject a PctZero above 100 with ERROR 211, as `subcatch_readSubareaParams()` already does for a PctRouted above 100:

```diff
+    // --- PctZero is a percent of the impervious area
+    if ( x[4] > 100.0 ) return error_setInpError(ERR_NUMBER, tok[5]);
```

The check sits after the PctRouted check rather than in the number loop, so that it does not overlap the [IO-20](../IO-20-subareas-bad-number-error-209/) patch, which changes the loop's error code. The 6.0.0 patch adds the same check after `handle_subareas()` stores PctZero, with the same error.

**Effect on other models.** None of the regression decks has a PctZero above 100, so their results are unchanged. A model with one now stops with ERROR 211.
