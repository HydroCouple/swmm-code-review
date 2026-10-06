# API-05: Ponded concentration and link pollutant load come out of the toolkit in the wrong units

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | In US units, rain at 10 mg/L ponded on a subcatchment is reported as 12,335,300 mg/L (1.23 million times too high), and setting the ponded concentration to 100 mg/L stores 1.23 million times too little mass. On a dry surface the getter returns NaN. A link's pollutant load reads 325,831 where the report shows 20.327 lb (a factor 16,000). Return codes are 0 and nothing else hints at the problem. |
| **Reached from** | `swmm_getValueExpanded` / `swmm_setValueExpanded` with `swmm_SUBCATCH_POLLUTANT_PONDED_CONCENTRATION`, and `swmm_getValueExpanded` with `swmm_LINK_POLLUTANT_LOAD` (and the Python wrappers built on them, e.g. `set_ponded_concentration`) |
| **5.3.0** | `setSubcatchValue()` in [`src/legacy/engine/swmm5.c:2114`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2114), `getSubcatchValue()` at [`:2688`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2688), `getLinkValue()` at [`:2853`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2853) |
| **5.2.4** | Not affected: it has no expanded getters or setters and no quality properties |
| **6.0.0** | Not affected: there is no ponded-concentration property and no link load getter; `swmm_subcatch_get_ponded_quality()` ([`openswmm_subcatchments_impl.cpp:1077`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_subcatchments_impl.cpp#L1077)) returns the ponded mass, and the test confirms it is in mg |
| **Since** | 5.3.0: the getters in fork commit 5b87a2b5 (December 2024), the ponded-concentration setter, written as the getter's inverse, in 82ce4ede (June 2026) |
| **Fix** | Convert with `LperFT3` (and the pollutant's mass factor for the load), return 0 for a dry surface: [`API-05_swmm530.patch`](API-05_swmm530.patch) |

The related external-buildup property (`swmm_SUBCATCH_EXTERNAL_POLLUTANT_BUILDUP`) is [API-06](../API-06-external-buildup-per-step/).

## The problem

A script that couples SWMM's surface water quality to another model reads the concentration of the water ponded on a subcatchment, or injects a concentration (a spill, a tracer). A script that tracks loads reads the mass a conduit has carried. In 5.3.0 all three values are in internal units that no user would expect.

The test deck is a 10 ac fully impervious subcatchment with 0.10 in of depression storage and no evaporation. It receives 1.00 in of rain at 1 in/hr between 0:15 and 1:15, with pollutant P1 at 10 mg/L in the rain and no buildup or washoff. The ponded water is rain only, so its P1 concentration must be 10 mg/L, and the P1 that leaves through conduit C1 must be the P1 in the 0.90 in of rain that runs off: 10 mg/L x 28.317 L/ft3 x 32,670 ft3 = 9.25e6 mg = 20.395 lb. What the API reports:

| Quantity | API, 5.3.0 | Expected |
|---|---|---|
| S1 ponded P1 concentration at 0:01 (dry surface) | NaN | 0 |
| S1 ponded P1 concentration at 0:45 | 12,335,300 | 10 mg/L |
| S1 runoff P1 concentration at 0:45 (`..._RUNOFF_CONCENTRATION`, for comparison) | 10 | 10 mg/L |
| S1 runoff P2 concentration after setting the ponded P2 concentration to 100 mg/L | 0.0000755 | about 93 mg/L |
| C1 P1 load (`swmm_LINK_POLLUTANT_LOAD`) | 325,831 | 20.395 lb |
| S1 P1 load (`swmm_SUBCATCH_POLLUTANT_TOTAL_LOAD`, for comparison) | 20.340 | 20.395 lb |

The getter and the setter of the ponded concentration are exact inverses of each other, so a set-then-get round trip (the check in the Python test suite) passes even though both are wrong.

## Why it happens

`pondedQual[p]` is a pollutant mass (mg for a MG/L pollutant): `surfqual.c` stores the ponded concentration in mass per ft3 times the ponded volume in ft3.

```c
// src/legacy/engine/surfqual.c, findPondedLoads()
wRain = Pollut[p].pptConcen * LperFT3 * vRain;       // mg/L * L/ft3 * ft3 = mg
...
cPonded = wPonded / Vinflow;                          // mg/ft3
...
Subcatch[j].pondedQual[p] = cPonded * subcatch_getDepth(j) * nonLidArea;   // mg
```

To get mg/L the API must divide by the volume in ft3 and by 28.317 L/ft3. It divides by the volume times `UCF(LANDAREA)`, the ft2-to-acre factor (1/43,560 in US units, 1/107,639 ft2 per ha in SI), so the result is 43,560 x 28.317 = 1.2335e6 times too large in US units (3.05e6 in SI). The setter multiplies by the same wrong factor:

```c
// src/legacy/engine/swmm5.c, getSubcatchValue()
case swmm_SUBCATCH_POLLUTANT_PONDED_CONCENTRATION:
    ...
    return subcatch->pondedQual[pollutantIndex] / (subcatch_getDepth(index) *
           MAX(0.0, subcatch->area - subcatch->lidArea) * UCF(LANDAREA));

// src/legacy/engine/swmm5.c, setSubcatchValue()
pondedVol = subcatch_getDepth(index) *
            MAX(0.0, Subcatch[index].area - Subcatch[index].lidArea) *
            UCF(LANDAREA);
Subcatch[index].pondedQual[pollutantIndex] = value * pondedVol;
```

When the surface is dry the depth is 0 and the getter returns 0/0.

A link's `totalLoad[p]` accumulates concentration x flow x time, i.e. mg/L x ft3. The report converts it with `LperFT3 * Pollut[p].mcf` to lbs or kg, and the subcatchment load is kept in lbs/kg from the start, but the link getter returns the raw sum:

```c
// src/legacy/engine/statsrpt.c, writeLinkLoads()
x = Link[j].totalLoad[p] * LperFT3 * Pollut[p].mcf;
// src/legacy/engine/swmm5.c, getLinkValue()
case swmm_LINK_POLLUTANT_LOAD:
    ...
    return link->totalLoad[pollutantIndex];
```

`LperFT3 * mcf` is 28.317 x 2.203e-6 = 6.24e-5 for a MG/L pollutant in US units, hence the factor 16,000.

## How to reproduce

| File | What it is |
|---|---|
| [`API-05_rain-quality.inp`](API-05_rain-quality.inp) | S1 (10 ac, 100% impervious, 0.10 in depression storage) -> J1 -> C1 -> O1; 1.00 in of rain with P1 at 10 mg/L; P2 has no source; KINWAVE, 4 h, 1-minute steps. A clean 0.1 cfs base flow keeps C1 wet, because a dry conduit gets NaN pollutant values in 5.3.0 ([NUM-01](../../1-numerical/NUM-01-dry-conduit-quality-nan/)) |
| [`API-05_test.c`](API-05_test.c) | Reads the ponded P1 concentration at 0:01 and 0:45, sets the ponded P2 concentration to 100 mg/L at 0:45 and records S1's P2 runoff concentration over the next 5 steps, and reads C1's P1 load at the end. 5.2.4 has no such API and prints PASS |
| [`API-05_test6.c`](API-05_test6.c) | 6.0.0: reads `swmm_subcatch_get_ponded_quality()` on the same deck and checks that mass / (10 mg/L x 28.317 L/ft3 x area) is the ponded depth: 0 when dry, back to the 0.10 in of depression storage at the end |

Tolerances: 5% on the ponded concentration, more than 50 mg/L after setting 100 mg/L (one minute of clean rain on about 0.23 in of ponded water dilutes it by about 7%), 3% on the link load (0.2% of the runoff is still draining after 4 h, and SWMM's lb/mg factor 2.203e-6 is 0.07% below the exact one).

```sh
tools/run-test.sh API-05            # 5.3.0: FAIL; 5.2.4 and 6.0.0: PASS
tools/run-test.sh API-05 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0:

```
quantity                                                              API       expected
S1 P1 ponded conc. at 0:01, dry surface (mg/L)                       -nan              0
S1 P1 ponded conc. at 0:45, rain-only water (mg/L)            1.23353e+07        10.0000
  (S1 P1 runoff conc. at 0:45, mg/L)                                   10        10.0000
S1 P2 ponded conc. set to 100 at 0:45, read back                      100            100
S1 P2 max runoff conc. in the next 5 steps (mg/L)              7.5495e-05           > 50
C1 P1 load (LINK_POLLUTANT_LOAD, lb)                               325831        20.3952
  (S1 P1 load, SUBCATCH_POLLUTANT_TOTAL_LOAD, lb)                 20.3403        20.3952
set returned 0
FAIL: 4 of 4 quality API checks wrong: ponded concentration 1.23353e+07 mg/L (expected 10), runoff after setting 100 mg/L 7.5495e-05 mg/L, C1 load 325831 (expected 20.395 lb)
API-05 5.3.0 base: FAIL
```

The report of the same run lists C1's P1 load as 20.327 lb in the Link Pollutant Load Summary. 6.0.0's ponded mass is consistent with the physics:

```
time           P1 mass (mg)     implied depth (in)
0:01                      0                0.00000
0:45             2.3209e+06                0.22579   (runoff conc. 10.0000 mg/L)
4:00             1.0494e+06                0.10209   (depression storage 0.10 in)
PASS: the ponded quality getter returns the ponded mass in mg (implied depth 0.1021 in at the end vs 0.10 in of depression storage)
API-05 6.0.0 base: PASS
```

**With the fix**, 5.3.0:

```
S1 P1 ponded conc. at 0:01, dry surface (mg/L)                          0              0
S1 P1 ponded conc. at 0:45, rain-only water (mg/L)                     10        10.0000
  (S1 P1 runoff conc. at 0:45, mg/L)                                   10        10.0000
S1 P2 ponded conc. set to 100 at 0:45, read back                      100            100
S1 P2 max runoff conc. in the next 5 steps (mg/L)                 93.1257           > 50
C1 P1 load (LINK_POLLUTANT_LOAD, lb)                              20.3261        20.3952
  (S1 P1 load, SUBCATCH_POLLUTANT_TOTAL_LOAD, lb)                 20.3403        20.3952
set returned 0
PASS: ponded concentration get/set are in mg/L (0 when dry) and the link load is in lb
API-05 5.3.0 patched: PASS
```

The 100 mg/L set at 0:45 now washes off: the run's Surface Runoff of P2 is 48.181 lb instead of 0.000.

## The fix

Use the ponded volume in ft3 and `LperFT3` in both directions, return 0 for a dry surface, and give the link load the report's conversion:

```diff
             pondedVol = subcatch_getDepth(index) *
-                        MAX(0.0, Subcatch[index].area - Subcatch[index].lidArea) *
-                        UCF(LANDAREA);
-            Subcatch[index].pondedQual[pollutantIndex] = value * pondedVol;
+                        MAX(0.0, Subcatch[index].area - Subcatch[index].lidArea);
+            Subcatch[index].pondedQual[pollutantIndex] = value * LperFT3 * pondedVol;
 ...
-        return subcatch->pondedQual[pollutantIndex] / (subcatch_getDepth(index) * MAX(0.0, subcatch->area - subcatch->lidArea) * UCF(LANDAREA));
+        // --- pondedQual is mass; ponded volume (ft3) is zero when dry
+        prop_results = subcatch_getDepth(index) * MAX(0.0, subcatch->area - subcatch->lidArea);
+        if (prop_results <= 0.0)
+            return 0.0;
+        return subcatch->pondedQual[pollutantIndex] / (prop_results * LperFT3);
 ...
     case swmm_LINK_POLLUTANT_LOAD:
 ...
-        return link->totalLoad[pollutantIndex];
+        // --- same conversion as the Link Pollutant Load Summary (lbs or kg)
+        return link->totalLoad[pollutantIndex] * LperFT3 * Pollut[pollutantIndex].mcf;
```

The ponded concentration is now in the pollutant's concentration units (mg/L, ug/L or #/L) and the link load in the report's mass units (lbs or kg, or counts), like `swmm_SUBCATCH_POLLUTANT_TOTAL_LOAD`. Clients that compensated for the old scaling must drop that. Only API values change; runs driven by an input file give identical results.
