# IO-19: Land use coverages above 100 % or below 0 % are accepted and scale pollutant loads

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Invalid input accepted without a warning. A land use given 160 % of a subcatchment produces 1.6 times the EMC washoff load (31.0 lb instead of 19.4 lb in the test); two land uses at 60 % and 70 % produce 1.3 times. Buildup is scaled the same way. The quality continuity error stays 0.000 %, so nothing in the report points to it. Negative percentages are accepted too |
| **Reached from** | `[COVERAGES]` rows; typically coverage tables pasted or generated with totals above 100 % |
| **5.3.0** | `subcatch_readLanduseParams()` in [`src/legacy/engine/subcatch.c:334`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L334) |
| **5.2.4** | Same code, [`src/solver/subcatch.c:317`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L317) |
| **6.0.0** | Reproduces: `handle_coverages()` stores the number unchecked ([`src/engine/input/handlers/QualityHandler.cpp:268`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/QualityHandler.cpp#L268)) |
| **Since** | Every release in the repository (initial commit, 2014) |
| **Fix** | Reject a percent outside 0–100, or one that takes the total above 100 %, with ERROR 211: [`IO-19_swmm530.patch`](IO-19_swmm530.patch), [`IO-19_swmm600.patch`](IO-19_swmm600.patch) |

## The problem

The input reference defines the `[COVERAGES]` percent as the "percent of the subcatchment's area covered by the land use". Each value therefore lies between 0 and 100, and the land uses of one subcatchment can cover at most 100 % of it together (any remainder simply has no land use). None of the three engines checks either rule.

The test subcatchment (10 ac, 1 in of rain) has two land uses, RES and COM, both with an event mean concentration of 100 mg/L of TSS and no buildup function, so the washoff load is the runoff volume times 100 mg/L times the covered fraction:

| `[COVERAGES]` | 5.3.0 and 6.0.0 washoff (lb) | Ratio |
|---|---|---|
| RES 60, COM 40 | 19.376 | 1.00 |
| RES 160 | 31.002 | 1.60 |
| RES 60, COM 70 | 25.189 | 1.30 |
| RES 100, COM −50 | 19.376 | 1.00 |

(5.2.4 prints 19.378, 31.005, 25.191 and 19.378.) All four decks run without an error or warning, and the Runoff Quality Continuity error is 0.000 % in every case, because the excess load is booked as surface buildup. The negative coverage in the last row is accepted as well; in this test COM's −50 % happens to contribute nothing.

## Why it happens

```c
// src/legacy/engine/subcatch.c, subcatch_readLanduseParams()
        if ( ! getDouble(tok[k], &f) )
            return error_setInpError(ERR_NUMBER, tok[k]);

        // --- store land use fraction in subcatch's landFactor property
        Subcatch[j].landFactor[m].fraction = f/100.0;
```

The only check is that the token is a number, and no later validation (`subcatch_validate()`, `landuse_*`) looks at the fractions. They are used as area weights: initial buildup uses `fArea = f * area` ([`landuse.c:381`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/landuse.c#L381)) and washoff `landuseArea = landFactor[i].fraction * area` ([`landuse.c:571`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/landuse.c#L571)), so a land use at 160 % washes off from 1.6 times the subcatchment's area.

6.0.0's handler does the same: `ctx.subcatches.coverage[...] = to_double(tok[2]);`. [IO-18](../IO-18-pct-zero-over-100/) is the same kind of defect for PctZero in `[SUBAREAS]`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-19_split-100.inp`](IO-19_split-100.inp) | Valid: RES 60 %, COM 40 % |
| [`IO-19_single-160.inp`](IO-19_single-160.inp) | RES 160 % |
| [`IO-19_total-130.inp`](IO-19_total-130.inp) | RES 60 %, COM 70 % (130 % in total) |
| [`IO-19_negative.inp`](IO-19_negative.inp) | RES 100 %, COM −50 % |
| [`IO-19_test.c`](IO-19_test.c) | Runs the four decks through the legacy toolkit; the valid one must run and the three invalid ones must be rejected when the project is opened. Prints the washoff load from each report that was written |
| [`IO-19_test6.c`](IO-19_test6.c) | The same check through the 6.0.0 API |

```sh
tools/run-test.sh IO-19            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-19 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 6.0.0 prints the same numbers):

```
TSS washoff from S1 (EMC 100 mg/L on every land use):
  deck         coverage (%)    error    washoff (lbs)
  split-100    60 + 40             0     19.376 (1.00x)
  single-160   160                 0     31.002 (1.60x)
  total-130    60 + 70             0     25.189 (1.30x)
  negative     100 + (-50)         0     19.376 (1.00x)
  (correct: split-100 runs; the other three are rejected)
FAIL: 3 of 3 decks with invalid coverages ran without an error (160 % gives 31.002 lbs, 1.60x the load of 100 %)
IO-19 5.3.0 base: FAIL
```

**With the fix**, 5.3.0 rejects the three decks with ERROR 200 (`ERROR 211: invalid number 160 at line 44 of [COVERAGE] section`, `... 70 at line 45`, `... -50 at line 45` in the reports); 6.0.0 returns its input-error code 5 with the same ERROR 211 messages:

```
  split-100    60 + 40             0     19.376 (1.00x)
  single-160   160               200         rejected
  total-130    60 + 70           200         rejected
  negative     100 + (-50)       200         rejected
  (correct: split-100 runs; the other three are rejected)
PASS: coverages outside 0-100 % or above 100 % in total are rejected
IO-19 5.3.0 patched: PASS
IO-19 6.0.0 patched: PASS
```

## The fix

Check the value, then the running total of the subcatchment's fractions after storing it (coverages may be split over several lines, and a land use listed twice keeps its last value, so the total is summed from the stored fractions):

```diff
-        if ( ! getDouble(tok[k], &f) )
+        if ( ! getDouble(tok[k], &f) || f < 0.0 || f > 100.0 )
             return error_setInpError(ERR_NUMBER, tok[k]);

         // --- store land use fraction in subcatch's landFactor property
         Subcatch[j].landFactor[m].fraction = f/100.0;
+
+        // --- land uses can cover at most 100% of the subcatchment
+        //     (0.1% is allowed for rounded percentages)
+        total = 0.0;
+        for ( i = 0; i < Nobjects[LANDUSE]; i++ )
+            total += Subcatch[j].landFactor[i].fraction;
+        if ( total > 1.001 ) return error_setInpError(ERR_NUMBER, tok[k]);
```

The 0.1 % margin accepts totals such as 33.34 + 33.33 + 33.34 = 100.01 from rounded tables; it lets through at most 0.1 % of extra load. The 6.0.0 patch applies the same rule in `handle_coverages()` and reports the same ERROR 211.

**Effect on other models.** Valid decks are unaffected. In the regression suite only `examples/Example1.inp` and `update_v5111/events_example.inp` have `[COVERAGES]`; every subcatchment in them totals exactly 100 %.

**Related, not fixed here.** 6.0.0's `handle_coverages()` reads only the first land use and percent of each line. The input reference allows several pairs per line (`Subcat Landuse Percent Landuse Percent ...`); with `S1 RES 60 COM 40` on one line 6.0.0 gives 11.626 lb instead of 19.376 lb. The EPA GUI writes one pair per line, which is why the test decks do.
