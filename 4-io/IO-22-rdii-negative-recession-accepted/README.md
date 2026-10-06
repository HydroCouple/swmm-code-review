# IO-22: A negative RDII recession limb ratio K is accepted, and K < -1 crashes the legacy engines

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | No message for a negative K. With -1 < K < 0 the RDII volume is (1+K) times what R says (half for K = -0.5); with K = -1 the hydrograph is silently dropped; with K < -1 5.2.4 and 5.3.0 write through a null pointer (normally a segmentation fault) and 6.0.0 silently produces no RDII from it |
| **Reached from** | A [HYDROGRAPHS] line with T > 0 and K < 0, e.g. a sign typo |
| **5.3.0** | `validateRdii()` in [`src/legacy/engine/rdii.c:873`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rdii.c#L873); null write in `getRainfall()` at [`rdii.c:1255`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rdii.c#L1255) |
| **5.2.4** | Same code, [`src/solver/rdii.c:836`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/rdii.c#L836) and [`rdii.c:1218`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/rdii.c#L1218) |
| **6.0.0** | Reproduces the silent results (no crash): the [HYDROGRAPHS] check in [`src/engine/core/SWMMEngine.cpp:8351`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L8351) copies legacy and its comment records a model that has K = -0.42 |
| **Since** | Every release in the repository history (initial commit, 2014) |
| **Fix** | Report ERROR 151 when the base time is shorter than the time to peak: [`IO-22_swmm530.patch`](IO-22_swmm530.patch), [`IO-22_swmm600.patch`](IO-22_swmm600.patch) |

## The problem

The input reference defines the RTK parameters of a unit hydrograph as R, the fraction of rainfall that becomes RDII, T, the time to peak, and K, "the ratio of the duration of the hydrograph's recession limb to the time to peak (T) making the hydrograph time base equal to T*(1+K) hours. The area under each unit hydrograph is 1 inch (or mm)." A duration ratio cannot be negative, and with K < 0 no triangle of base T*(1+K) and peak at T exists.

SWMM accepts it anyway. With R = 0.1, T = 1 h and 1.0 in of rain on 100 ac, RDII must be R x P x A = 36,300 ft3:

| K | 5.2.4 / 5.3.0 | 6.0.0 |
|---|---|---|
| -0.5 | 18,151 ft3, half | 18,151 ft3, half |
| -1 | 0 | 0 |
| -2 | null-pointer write in `getRainfall()` | 0 |

None of these prints an error or a warning. A sign typo in K therefore gives a plausible but wrong RDII hydrograph, or none. 6.0.0's own validation code mentions a real model with K = -0.42 (`CHES_47_SEP`), whose short-term RDII is 58% of what its R implies.

## Why it happens

`setUnitHydParams()` stores the time to peak and the base time without looking at K, and `validateRdii()` only rejects a negative time to peak. A zero base time is taken to mean that the response is not used:

```c
// src/legacy/engine/rdii.c, setUnitHydParams()
        tBase = t * (1.0 + k);                              // hours
        UnitHyd[j].tPeak[m][i] = (long)(t * 3600.);         // seconds
        UnitHyd[j].tBase[m][i] = (long)(tBase * 3600.);     // seconds

// src/legacy/engine/rdii.c, validateRdii()
                // --- if no base time then UH doesn't exist
                if ( UnitHyd[j].tBase[m][k] == 0 ) continue;
                ...
                // --- can't have negative UH parameters
                if ( UnitHyd[j].tPeak[m][k] < 0.0 )
                {
                    report_writeErrorMsg(ERR_UNITHYD_TIMES, UnitHyd[j].ID);
                }
```

- **-1 < K < 0.** `getUnitHydOrd()` returns 0 for `t >= tBase`, so the hydrograph stops part-way up its rising limb. The peak ordinate is still 2/tBase, so the area is tBase/tPeak = 1 + K instead of 1.
- **K = -1.** The base time is 0 and the response is skipped as if it had not been entered.
- **K < -1.** The base time is negative. `getMaxPeriods()` computes `tBase / rainInterval + 1` past-rainfall periods, which is negative (-3 in the test), keeps the maximum with 0, and `allocRdiiMemory()` allocates no array. `getRainfall()` still stores the rain of every interval in `pastRain[0]`, through a null pointer.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-22_negative-k.inp`](IO-22_negative-k.inp) | 1.0 in in the first hour on a 100 ac sewershed, UH R = 0.1, T = 1 h, K = -0.5 |
| [`IO-22_test.c`](IO-22_test.c) | Runs the deck with K = -0.5, -1, -2 (rewriting the K token) and +0.5 as a control through the legacy toolkit; integrates node J1's lateral inflow (RDII only). Each negative K must be rejected; the control must give R x P x A within 1% |
| [`IO-22_test6.c`](IO-22_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh IO-22            # 5.2.4, 5.3.0: CRASH; 6.0.0: FAIL
tools/run-test.sh IO-22 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.3.0 (5.2.4 is the same, at `rdii.c:1218`) runs the first two cases and stops on the third:

```
K       error  RDII volume (ft3)  ratio to R*P*A
-0.5        0       18150.7       0.500
-1          0           0.0       0.000
../src/src/legacy/engine/rdii.c:1255:17: runtime error: applying zero offset to null pointer
    #0 0x7fd58737ad10 in getRainfall .../src/legacy/engine/rdii.c:1255:46
    #1 0x7fd58737ad10 in createRdiiFile .../src/legacy/engine/rdii.c:808:13
    #2 0x7fd5873764f2 in rdii_openRdii .../src/legacy/engine/rdii.c:470:35
IO-22 5.3.0 base: CRASH
```

6.0.0:

```
K       error  RDII volume (ft3)  ratio to R*P*A
-0.5        0       18150.7       0.500
-1          0           0.0       0.000
-2          0           0.0       0.000
0.5         0       36301.3       1.000
FAIL: 3 of 4 cases wrong: a negative K must be rejected, K = 0.5 must run and give R*P*A
IO-22 6.0.0 base: FAIL
```

**With the fix** every negative K stops the run with ERROR 151 (`a Unit Hydrograph in set UH1 has invalid time base.`; 6.0.0's API returns its parse-error code 5 with that message):

```
K       error  RDII volume (ft3)  ratio to R*P*A
-0.5      151  (rejected)
-1        151  (rejected)
-2        151  (rejected)
0.5         0       36301.3       1.000
PASS: negative K is rejected with an input error; K = 0.5 gives R*P*A
IO-22 5.3.0 patched: PASS
IO-22 6.0.0 patched: PASS
```

## The fix

A base time shorter than the time to peak is reported as ERROR 151, and a zero base time only counts as "no hydrograph" when the time to peak is zero as well, which is how unused responses are entered (T = 0):

```diff
                 // --- if no base time then UH doesn't exist
-                if ( UnitHyd[j].tBase[m][k] == 0 ) continue;
+                //     (unless it has a time to peak, i.e., K = -1)
+                if ( UnitHyd[j].tBase[m][k] == 0 &&
+                     UnitHyd[j].tPeak[m][k] == 0 ) continue;
 ...
                 // --- can't have negative UH parameters
-                if ( UnitHyd[j].tPeak[m][k] < 0.0 )
+                //     (a base time shorter than the time to peak means K < 0)
+                if ( UnitHyd[j].tPeak[m][k] < 0.0 ||
+                     UnitHyd[j].tBase[m][k] < UnitHyd[j].tPeak[m][k] )
```

For K >= 0, T*(1+K) >= T in floating point, so no valid input is rejected; K = 0 (a hydrograph with no recession limb) is still accepted. Because `createRdiiFile()` returns before processing rainfall when validation fails, the null-pointer write can no longer be reached. The 6.0.0 patch makes the same change in its [HYDROGRAPHS] validation. As for the existing checks in `validateRdii()`, 5.3.0 writes the message once for each month an `ALL` line covers; 6.0.0 writes it once.

**Effect on other models.** No regression-suite deck uses RDII, and the 174 hydrograph lines of the Richmond CSO model in 6.0.0's unit-test data all have K > 0. A model with a negative K, such as the `CHES_47_SEP` model mentioned in 6.0.0's source, now stops with ERROR 151 instead of running with a truncated hydrograph; its K has to be corrected.

A related parsing defect on the same lines is not fixed here: when an initial-abstraction value is not a number, 5.2.4 and 5.3.0 name the token before it in the ERROR 211 message (`tok[i+2]` instead of `tok[i+3]` in `rdii_readUnitHydParams()`), and 6.0.0 accepts it silently.
