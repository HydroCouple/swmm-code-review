# CON-19: Vegetative swales and permeable pavement lose track of surface water when VegFrac > 0

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | With a vegetation volume fraction (VegFrac) above 0, the surface layer's depth rises and falls 1/(1 - VegFrac) times too slowly for the water it gains or loses, while its stored volume is counted with the factor (1 - VegFrac). Part of the water disappears while the surface fills and reappears as it drains. In the test, a swale with VegFrac 0.5 that received 1.930 in holds 0.958 in at the end of the run (runoff continuity error 48.6 %), and a permeable pavement that received 2.000 in reports 1.371 in of storage (31.5 %). A swale also conveys flow as if it were 1/(1 - VegFrac) times larger. Only the continuity error shows it. |
| **Reached from** | `[LID_CONTROLS]` type `VS` or `PP` whose `SURFACE` line has VegFrac > 0 (typical swale values are 0.1 to 0.8) |
| **5.3.0** | `swaleFluxRates()` in [`src/legacy/engine/lidproc.c:1164`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L1164) against [line 1169](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L1169); `pavementFluxRates()` at [line 1042](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L1042) against [line 859](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L859) |
| **5.2.4** | Same code, [`src/solver/lidproc.c:1164`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lidproc.c#L1164) and [`:1042`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lidproc.c#L1042) |
| **6.0.0** | Reproduces: `LIDSolver::swaleFluxRates()` and `pavementFluxRates()` in [`src/engine/hydrology/LID.cpp:1143`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/LID.cpp#L1143) and [`:1085`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/LID.cpp#L1085) copy the legacy code; its runoff continuity table has a separate swale storage error (see below) |
| **Since** | 5.1.001, the oldest source in the repository history, for both. The infiltration trench had the same omission in 5.1.001 and was corrected later; the pavement was not. |
| **Fix** | Divide the surface depth rate by the surface void fraction, as the other LID types do: [`CON-19_swmm530.patch`](CON-19_swmm530.patch), [`CON-19_swmm600.patch`](CON-19_swmm600.patch) |

## The problem

The `SURFACE` line of an LID control has a vegetation volume fraction, VegFrac: the "fraction of the surface storage volume that is filled with vegetation" (input reference). The water on the surface fills only the rest, the void fraction phi = 1 - VegFrac. For the bio-retention cell, rain garden, green roof and infiltration trench, SWMM applies phi both to the stored volume and to the rate at which the depth changes, as in the reference manual's surface layer equation phi1 dd1/dt = i + q0 - e1 - f1 - q1. The vegetative swale and the permeable pavement apply it to the stored volume only.

Two test decks, with no evaporation and no infiltration:

- a 1-acre vegetative swale (top width 20 ft, 12 in deep, 5:1 side slopes, n = 0.24, 0.5 % slope) that receives 2.0 in of rain in the hour the run lasts and still holds water at the end;
- a 1-acre permeable pavement with 3 in of surface storage and no overland flow, whose pavement passes only 0.5 in/hr, so water is ponded on it when the run ends at 1:30. Nothing leaves it, so it must end with all 2.0 in.

The error grows with VegFrac and is the share of the water held at the end that is not counted (5.3.0, runoff continuity table):

| VegFrac | Swale: final storage (in) | Swale: continuity error | Pavement: final storage (in) | Pavement: continuity error |
|---|---|---|---|---|
| 0 | 1.782 | 0.624 % | 2.000 | 0.000 % |
| 0.5 | 0.958 | 48.639 % | 1.371 | 31.461 % |
| 0.8 | 0.394 | 79.542 % | 0.993 | 50.335 % |

The error is not only in the bookkeeping. The swale's depth sets its Manning outflow, and the depth is too low for the water it holds: with VegFrac 0.5 the swale ends at a water level of 3.360 in instead of 5.791 in and has released 0.070 in instead of 0.190 in. A vegetated swale therefore attenuates and delays flow as if it were 1/phi times larger. Over a storm that ends with the swale empty the lost and created water cancel, so long runs show smaller errors, but the hydrograph stays wrong.

## Why it happens

The swale computes the volume of water at depth d from the trapezoidal section times the void fraction (Eqs. 6-53 and 6-54 of the Reference Manual Vol. III), but advances the depth with the net inflow divided by the plan area only:

```c
// src/legacy/engine/lidproc.c, swaleFluxRates()
    flowArea = (depth * (botWidth + slope * depth)) *
               theLidProc->surface.voidFrac;

    //... wet volume and effective depth
    volume = length * flowArea;
    ...
    f[SURF] = dVdT / surfArea;
    ...
    SurfaceVolume = volume / lidArea;
```

The volume is V(d) = phi L d (Wb + s d), so dV/dd = phi x surfArea, and a net inflow dVdT changes V by phi x dVdT. The other (1 - phi) x dVdT is not stored anywhere. Eq. 6-51 of the manual, A1 dd1/dt = (i + q0)A - (e1 + f1)A1 - q1A, leaves out phi in the same way and so contradicts Eqs. 6-53/6-54; the code follows the manual.

The permeable pavement has the same mismatch with a plain depth:

```c
// src/legacy/engine/lidproc.c, pavementFluxRates()
    SurfaceVolume = surfaceDepth * theLidProc->surface.voidFrac;
    ...
    f[SURF] = SurfaceInflow - SurfaceEvap - SurfaceInfil - SurfaceOutflow;
```

while `biocellFluxRates()`, `trenchFluxRates()` and `greenRoofFluxRates()` write `f[SURF] = (...) / theLidProc->surface.voidFrac`. The overflow function `getSurfaceOverflowRate()`, used by the pavement here, also counts the water above the berm as `delta * surface.voidFrac`, consistent with the corrected depth rate.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-19_swale.inp`](CON-19_swale.inp) | 1-acre swale, top width 20 ft, 12 in deep, side slopes 5:1, VegFrac 0.5, n = 0.24, slope 0.5 %; 2.0 in of rain in 1 hour; the run ends at 1:00 with water in the swale; writes the LID report file `CON-19_lid.txt` |
| [`CON-19_pavement.inp`](CON-19_pavement.inp) | 1-acre permeable pavement, 3 in surface storage with VegFrac 0.5 and no overland flow, 6 in pavement with permeability 0.5 in/hr, 12 in gravel bed with no seepage; 2.0 in of rain in 1 hour; the run ends at 1:30 |
| [`CON-19_test.c`](CON-19_test.c) | Runs both decks through the legacy toolkit. Swale: the water held at the final level read from the LID report file, phi L d (Wb + s d) / A, must equal rain - surface runoff within 0.05 in. Pavement: final storage must equal rain - surface runoff within 0.05 in and the runoff continuity error must be below 1 % |
| [`CON-19_test6.c`](CON-19_test6.c) | The same against the 6.0.0 C API |

The swale check uses the water level rather than the continuity table because 6.0.0's table has its own swale error (see below). The tolerance of 0.05 in is four times the integration error of the swale scheme: with VegFrac 0 the swale ends 0.012 in short of rain - runoff (the 0.624 % in the table above).

```sh
tools/run-test.sh CON-19            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-19 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.3.0 (5.2.4 prints the same within 0.001 in; 6.0.0 prints the same swale numbers and a pavement storage of 1.375 in, 31.253 %):

```
Swale, VegFrac 0.5 (in)
  rain 2.000  surface runoff 0.070  -> must hold 1.930
  final water level 3.360  -> holds 0.958
Permeable pavement, VegFrac 0.5 (in)
  rain 2.000  surface runoff 0.000  -> must hold 2.000
  final storage 1.371  runoff continuity error 31.461 %
FAIL: the swale received 1.930 in net but its final level of 3.360 in holds 0.958 in
```

**With the fix**, 5.3.0 (6.0.0 prints the same, except a pavement continuity error of 0.004 %):

```
Swale, VegFrac 0.5 (in)
  rain 2.000  surface runoff 0.190  -> must hold 1.810
  final water level 5.791  -> holds 1.797
Permeable pavement, VegFrac 0.5 (in)
  rain 2.000  surface runoff 0.000  -> must hold 2.000
  final storage 2.000  runoff continuity error 0.000 %
PASS: with VegFrac 0.5 the water held by the swale and the pavement equals what they received
```

With the fix, 5.3.0's runoff continuity error for the swale deck is 0.649 %, the same integration error as with VegFrac 0.

**6.0.0's own swale storage error.** 6.0.0 builds the runoff continuity table's final storage from `LIDSolver::storedVolume()` ([`LID.cpp:561`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/LID.cpp#L561)), which counts every LID's surface water as depth x void fraction. For a swale that is not the trapezoid volume legacy uses (`waterBalance.finalVol`). The swale deck with VegFrac 0 reports 3.150 in of final storage and a continuity error of -67.778 % in 6.0.0, against 1.782 in and 0.624 % in 5.3.0. With this fix, 6.0.0's table for the VegFrac 0.5 swale reads 2.895 in and -54.265 % instead of 1.680 in and 12.522 %. That is a separate defect, not changed here.

## The fix

Divide the surface depth rate by the void fraction:

```diff
@@ pavementFluxRates()
-    f[SURF] = SurfaceInflow - SurfaceEvap - SurfaceInfil - SurfaceOutflow;
+    f[SURF] = (SurfaceInflow - SurfaceEvap - SurfaceInfil - SurfaceOutflow) /
+              theLidProc->surface.voidFrac;
@@ swaleFluxRates()
-    f[SURF] = dVdT / surfArea;
+    f[SURF] = dVdT / (surfArea * theLidProc->surface.voidFrac);
```

The depth now changes by the net inflow divided by the space the water can occupy, the stored volume equals the water received, and the swale's depth, and with it its Manning outflow and the time it reaches its berm, match the water it holds. The 6.0.0 patch makes the same two changes in `LIDSolver`. The reference manual's Eq. 6-51 should read phi1 A1 dd1/dt on its left side.

What changes for users: every vegetative swale and permeable pavement with VegFrac > 0 fills faster and, for a swale, conveys more flow; in the test the swale's outflow during the storm rises from 0.070 in to 0.190 in. Nothing changes when VegFrac = 0 (the division is by 1.0).

Effect on other models: no regression deck uses a swale or a pavement with VegFrac > 0 (`update_v52/CoS-Reduced-Outlets.inp` defines a swale with VegFrac 0.5 but does not place it). The 5.3.0 and 6.0.0 CLIs built with the patch give binary output files identical to the unpatched builds for `examples/Example4.inp` (swale and pavement with VegFrac 0), `update_v52/CoS-Reduced-Outlets.inp` and `update_v5111/porous_pavement.inp`.
