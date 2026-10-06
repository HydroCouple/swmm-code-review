# API-13: Rainfall injected on a subcatchment through the API is computed with the dry time step

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | When rain is supplied to a subcatchment through the API and its rain gage is dry, the runoff step stays at `DRY_STEP`. The whole step is computed with the rate set at its start; changes the caller makes during it are ignored. In the test (2 in/hr for 30 min, `DRY_STEP` 1 h): with a 1-h gage interval the subcatchment receives 2.000 in instead of 1.000 in; with a 30-min interval it receives 1.000 in in one 30-min step and the runoff continuity error is -17.25 % instead of -0.10 %. The report shows the error but nothing explains it. |
| **Reached from** | 5.3.0: `swmm_setValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_API_RAINFALL` (or `_API_SNOWFALL`)`, ...)` during a run. 6.0.0: `swmm_forcing_subcatch_rainfall()` (or the subcatchment snowfall forcing) during a run. In both cases with no rain from the subcatchment's gage at that time |
| **5.3.0** | `runoff_execute()` sets `IsRaining` from the gages only, [`src/legacy/engine/runoff.c:197-201`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L197-L201); `runoff_getTimeStep()` picks the step from it, [`runoff.c:329`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L329); the API rate is added in `getNetPrecip()`, [`subcatch.c:805`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L805) |
| **5.2.4** | Not affected: there is no subcatchment rainfall property ([`src/solver/swmm5.c:867`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L867)); gage rainfall set with `swmm_GAGE_RAINFALL` does set the wet step |
| **6.0.0** | Reproduces with `swmm_forcing_subcatch_rainfall()`: `is_raining` is set from the gages only, [`src/engine/core/SWMMEngine.cpp:1920-1927`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L1920-L1927), and `computeRunoffTimestep()` picks the step from it, [`SWMMEngine.cpp:3279`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L3279) |
| **Since** | 5.3.0, when subcatchment API rainfall was added (fork commit 5b87a2b5, December 2024, #204); 6.0.0's forcing API from defd494c (March 2026) |
| **Fix** | Treat a positive subcatchment API rainfall or snowfall as rain when choosing the runoff step: [`API-13_swmm530.patch`](API-13_swmm530.patch), [`API-13_swmm600.patch`](API-13_swmm600.patch) |

## The problem

Both engines let a program supply rainfall directly to a subcatchment during a run, bypassing its rain gage: for example, to drive a model from radar or from another model, re-setting the rate before each step. The rate is used, but the runoff time step does not know about it.

SWMM computes runoff with `WET_STEP` while it rains and `DRY_STEP` otherwise, and "it rains" means "a rain gage reports rain". With the gage dry, the first runoff step is a dry step, up to `DRY_STEP` long (shortened only to the gage's next recorded value). Runoff is computed ahead of routing, so that whole step is computed once, at its start, with the API rate in force then. Whatever the caller sets during the following routing steps falls inside a runoff step that is already done.

In the test, the caller supplies 2 in/hr to a 1-acre impervious subcatchment for the first 30 minutes and 0 afterwards, re-setting the value before every 1-minute step. The right answer is 1.000 in of rain, with the small continuity error of the same storm read from a gage (-0.10 %):

- With a 1-hour gage interval, the first runoff step is 1 hour long and uses 2 in/hr throughout. The subcatchment receives 2.000 in; setting 0 at 30 minutes has no effect. Continuity error -8.62 %.
- With a 30-minute gage interval, the first step is 30 minutes long. The depth is right, 1.000 in, but one 30-minute step of a nonlinear reservoir gives a continuity error of -17.25 % (more runoff than rain).

## Why it happens

```c
// src/legacy/engine/runoff.c, runoff_execute()
    IsRaining = FALSE;
    for (j = 0; j < Nobjects[GAGE]; j++)
    {
        gage_setState(j, currentDate);
        if ( Gage[j].rainfall > 0.0 ) IsRaining = TRUE;
    }
...
// runoff_getTimeStep()
    if ( IsRaining || HasSnow || HasRunoff || HasWetLids )
    {
        timeStep = WetStep;
    }
    else timeStep = DryStep;
```

The API rate is added later, per subcatchment, when the step is computed:

```c
// src/legacy/engine/subcatch.c, getNetPrecip()
    rainfall += Subcatch[j].apiRainfall;
    snowfall += Subcatch[j].apiSnowfall;
```

and `execRouting()` runs `runoff_execute()` only when the routing clock passes the end of the previous runoff step (`while (NewRunoffTime < nextRoutingTime)`), so a long runoff step is never revisited.

6.0.0 has the same structure. `swmm_forcing_subcatch_rainfall()` stores the forcing in `ctx.forcing`, the runoff solver applies it through `effective_rainfall()`, and the wet/dry choice looks only at `ctx_.gages.rainfall`. A gage-level forcing (`swmm_forcing_gage_rainfall()`) is no way around it: the runoff step overwrites the gage's rainfall from its series before the forcing is used (see API-14).

## How to reproduce

| File | What it is |
|---|---|
| [`API-13_gage1h.inp`](API-13_gage1h.inp) | S1: 1 ac, 100 % impervious, no infiltration or evaporation; gage G1 with a 1-h interval and no rain; `WET_STEP` 1 min, `DRY_STEP` 1 h, `ROUTING_STEP` 1 min |
| [`API-13_gage30min.inp`](API-13_gage30min.inp) | The same with a 30-min gage interval |
| [`API-13_gage-rain.inp`](API-13_gage-rain.inp) | Reference: the same 2 in/hr for 30 min from the gage |
| [`API-13_test.c`](API-13_test.c) | Legacy API: sets `swmm_SUBCATCH_API_RAINFALL` on S1 before every `swmm_step()` (2 in/hr while t < 30 min, then 0), then reads Total Precipitation and the continuity error from the report's Runoff Quantity Continuity table. Requires 1.000 ± 0.01 in and an error within ±1 % (5.3.0 only, `#ifdef`; 5.2.4 prints PASS because the property does not exist) |
| [`API-13_test6.c`](API-13_test6.c) | The same with `swmm_forcing_subcatch_rainfall(..., SWMM_FORCING_OVERRIDE, SWMM_FORCING_PERSIST)` |
| [`API-13_swmm530.patch`](API-13_swmm530.patch), [`API-13_swmm600.patch`](API-13_swmm600.patch) | The fixes |

```sh
tools/run-test.sh API-13            # 5.2.4 PASS, 5.3.0 FAIL, 6.0.0 FAIL
tools/run-test.sh API-13 --patched  # 5.3.0 PASS, 6.0.0 PASS
```

**Without the fix**, both engines give the same wrong results:

```
2 in/hr for 30 min on S1, DRY_STEP 1 h, WET_STEP 1 min
                                 precipitation (in)  runoff continuity (%)
rain from the gage (reference)                1.000                  -0.10
API, gage interval 30 min                     1.000                 -17.25
API, gage interval 1 h                        2.000                  -8.62
FAIL: in 2 of 3 runs the 1.000 in of rain was not applied as given (wrong depth or a runoff continuity error beyond 1 %)
API-13 5.3.0 base: FAIL
```

```
forcing, gage interval 30 min                 1.000                 -17.25
forcing, gage interval 1 h                    2.000                  -8.62
FAIL: in 2 of 3 runs the 1.000 in of rain was not applied as given (wrong depth or a runoff continuity error beyond 1 %)
API-13 6.0.0 base: FAIL
```

**With the fix**, the injected storm gives the same totals as the gage storm, in both engines:

```
rain from the gage (reference)                1.000                  -0.10
API, gage interval 30 min                     1.000                  -0.10
API, gage interval 1 h                        1.000                  -0.10
PASS: rain injected through the API gives 1.000 in and a small continuity error, like the same rain from a gage
API-13 5.3.0 patched: PASS
```

```
forcing, gage interval 30 min                 1.000                  -0.10
forcing, gage interval 1 h                    1.000                  -0.10
PASS: rain injected through the API gives 1.000 in and a small continuity error, like the same rain from a gage
API-13 6.0.0 patched: PASS
```

## The fix

5.3.0 sets `IsRaining` when any subcatchment has a positive API rainfall or snowfall:

```diff
     IsRaining = FALSE;
     for (j = 0; j < Nobjects[GAGE]; j++)
     {
         gage_setState(j, currentDate);
         if ( Gage[j].rainfall > 0.0 ) IsRaining = TRUE;
     }
+
+    // --- precipitation set on a subcatchment through the API also
+    //     requires a wet time step
+    for (j = 0; j < Nobjects[SUBCATCH]; j++)
+    {
+        if ( Subcatch[j].apiRainfall > 0.0 || Subcatch[j].apiSnowfall > 0.0 )
+            IsRaining = TRUE;
+    }
```

6.0.0 does the same for active subcatchment rainfall and snowfall forcings with a positive value. Once the rate drops to 0, ponded water keeps the step wet (`HasRunoff`), as after gage rain.

Input files run without the API are unaffected: the API rates are 0 and the forcings inactive unless a program sets them.

The fix cannot see rain that a caller starts in the middle of a dry step that has already been computed: that rain is applied from the end of the step. A program that injects rain into a model whose gages are dry should keep `DRY_STEP` no longer than the interval at which it changes the rate.
