# CON-20: A vegetative swale infiltrates at the soil's rate times the subcatchment's pervious fraction

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | In a subcatchment that uses Horton or Curve Number infiltration, a vegetative swale infiltrates at the pervious area's rate multiplied by (1 - imperviousness). In the test, with a soil that takes 1.0 in/hr, a full swale infiltrates 0.500 in/hr at 50 % impervious and 0.100 in/hr at 90 % impervious, but 1.000 in/hr at 100 % impervious. Over the storm the 90 % swale infiltrates 0.21 in instead of 2.13 in. Swales in highly impervious catchments, where they are most used, are hit hardest. Nothing warns. |
| **Reached from** | `[LID_USAGE]` of a `VS` control in a subcatchment with 0 < %Imperv < 100 and a non-LID area, with `HORTON`, `MODIFIED_HORTON` or `CURVE_NUMBER` infiltration (with Green-Ampt the swale has its own infiltration state and is not affected) |
| **5.3.0** | `findNativeInfil()` in [`src/legacy/engine/lid.c:1755`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lid.c#L1755); the rate becomes the swale's infiltration in `lidproc_getOutflow()` ([`lidproc.c:283`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L283)) and `swaleFluxRates()` ([`lidproc.c:1124`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L1124)) |
| **5.2.4** | Same code, [`src/solver/lid.c:1712`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lid.c#L1712) |
| **6.0.0** | Reproduces with the same numbers: `SWMMEngine::stepRunoff()` in [`src/engine/core/SWMMEngine.cpp:2556`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L2556) copies the legacy formula |
| **Since** | 5.1.001, the oldest source in the repository history |
| **Fix** | Divide the pervious infiltration volume by the pervious area, not the whole non-LID area: [`CON-20_swmm530.patch`](CON-20_swmm530.patch), [`CON-20_swmm600.patch`](CON-20_swmm600.patch) |

## The problem

A vegetative swale has only a surface layer. Its infiltration into the native soil is not modelled by the swale itself; the Reference Manual Vol. III (6.2.8) says:

> Because the swale is assumed to sit on top of the subcatchment's native soil, the infiltration rate f1 is the same value computed for the pervious area of the subcatchment by SWMM's runoff module.

The test has three 10-acre subcatchments, 50 %, 90 % and 100 % impervious, each with a 20000 ft2 swale (top width 20 ft, 12 in deep) that takes all the impervious runoff. Horton infiltration has MaxRate = MinRate = 1 in/hr, so the soil takes 1 in/hr at any time. 3 in/hr of rain falls for 2 hours. At 01:00 the pervious area infiltrates at 1 in/hr and every swale is full, so every swale should infiltrate at 1 in/hr. The LID report files show:

| %Imperv | Swale infiltration at 01:00 (in/hr) | Swale infiltration over the run (in, LID Performance Summary) |
|---|---|---|
| 50 | 0.500 | 1.22 |
| 90 | 0.100 | 0.21 |
| 100 | 1.000 | 3.55 |

The rate is the soil's rate times the pervious fraction, and it jumps from 0.1 to 1.0 in/hr between 90 % and 100 % impervious. A designer who sizes a swale for a highly impervious site, the usual case, gets a fraction of the infiltration the soil parameters imply.

## Why it happens

`findNativeInfil()` sets the native infiltration rate from the infiltration volume of the subcatchment's non-LID area in the current step:

```c
// src/legacy/engine/lid.c, findNativeInfil()
    //... subcatchment has non-LID pervious area
    nonLidArea = Subcatch[j].area - Subcatch[j].lidArea;
    if ( nonLidArea > 0.0 && Subcatch[j].fracImperv < 1.0 )
    {
        NativeInfil = Vinfil / nonLidArea / tStep;
    }

    //... otherwise find infil. rate for the subcatchment's rainfall + runon
    else
    {
        NativeInfil = infil_getInfil(j, tStep,
                                     Subcatch[j].rainfall,
                                     Subcatch[j].runon,
                                     getSurfaceDepth(j));
    }
```

`Vinfil` is accumulated in `getSubareaRunoff()` as `infil * area * tStep`, where `area` is the pervious subarea, `nonLidArea * subArea[PERV].fArea`. Only the pervious subarea infiltrates. Dividing by the whole non-LID area gives `infil * fArea[PERV]`, the pervious rate times the pervious fraction. The comment and the `fracImperv < 1.0` test show the intent was the pervious area's rate. With no pervious area the `else` branch evaluates the infiltration model directly, which is why 100 % impervious gives the full rate.

For a swale without its own Green-Ampt state, `lidproc_getOutflow()` uses this rate as the surface infiltration (`else SurfaceInfil = infil;`), and `swaleFluxRates()` turns it into the exfiltration `StorageExfil = SurfaceInfil * surfArea`. No other LID type uses it.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-20_imperv50.inp`](CON-20_imperv50.inp) | 10-acre subcatchment, 50 % impervious, Horton 1 in/hr constant; 20000 ft2 swale, top width 20 ft, 12 in deep, VegFrac 0, taking all impervious runoff; 3 in/hr of rain for 2 hours; writes `CON-20_lid50.txt` |
| [`CON-20_imperv90.inp`](CON-20_imperv90.inp) | The same, 90 % impervious |
| [`CON-20_imperv100.inp`](CON-20_imperv100.inp) | The same, 100 % impervious (no pervious area) |
| [`CON-20_test.c`](CON-20_test.c) | Runs the three decks through the legacy toolkit and reads the swale's surface infiltration rate at 01:00 from each LID report file; it must be 1.0 in/hr within 0.05 in/hr |
| [`CON-20_test6.c`](CON-20_test6.c) | The same against the 6.0.0 C API |

```sh
tools/run-test.sh CON-20            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-20 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4, 5.3.0 and 6.0.0 print the same):

```
Swale at 01:00, pervious area infiltrating at 1.000 in/hr
%Imperv  swale infil (in/hr)  exfil (in/hr)  level (in)
     50                0.500          0.500      12.000
     90                0.100          0.100      12.000
    100                1.000          1.000      12.000
FAIL: at 50 % impervious the swale infiltrates 0.500 in/hr while the pervious area infiltrates 1.000 in/hr
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Swale at 01:00, pervious area infiltrating at 1.000 in/hr
%Imperv  swale infil (in/hr)  exfil (in/hr)  level (in)
     50                1.000          1.000      12.000
     90                1.000          1.000      12.000
    100                1.000          1.000      12.000
PASS: the swale infiltrates at the pervious area's rate at every imperviousness
```

With the fix the swale's infiltration over the run is 2.43 in at 50 % impervious (was 1.22) and 2.13 in at 90 % (was 0.21); 100 % is unchanged at 3.55 in. The runoff continuity errors stay below 0.04 % in 5.3.0.

## The fix

Divide by the pervious subarea, the area `Vinfil` was computed over:

```diff
     if ( nonLidArea > 0.0 && Subcatch[j].fracImperv < 1.0 )
     {
-        NativeInfil = Vinfil / nonLidArea / tStep;
+        NativeInfil = Vinfil / (nonLidArea * Subcatch[j].subArea[PERV].fArea) /
+                      tStep;
     }
```

The 6.0.0 patch makes the same change in `SWMMEngine::stepRunoff()`, with `1.0 - frac_imperv` as the pervious fraction (the value 6.0.0's runoff solver uses for the pervious subarea). The condition `fracImperv < 1.0` already guarantees a non-zero divisor.

What changes for users: swales in Horton and Curve Number subcatchments infiltrate more, by the factor 1 / (1 - imperviousness), and the result no longer jumps at 100 % impervious while it rains.

**What the fix leaves.** The rate is still the pervious area's *actual* infiltration, as the manual specifies, not the soil's capacity under the swale's own ponded water. Once the pervious area has drained after the storm, the swale stops infiltrating although it is still full of water: in the 90 % deck the swale's infiltration drops to 0 at 02:14 while it holds 10.3 in at 02:30, with or without the fix. With 100 % impervious, the `else` branch evaluates the infiltration model with the swale's own ponded depth (`getSurfaceDepth()`), and the swale keeps infiltrating at 1.0 in/hr after the rain. Making the two branches agree needs an infiltration state for the swale, as it already has for Green-Ampt; that is a change of formulation and is not made here. (6.0.0's version of the `else` branch, `RunoffSolver::nativeInfilFullLid()`, passes the pervious area's ponded depth instead of the swale's, so its 100 % impervious swale stops infiltrating at 02:01 where 5.3.0's continues; that parity difference is separate from this issue.)

Effect on other models: the only regression deck with a swale, `examples/Example4.inp`, uses Green-Ampt infiltration and is not affected. The 5.3.0 and 6.0.0 CLIs built with the patch give binary output files identical to the unpatched builds for it and for `update_v5111/rain_garden.inp`.
