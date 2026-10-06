# NUM-37: A rooftop disconnection with Rough or Slope = 0 never sheds water

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | The roof produces no surface runoff and no gutter flow. Every inch of rain stays in the roof's surface storage, without limit. In the test, 2.0 in of rain on a roof with 0.1 in of storage gives 0.000 in of runoff and 2.000 in of final storage instead of 1.900 in of runoff. The runoff continuity error stays at 0, so nothing warns the user. |
| **Reached from** | `[LID_CONTROLS]` type `RD` whose `SURFACE` line has `Rough` = 0 or `Slope` = 0, which the input reference allows and documents as "overflows within a single time step" |
| **5.3.0** | `roofFluxRates()` in [`src/legacy/engine/lidproc.c:483`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L483); the fallback overflow is switched off for roofs in `validateLidProc()` at [`lid.c:1113`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lid.c#L1113) |
| **5.2.4** | Same code, [`src/solver/lidproc.c:483`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lidproc.c#L483) and [`lid.c:1084`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lid.c#L1084) |
| **6.0.0** | Reproduces with the same numbers: `LIDSolver::roofFluxRates()` in [`src/engine/hydrology/LID.cpp:776`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/LID.cpp#L776) copies the legacy code |
| **Since** | 5.1.008, which introduced rooftop disconnection |
| **Fix** | Keep the overflow rate that is computed: [`NUM-37_swmm530.patch`](NUM-37_swmm530.patch), [`NUM-37_swmm600.patch`](NUM-37_swmm600.patch) |

## The problem

A rooftop disconnection has a surface layer (depression storage on the roof) and a "drain" that stands for the gutters and downspouts: the drain coefficient is the largest flow rate, in in/hr, the gutters can carry. Water above the storage depth leaves the roof by overland flow (Manning, with `Rough` and `Slope`), the gutters take up to their capacity of it, and the rest runs off the roof surface. The input reference says what happens without overland flow:

> If either Rough or Slope values are 0 then any ponded water that exceeds the surface storage depth is assumed to completely overflow the LID control within a single time step.

That is not what SWMM does. With `Rough` = 0 or `Slope` = 0 the roof never sheds anything. In the test, a 1-acre roof with 0.1 in of storage and 0.5 in/hr gutters gets 2.0 in of rain at 1 in/hr. Once 0.1 in is stored, the gutters should carry 0.5 in/hr and the other 0.5 in/hr should overflow, for 0.950 in each. Instead the run reports no surface runoff, no LID drainage, and 2.000 in held on the roof (20 times its storage depth). The water balance closes, because the water is booked as storage, so the continuity error is 0.000 % and there is no warning.

## Why it happens

`roofFluxRates()` computes the surface outflow, then takes the gutter flow out of it. Without overland flow (`surface.alpha` is 0 when `Rough` or `Slope` is 0) it calls `getSurfaceOverflowRate()`, but discards the return value:

```c
// src/legacy/engine/lidproc.c, roofFluxRates()
if ( theLidProc->surface.alpha > 0.0 )
  SurfaceOutflow = getSurfaceOutflowRate(surfaceDepth);
else getSurfaceOverflowRate(&surfaceDepth);          // result thrown away
StorageDrain = MIN(theLidProc->drain.coeff/UCF(RAINFALL), SurfaceOutflow);
SurfaceOutflow -= StorageDrain;
f[SURF] = (SurfaceInflow - SurfaceEvap - StorageDrain - SurfaceOutflow);
```

`SurfaceOutflow` was set to 0 in `lidproc_getOutflow()` before the solve, so it stays 0, and so does the gutter flow `StorageDrain`. The surface depth only rises.

Other LID types with a surface layer have a fallback: after the solve, `lidproc_getOutflow()` adds `getSurfaceOverflowRate(&x[SURF])` to the outflow when `surface.canOverflow` is set. `validateLidProc()` sets that flag to FALSE for every roof, whatever `alpha` is:

```c
// src/legacy/engine/lid.c, validateLidProc()
LidProcs[j].surface.canOverflow = TRUE;
switch (LidProcs[j].lidType)
{
    case ROOF_DISCON: LidProcs[j].surface.canOverflow = FALSE; break;
```

so nothing removes the water. The fallback would not be the right fix for roofs anyway: it runs after the gutter flow has been computed, so all the overflow would bypass the gutters.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-37_rough0.inp`](NUM-37_rough0.inp) | 1-acre rooftop disconnection, 0.1 in storage, `Rough` = 0, `Slope` = 1 %, gutters 0.5 in/hr; 2.0 in of rain at 1 in/hr; no infiltration, no evaporation |
| [`NUM-37_slope0.inp`](NUM-37_slope0.inp) | The same with `Rough` = 0.015 and `Slope` = 0 |
| [`NUM-37_test.c`](NUM-37_test.c) | Runs both decks through the legacy toolkit and reads surface runoff, LID drainage and final storage from the runoff continuity table |
| [`NUM-37_test6.c`](NUM-37_test6.c) | The same against the 6.0.0 C API |

```sh
tools/run-test.sh NUM-37            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-37 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4, 5.3.0 and 6.0.0 print the same):

```
Roof          Surface runoff  LID drainage  Final storage   (in)
Rough = 0              0.000         0.000          2.000
Slope = 0              0.000         0.000          2.000
expected               0.950         0.950          0.100
FAIL: with Rough = 0 the roof kept 2.000 in of 2.0 in of rain (storage depth 0.1 in) and shed 0.000 in (expected 1.900 in)
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Roof          Surface runoff  LID drainage  Final storage   (in)
Rough = 0              0.950         0.950          0.100
Slope = 0              0.950         0.950          0.100
expected               0.950         0.950          0.100
PASS: with Rough or Slope = 0 water above the 0.1 in storage depth overflows; the gutters carry 0.5 in/hr of it
```

## The fix

Keep the overflow rate as the surface outflow, as the Manning branch does:

```diff
     if ( theLidProc->surface.alpha > 0.0 )
       SurfaceOutflow = getSurfaceOutflowRate(surfaceDepth);
-    else getSurfaceOverflowRate(&surfaceDepth);
+    else SurfaceOutflow = getSurfaceOverflowRate(&surfaceDepth);
```

Water above the storage depth now leaves within the step, the gutters take up to their capacity of it and the rest is surface runoff, which is the documented behaviour. The 6.0.0 patch makes the same change in `LIDSolver::roofFluxRates()`.

Effect on other models: the change only affects rooftop disconnections with `Rough` or `Slope` = 0. None of the 73 regression decks uses a rooftop disconnection, so none changes.
