# API-07: The subcatchment buildup getter adds up the land uses' buildup densities

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | With more than one land use on a subcatchment, `swmm_SUBCATCH_POLLUTANT_BUILDUP` returns the sum of each land use's own buildup per acre instead of the subcatchment's buildup per acre. In the test, 130 lb on 10 ac (13 lb/ac) reads as 60 lb/ac; with N land uses at the same density the value is N times too high. A calibration or control loop that reads it (and adds buildup through [API-06](../API-06-external-buildup-per-step/)'s setter) works from the wrong number. Nothing warns. |
| **Reached from** | `swmm_getValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_POLLUTANT_BUILDUP, ...)` on a subcatchment with two or more land uses |
| **5.3.0** | `getSubcatchValue()` in [`src/legacy/engine/swmm5.c:2661`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2661) |
| **5.2.4** | Not affected: no such property |
| **6.0.0** | Not applicable: there is no runtime buildup getter; the buildup the engine computes on the test deck is right (130 lb) |
| **Since** | 5.3.0, fork commit 5b87a2b5 ("WIP API bindings for pollutants", December 2024) |
| **Fix** | Divide the total buildup by the total land-use area: [`API-07_swmm530.patch`](API-07_swmm530.patch) |

## The problem

SWMM keeps a subcatchment's buildup per land use, as a mass on that land use's share of the area. The API offers it as one number per subcatchment and pollutant, in mass per unit area (the Python wrapper documents "Pollutant buildup on the subcatchment surface (mass/area)").

The test deck has a 10 ac subcatchment split into three land uses: L1 on 20% (2 ac), L2 on 30% (3 ac), L3 on 50% (5 ac). After 5 dry days the exponential buildup of P1 is at its maximum: 50 lb/ac on L1 (100 lb), 10 lb/ac on L2 (30 lb), and none on L3. The report's Initial Buildup is 130 lb, so the subcatchment carries 13 lb/ac. The getter returns 60 lb/ac, which is 50 + 10 + 0.

## Why it happens

The loop divides each land use's buildup by that land use's area and adds the quotients:

```c
// src/legacy/engine/swmm5.c, getSubcatchValue()
case swmm_SUBCATCH_POLLUTANT_BUILDUP:
    ...
    prop_results = 0.0;

    for (i = 0; i < Nobjects[LANDUSE]; i++)
    {
        sub_catch_area = subcatch->area * UCF(LANDAREA) * subcatch->landFactor[i].fraction;

        if (sub_catch_area > 0)
        {
            prop_results += subcatch->landFactor[i].buildup[pollutantIndex] / sub_catch_area;
        }
    }

    return prop_results;
```

That is sum(B_i / A_i). The buildup per unit area is sum(B_i) / sum(A_i). The two agree only when one land use covers the subcatchment.

## How to reproduce

| File | What it is |
|---|---|
| [`API-07_three-land-uses.inp`](API-07_three-land-uses.inp) | S1, 10 ac: L1 20% (EXP buildup, max 50 lb/ac), L2 30% (EXP, max 10 lb/ac), L3 50% (no buildup); DRY_DAYS 5 with a rate constant of 10/day, so the buildup starts at its maximum; no rain, 1 h |
| [`API-07_test.c`](API-07_test.c) | Reads `swmm_SUBCATCH_POLLUTANT_BUILDUP` for S1 after `swmm_start` and after the first step; expects 13 lb/ac within 1%. 5.2.4 has no such API and prints PASS |
| [`API-07_test6.c`](API-07_test6.c) | 6.0.0 has no buildup getter; runs the deck and checks the report's Initial Buildup (130 lb) |

The deck gives each land use its own `[COVERAGES]` line, because 6.0.0 reads only the first land use of a multi-pair line (reported separately).

```sh
tools/run-test.sh API-07            # 5.3.0: FAIL; 5.2.4 and 6.0.0: PASS
tools/run-test.sh API-07 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0:

```
S1: L1 2 ac x 50 lb/ac + L2 3 ac x 10 lb/ac + L3 5 ac x 0 = 130 lb on 10 ac
SUBCATCH_POLLUTANT_BUILDUP(S1, P1)  API (lb/ac)     expected
after swmm_start                        60.0000      13.0000
after the first step                    60.0000      13.0000
FAIL: the buildup getter returns 60.0000 lb/ac for 130 lb on 10 ac (13 lb/ac)
API-07 5.3.0 base: FAIL
```

6.0.0:

```
6.0.0 has no subcatchment buildup getter; report Initial Buildup = 130.000 lb (13.0000 lb/ac on 10 ac), expected 130 lb (13 lb/ac); run code 0
PASS: no buildup getter in 6.0.0, and the buildup it computes is 13 lb/ac
API-07 6.0.0 base: PASS
```

**With the fix**, 5.3.0:

```
after swmm_start                        13.0000      13.0000
after the first step                    13.0000      13.0000
PASS: the buildup getter returns the subcatchment's buildup per unit area (13 lb/ac)
API-07 5.3.0 patched: PASS
```

## The fix

Add up the masses and the areas separately and divide once:

```diff
             if (sub_catch_area > 0)
             {
-                prop_results += subcatch->landFactor[i].buildup[pollutantIndex] / sub_catch_area;
+                prop_results += subcatch->landFactor[i].buildup[pollutantIndex];
+                landuseArea += sub_catch_area;
             }
         }
 
+        // --- total buildup per unit area covered by land uses
+        if (landuseArea > 0.0)
+            prop_results /= landuseArea;
         return prop_results;
```

The area is the area covered by land uses, which is the subcatchment area when the coverages add up to 100%. Dividing by the covered area keeps every single-land-use result exactly as before (also when that land use covers less than 100%), and matches `[LOADINGS]`, whose initial buildup per unit area is spread over the covered area only (`landuse_getInitBuildup`). A subcatchment with no land use returns 0, as before. Only the API value changes; runs driven by an input file are unchanged.
