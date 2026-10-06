# NUM-32: Evaporation and infiltration are both charged to the same ponded water

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | When ponded water on a pervious area is used up within one runoff step, infiltration takes all of it and evaporation is charged on top. Up to evaporation rate x step of water is lost that never existed, and it also reaches the groundwater module as infiltration. In the test, 0.625 in of losses from 0.500 in of rain: a -25 % runoff continuity error with a 6-hour dry step. Only the continuity error shows it. |
| **Reached from** | Any pervious area with evaporation and ponded water that drains within one step, worst with a long `DRY_STEP`. Every infiltration method. |
| **5.3.0** | `getSubareaRunoff()` in [`src/legacy/engine/subcatch.c:972`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L972) and `getSubareaInfil()` at [`subcatch.c:1031`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L1031) |
| **5.2.4** | Same code, [`src/solver/subcatch.c:939`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L939) and [`subcatch.c:998`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L998) |
| **6.0.0** | Reproduces with the same numbers: `RunoffSolver::execute()` in [`src/engine/hydrology/Runoff.cpp:432`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L432) and [`Runoff.cpp:457`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L457) |
| **Since** | Every release (5.0.022 has the same sequence) |
| **Fix** | Infiltrate only the ponded water left after evaporation: [`NUM-32_swmm530.patch`](NUM-32_swmm530.patch), [`NUM-32_swmm600.patch`](NUM-32_swmm600.patch) |

## The problem

In the test deck 1 in/hr of rain falls for 30 minutes on a pervious area that infiltrates 0.5 in/hr. About 0.24 in stays ponded in the 0.3 in of depression storage, so nothing runs off. Evaporation is 0.5 in/day. When the rain stops the runoff step becomes the 6-hour dry step, and in that one step the ponded water is used up:

- evaporation is charged at its full rate: 0.5 in/day x 6 h = 0.125 in;
- infiltration could take 3 in in 6 h, so it takes all the ponded water, about 0.24 in.

That is 0.365 in of losses from 0.24 in of water. The ponded depth is set to zero and both volumes are booked. Over the event the report shows 0.134 in of evaporation and 0.491 in of infiltration from 0.500 in of rain, with a runoff continuity error of -25.000 %. The extra 0.125 in is exactly the evaporation of the dry step.

The overdraw is at most the evaporation rate times the step, so it grows with `DRY_STEP`. In a real model it happens after every storm that leaves water in depression storage. The infiltrated surplus also goes to the groundwater module.

## Why it happens

`getSubareaRunoff()` first charges evaporation to the ponded water, then asks for the infiltration rate with the full ponded depth:

```c
// src/legacy/engine/subcatch.c, getSubareaRunoff()
surfMoisture = subarea->depth / tStep;
surfEvap = MIN(surfMoisture, evap);                    // evaporation from ponded water
if ( i == PERV ) infil = getSubareaInfil(j, subarea, precip, tStep);
...
Vevap += surfEvap * area * tStep;
Vinfil += infil * area * tStep;
...
if ( surfEvap + infil >= surfMoisture )
{
    subarea->depth = 0.0;                              // neither loss is reduced
}

// src/legacy/engine/subcatch.c, getSubareaInfil()
infil = infil_getInfil(j, tStep, precip,
                       subarea->inflow, subarea->depth);   // the full depth
```

Every infiltration method limits the rate to the water available, `rainfall + runon + depth / tStep`, so infiltration can take water that evaporation has already used. The infiltration routines document the intended input. From `horton_getInfil()`:

```c
//           irate = net "rainfall" rate (ft/sec),
//                 = rainfall + snowmelt + runon - evaporation
```

6.0.0 has the same sequence (`surfEvap = std::min(surfMoisture, evapRate)`, then `infilGetInfil(ctx, i, precip, runon, depth, dt, ...)`, then the same `surfEvap + infil >= surfMoisture` test).

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-32_pond-dries-in-one-step.inp`](NUM-32_pond-dries-in-one-step.inp) | 1 acre pervious, Horton 0.5 in/hr (constant), S-Perv 0.3 in, evaporation 0.5 in/day, 1 in/hr for 30 min; WET_STEP 5 min, DRY_STEP 6 h |
| [`NUM-32_test.c`](NUM-32_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0) and checks that evaporation + infiltration + runoff + final storage equals the rain |
| [`NUM-32_test6.c`](NUM-32_test6.c) | The same with the 6.0.0 engine API |

Nothing in the deck runs off, so no other source of continuity error is involved.

```sh
tools/run-test.sh NUM-32            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-32 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** all three engines print:

```
Total precipitation (in)   0.500
Evaporation loss (in)      0.134
Infiltration loss (in)     0.491
Surface runoff (in)        0.000
Final storage (in)         0.000
Losses + runoff + storage  0.625   (must equal the rain)
Continuity error (%)      -25.000   (swmm_getMassBalErr: -25.000)
FAIL: evaporation 0.134 in + infiltration 0.491 in + runoff 0.000 in + storage 0.000 in = 0.625 in from 0.500 in of rain (continuity error -25.000 %)
NUM-32 5.2.4 base: FAIL
NUM-32 5.3.0 base: FAIL
NUM-32 6.0.0 base: FAIL
```

**With the fix** 5.3.0 and 6.0.0 both print:

```
Evaporation loss (in)      0.134
Infiltration loss (in)     0.366
Losses + runoff + storage  0.500   (must equal the rain)
Continuity error (%)       0.000   (swmm_getMassBalErr: 0.000)
PASS: evaporation 0.134 in + infiltration 0.366 in = rain 0.500 in, continuity error 0.000 %
NUM-32 5.3.0 patched: PASS
NUM-32 6.0.0 patched: PASS
```

## The fix

Give the infiltration routine the ponded depth that is left after evaporation. `getSubareaInfil()` gets the evaporation charged to the ponded water as an extra argument:

```diff
-    if ( i == PERV ) infil = getSubareaInfil(j, subarea, precip, tStep);
+    if ( i == PERV ) infil = getSubareaInfil(j, subarea, precip, surfEvap, tStep);
 ...
-    // --- compute infiltration rate 
-    infil = infil_getInfil(j, tStep, precip,
-                           subarea->inflow, subarea->depth);
+    // --- compute infiltration rate from the ponded water left after
+    //     evaporation, so the two losses never exceed the water available
+    infil = infil_getInfil(j, tStep, precip, subarea->inflow,
+                           MAX(0.0, subarea->depth - evap * tStep));
```

Since `surfEvap <= depth / tStep`, infiltration is now limited to `surfMoisture - surfEvap`, so the two losses can no longer add up to more than the water available. The fix is made at the input to the infiltration routine rather than by cutting `infil` afterwards, so the Horton, Green-Ampt and curve-number states advance only by the water actually infiltrated. Evaporation keeps the priority it already had. With a long dry step that still gives it a larger share than it would have in reality (in the test the 0.24 in would infiltrate in about 30 minutes), but the total is now right. The 6.0.0 patch makes the same change at its `infilGetInfil()` call.

**Effect on other models.** Horton and curve-number results change only in steps where the losses would have exceeded the water available. Green-Ampt also uses the ponded depth in its suction head term (`S + depth`), so its rate changes by a negligible amount in every step with ponded water and evaporation. Regression decks with evaporation, base -> patched, 5.3.0 and 6.0.0 identical to each other:

| Deck | Infiltration | Runoff continuity error, % | Other change |
|---|---|---|---|
| `swc/swc1.inp` (Green-Ampt) | 145.582 -> 144.793 in | -0.515 -> -0.090 | surface runoff 22.351 -> 22.419 in |
| `examples/Example4.inp` (Green-Ampt) | unchanged | -0.044 -> -0.039 (6.0.0: -0.038 -> -0.033) | none |
| `user/user2.inp` (Horton) | 218.602 -> 218.601 ac-ft | unchanged | none |
| `swc/swc12.inp`, `user/user1.inp`, `user/user5.inp` | identical reports | | |
