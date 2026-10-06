# IO-32: Grate type P_BAR-50x100 is read as P_BAR-50

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Every P_BAR-50x100 grate is modelled as a P_BAR-50 grate, which has a much higher splash-over velocity (8.16 instead of 4.70 ft/s for a 2-ft grate) and a larger open area (90% instead of 80%). On grade at high velocity the capture is overstated: on an 8% street the test grate captures 47.6% of 6 cfs instead of 40.0%, and 38.4% of 12 cfs instead of 28.2%. On sag and for drop grates, the orifice capacity is 12.5% too high. No warning. |
| **Reached from** | `[INLETS]` GRATE or DROP_GRATE with grate type `P_BAR-50x100` (any letter case) |
| **5.3.0** | `readGrateInletParams()` in [`src/legacy/engine/inlet.c:836`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inlet.c#L836) with `GrateTypeWords` at [`:139`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inlet.c#L139) |
| **5.2.4** | Same code, [`src/solver/inlet.c:836`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/inlet.c#L836) |
| **6.0.0** | Not affected: `find_word()` compares whole keywords ([`src/engine/input/handlers/InfraHandler.cpp:94`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/InfraHandler.cpp#L94)) |
| **Since** | 5.2.0, when inlets were added |
| **Fix** | Recognise the longer keyword when the prefix matches: [`IO-32_swmm530.patch`](IO-32_swmm530.patch) |

## The problem

HEC-22 lists seven standard grates. Two of them are parallel-bar grates with the same bar spacing: P-50 (P_BAR-50) and P-50x100 (P_BAR-50x100), which adds lateral rods every 4 inches. The rods make water splash over the grate at a much lower velocity, so at the same street flow a P-50x100 grate captures less of the frontal flow than a P-50.

5.2.4 and 5.3.0 never use the P-50x100 data. A deck that asks for `P_BAR-50x100` gets the splash-over velocity curve and open area of `P_BAR-50`. The test puts a 2 ft x 2 ft P_BAR-50x100 grate on an 8% street. The water reaches the grate at 6.51 ft/s (6 cfs) and 7.74 ft/s (12 cfs), above the P-50x100 splash-over velocity of 4.70 ft/s but below P-50's 8.16 ft/s. So instead of losing part of the frontal flow, the grate takes all of it, and captures 2.86 instead of 2.40 cfs and 4.61 instead of 3.38 cfs.

## Why it happens

```c
// src/legacy/engine/inlet.c
static char* GrateTypeWords[] =
  {"P_BAR-50", "P_BAR-50x100", "P_BAR-30", "CURVED_VANE", "TILT_BAR-45", "TILT_BAR-30",
   "RETICULINE", "GENERIC", NULL};
...
// readGrateInletParams()
grateType = findmatch(tok[4], GrateTypeWords);
```

`findmatch()` returns the first keyword for which `match(token, keyword)` is true, and `match()` is true when the token *starts with* the keyword, ignoring case (input.c:805). `P_BAR-50x100` starts with `P_BAR-50`, which comes first in the list, so the result is always index 0, and index 1 can never be selected. That index is then used for the splash-over velocity coefficients and the open-area ratio:

```c
static const double SplashCoeffs[][4] = {
    {2.22, 4.03, 0.65, 0.06},     //P_BAR-50
    {0.74, 2.44, 0.27, 0.02},     //P_BAR-50x100
...
static const double GrateOpeningRatios[] = {
    0.90,     //P_BAR-50
    0.80,     //P_BAR-50x100
```

No other grate keyword is a prefix of another.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-32_pbar50x100-grate.inp`](IO-32_pbar50x100-grate.inp) | Two 8% streets (Sx 2%, n 0.016, no gutter depression) carrying 6 and 12 cfs to an on-grade 2 ft x 2 ft P_BAR-50x100 grate |
| [`IO-32_test.c`](IO-32_test.c) | Legacy toolkit (5.2.4, 5.3.0). Measures each grate's capture at steady state and compares it with HEC-22 Eqs. 4-2, 4-16, 4-18, 4-19 and 4-21 for a P-50x100 grate, computed in the test |
| [`IO-32_test6.c`](IO-32_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh IO-32            # 5.2.4 and 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh IO-32 --patched  # 5.3.0 with the fix and 6.0.0: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print:

```
Splash-over velocity of a 2-ft grate: P_BAR-50x100 4.70 ft/s, P_BAR-50 8.16 ft/s
Street  Q (cfs)  V (ft/s)  captured (cfs)  capture %  HEC-22 P_BAR-50x100 %
S1a       6.000      6.51           2.856      47.60                  40.04
S2a      12.000      7.74           4.605      38.37                  28.15
FAIL: the P_BAR-50x100 grate captures up to 10.22 points more than HEC-22 gives for that grate
IO-32 5.2.4 base: FAIL
IO-32 5.3.0 base: FAIL
```

The legacy captures are what HEC-22 gives for a P-50 grate, whose frontal capture stays at 100% below 8.16 ft/s. 6.0.0 reads the keyword correctly:

```
Street  Q (cfs)  V (ft/s)  captured (cfs)  capture %  HEC-22 P_BAR-50x100 %
S1a       6.000      6.51           2.401      40.02                  40.04
S2a      12.000      7.74           3.376      28.13                  28.15
PASS: the P_BAR-50x100 grate follows HEC-22 for that grate (largest difference 0.02 points)
IO-32 6.0.0 base: PASS
```

**With the fix**, 5.3.0 prints the same values as 6.0.0:

```
S1a       6.000      6.51           2.401      40.02                  40.04
S2a      12.000      7.74           3.376      28.13                  28.15
PASS: the P_BAR-50x100 grate follows HEC-22 for that grate (largest difference 0.02 points)
IO-32 5.3.0 patched: PASS
```

## The fix

When `findmatch()` returns `P_BAR-50`, check whether the token is in fact `P_BAR-50x100`, using the existing case-insensitive whole-string comparison `strcomp()`:

```diff
     grateType = findmatch(tok[4], GrateTypeWords);
     if (grateType < 0) return error_setInpError(ERR_KEYWORD, tok[4]);
 
+    // --- findmatch() stops at "P_BAR-50", a prefix of "P_BAR-50x100"
+    if (grateType == P50 && strcomp(tok[4], GrateTypeWords[P50x100]))
+        grateType = P50x100;
+
```

Only decks that use `P_BAR-50x100` change. No deck in the regression suite or in the 6.0.0 examples uses it. There is no 6.0.0 patch.
