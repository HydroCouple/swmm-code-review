# API-12: swmm_getValue(swmm_MAXROUTESTEP) always returns the minimum routing step

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | The "maximum routing step" getter returns `MinRouteStep` (0.5 s by default) for every model at every time. In the test model the engine takes steps of up to 30 s while the getter says 0.5 s; a caller that sizes a stride or an external coupling step from it works 60 times finer than needed. Calling it between `swmm_start` and the first `swmm_step` also changes the run (the first step becomes 30 s instead of 0.5 s). No warning. |
| **Reached from** | `swmm_getValue(swmm_MAXROUTESTEP, 0)` (5.3.0 also `swmm_getValueExpanded(swmm_SYSTEM, swmm_MAXROUTESTEP, ...)`) during a dynamic wave run |
| **5.3.0** | `getMaxRouteStep()` in [`src/legacy/engine/swmm5.c:3164`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L3164), which calls `dynwave_getRoutingStep()` ([`dynwave.c:192`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L192)) and `getVariableStep()` ([`dynwave.c:834`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L834)) |
| **5.2.4** | Same code: [`src/solver/swmm5.c:1345`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L1345) |
| **6.0.0** | Not applicable: the 6.0.0 API has no Courant-limited step getter; `swmm_get_routing_step()` returns the `ROUTING_STEP` option, which bounds the steps taken (checked by the 6.0.0 test) |
| **Since** | 5.2.0, which added `swmm_MAXROUTESTEP` |
| **Fix** | A side-effect-free `dynwave_getMaxRoutingStep()` capped at `ROUTING_STEP`: [`API-12_swmm530.patch`](API-12_swmm530.patch) |

## The problem

With `VARIABLE_STEP 0.75` the dynamic wave solver picks each step from the Courant condition with a safety factor of 0.75, capped at `ROUTING_STEP`. `swmm_MAXROUTESTEP` is meant to give the same Courant step without the safety factor (factor 1), i.e. the longest step that is stable for the current flow. Whatever the flow, it returns `MinRouteStep`. In the test model (two 100-ft, 2-ft pipes draining a 40-acre subcatchment under 2 in/hr of rain, `ROUTING_STEP 30`, `MINIMUM_STEP 0.5`), 5.3.0 takes 1210 steps between 1.49 s and 30 s, and the getter reports 0.500 s before every one of them.

The value can never be right: the solver's own check, with the stricter factor 0.75, accepts every one of those steps, so the stable step with factor 1 is at least as long.

## Why it happens

```c
// src/legacy/engine/swmm5.c, getMaxRouteStep()
    if (!IsStartedFlag || RouteModel != DW)
        return result;
    CourantFactor = 1.0;
    result = routing_getRoutingStep(RouteModel, MinRouteStep);
    CourantFactor = tmpCourantFactor;
```

The second argument of `routing_getRoutingStep()` is the user's fixed step, which the dynamic wave solver uses as the upper limit of its search. `dynwave_getRoutingStep()` then takes one of two paths, and both return `MinRouteStep`:

```c
// src/legacy/engine/dynwave.c, dynwave_getRoutingStep()
    if ( VariableStep == 0.0 )            // before the first step
    {
        VariableStep = MinRouteStep;
    }
    else VariableStep = getVariableStep(fixedStep);

// src/legacy/engine/dynwave.c, getVariableStep(maxStep)
    tMin = maxStep;                       // = MinRouteStep: can only go down
    tMinLink = getLinkStep(tMin, &minLink);
    tMinNode = getNodeStep(tMinLink, &minNode);
    ...
    stats_updateCriticalTimeCount(minNode, minLink);
    if ( tMin < MinRouteStep ) tMin = MinRouteStep;   // ...and is clamped back up
```

The call is not read-only either. It overwrites the solver's `VariableStep`: before the first step that flips the "first step" test, so the first routing step is computed from the Courant condition (30 s here) instead of being `MinRouteStep`. It also passes through `stats_updateCriticalTimeCount()`, so in a model with a Courant step below `MinRouteStep` each query adds to the report's time-step-critical counts.

## How to reproduce

| File | What it is |
|---|---|
| [`API-12_variable-step.inp`](API-12_variable-step.inp) | 40-acre subcatchment, two 100-ft conduits, 2 in/hr storm; DYNWAVE, `ROUTING_STEP 30`, `VARIABLE_STEP 0.75`, `MINIMUM_STEP 0.5`, 3 hours |
| [`API-12_test.c`](API-12_test.c) | Reads `swmm_MAXROUTESTEP` after `swmm_start` and after every step, and compares it with the length of the next step the engine takes (1 ms tolerance for the solver's millisecond rounding) |
| [`API-12_test6.c`](API-12_test6.c) | The same check for 6.0.0's `swmm_get_routing_step()` |

```sh
tools/run-test.sh API-12            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh API-12 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0:

```
ROUTING_STEP                           30.000 s
steps taken                              1210
step lengths taken                      1.492 to 30.000 s
MAXROUTESTEP reported before a step     0.500 to 0.500 s
shortest step after the first           1.492 s; MAXROUTESTEP before it 0.500 s
steps longer than the MAXROUTESTEP
  reported just before them              1210 of 1210
  first: at 0.0000 h the engine took 30.000 s; MAXROUTESTEP was 0.500 s
FAIL: MAXROUTESTEP (0.500 to 0.500 s) is shorter than the steps the engine takes (up to 30.000 s)
API-12 5.3.0 base: FAIL
```

5.2.4 is the same (its shortest step is 1.087 s). The first step is 30 s in both because the query before it reset `VariableStep`; 6.0.0, with no such query, starts with a 0.5-s step and takes 1212 steps.

**With the fix**:

```
ROUTING_STEP                           30.000 s
steps taken                              1211
step lengths taken                      0.500 to 30.000 s
MAXROUTESTEP reported before a step     1.996 to 30.000 s
shortest step after the first           1.497 s; MAXROUTESTEP before it 1.996 s
steps longer than the MAXROUTESTEP
  reported just before them                 0 of 1211
PASS: MAXROUTESTEP is never shorter than the next step the engine takes
API-12 5.3.0 patched: PASS
```

At the peak the engine takes 1.497 s and the getter reports 1.996 s: the ratio is the 0.75 safety factor. The run now starts with the 0.5-s step it takes when nobody queries the getter (1211 steps).

## The fix

A new function in `dynwave.c` runs the solver's link (Courant) and node (depth change) checks with a Courant factor of 1, starting from `ROUTING_STEP`, clamps and rounds the result as the solver does, and touches nothing else:

```c
double dynwave_getMaxRoutingStep(double maxStep)
{
    int    minLink = -1, minNode = -1;
    double saveCourantFactor = CourantFactor;
    double tMin;

    CourantFactor = 1.0;
    tMin = getLinkStep(maxStep, &minLink);
    tMin = getNodeStep(tMin, &minNode);
    CourantFactor = saveCourantFactor;
    if ( tMin < MinRouteStep ) tMin = MinRouteStep;
    return floor(1000.0 * tMin) / 1000.0;
}
```

`getMaxRouteStep()` returns `dynwave_getMaxRoutingStep(RouteStep)` (and, as before, `RouteStep` before the run starts or for other routing methods); the prototype goes in `funcs.h`. The new function is called only by the getter, so runs from an input file are unchanged; a script that queried the getter before the first step gets the same first step as one that did not.
