# API-06: External pollutant buildup set through the API is added again at every runoff step

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | One call that sets an external buildup of 1.0 on a 10 ac subcatchment adds 240 lb over a dry day with a 1 h DRY_STEP and 2,880 lb with a 5 min DRY_STEP: the result is proportional to the number of runoff steps, so it changes with DRY_STEP, WET_STEP and rain timing. Reading the property back gives 43,561.6 (US units) instead of 1.0. Nothing warns. |
| **Reached from** | `swmm_setValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_EXTERNAL_POLLUTANT_BUILDUP, ...)` during a run (Python: `set_external_pollutant_buildup`), and the matching getter |
| **5.3.0** | `surfqual_applyAPIBuildup()` in [`src/legacy/engine/surfqual.c:189`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/surfqual.c#L189), called from `runoff_execute()` at [`runoff.c:277`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L277); setter and getter in [`swmm5.c:2129`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2129) and [`:2678`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2678) |
| **5.2.4** | Not affected: no such property |
| **6.0.0** | Not applicable: there is no runtime external buildup. The per-area buildup setter it has, `swmm_subcatch_set_initial_loading()`, reads back as set and gives the same buildup for both time steps (checked by the test) |
| **Since** | 5.3.0, fork commit 5b87a2b5 ("WIP API bindings for pollutants", December 2024), getter and application together |
| **Fix** | Apply the value as a rate per day over each runoff step, return it unscaled: [`API-06_swmm530.patch`](API-06_swmm530.patch) |

## The problem

5.3.0 lets a toolkit client add pollutant buildup to a subcatchment from outside the model, for example deposition computed by an air-quality model or a spill. The value is a mass per unit area (the code multiplies it by the land use area in acres or hectares). What the run does with it depends on the runoff time step rather than on the value:

| DRY_STEP | Buildup after one dry day, value 1.0 set once at the start | Surface Buildup in the report (10 ac) |
|---|---|---|
| 1 h | 24 lb/ac | 240.000 lb |
| 5 min | 288 lb/ac | 2,880.000 lb |

The value is added in full at every runoff step and never cleared, so it behaves neither as a one-off addition (which would give 1 lb/ac) nor as a rate (which needs a time unit). Since runoff steps switch between DRY_STEP and WET_STEP and are shortened at rain-series breakpoints, the same script gives different loads for different time-step settings and storms.

The getter does not invert the setter either: after setting 1.0, `swmm_getValueExpanded` returns 43,561.6 in US units (1 / UCF(LANDAREA)) and 107,639 in SI.

## Why it happens

`runoff_execute()` calls the API buildup routine for every subcatchment at every runoff step, wet or dry:

```c
// src/legacy/engine/runoff.c, runoff_execute()
// --- apply api unconstrained pollutant buildup from API
surfqual_applyAPIBuildup(j);
```

and the routine adds value x area without any time-step factor:

```c
// src/legacy/engine/surfqual.c, surfqual_applyAPIBuildup()
area = f * Subcatch[subcatchIndex].area * UCF(LANDAREA);       // acres or ha
...
newBuildup = MAX(0.0, oldBuildup + Subcatch[subcatchIndex].apiExtBuildup[p] * area);
```

Its declaration in `funcs.h` says it "applies the buildup of a pollutant over a subcatchment for a given time step", and `objects.h` calls the field a "build up flux", but the time step is not passed in. The setter stores the value unscaled and the getter divides it by `UCF(LANDAREA)`:

```c
// src/legacy/engine/swmm5.c
Subcatch[index].apiExtBuildup[pollutantIndex] = value;                // setter
return subcatch->apiExtBuildup[pollutantIndex] / UCF(LANDAREA);       // getter
```

## How to reproduce

| File | What it is |
|---|---|
| [`API-06_dry-step-1h.inp`](API-06_dry-step-1h.inp) | S1, 10 ac, one land use (100%) with no buildup function, P1, no rain, 24 h, DRY_STEP 1 h |
| [`API-06_dry-step-5min.inp`](API-06_dry-step-5min.inp) | The same with DRY_STEP 5 min |
| [`API-06_test.c`](API-06_test.c) | Sets the external buildup of P1 on S1 to 1.0 after `swmm_start` (before the first step), reads it back, runs the day and reads S1's buildup per acre at the end (`swmm_SUBCATCH_POLLUTANT_BUILDUP`, correct for one land use, see [API-07](../API-07-buildup-getter-sums-land-uses/)). Expected: read-back 1.0 and 1.0 lb/ac after one day for both decks (within 5%). 5.2.4 has no such API and prints PASS |
| [`API-06_test6.c`](API-06_test6.c) | 6.0.0: sets a 1.0 lb/ac initial loading through the API on both decks, reads it back, and reads the report's Initial Buildup (10 lb expected for both) |

The expected 1.0 lb/ac holds for either reading of the property, a rate of 1 lb/ac per day or a one-off addition of 1 lb/ac, because the run lasts one day.

```sh
tools/run-test.sh API-06            # 5.3.0: FAIL; 5.2.4 and 6.0.0: PASS
tools/run-test.sh API-06 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0:

```
EXTERNAL_POLLUTANT_BUILDUP(S1, P1) set to 1.0 at the start of a dry day
DRY_STEP        read back     buildup after 24 h         expected
1 h            43561.5961         24.0000 lb/ac        1.0 lb/ac
5 min          43561.5961        288.0000 lb/ac        1.0 lb/ac
-> the getter does not return the value set
-> the buildup added depends on the runoff time step
FAIL: external buildup reads back as 4.356e+04 and adds 24 lb/ac (1 h steps) vs 288 lb/ac (5 min steps) for one value of 1.0
API-06 5.3.0 base: FAIL
```

6.0.0's per-area buildup API:

```
DRY_STEP    read back     report Initial Buildup   expected
1 h            1.0000                  10.000 lb      10 lb
5 min          1.0000                  10.000 lb      10 lb
PASS: a per-area buildup set through the API reads back as set and gives 1.0 lb/ac x 10 ac = 10 lb whatever the time step
API-06 6.0.0 base: PASS
```

**With the fix**, 5.3.0 (the report's Surface Buildup is 10.000 lb for both decks):

```
DRY_STEP        read back     buildup after 24 h         expected
1 h                1.0000          1.0000 lb/ac        1.0 lb/ac
5 min              1.0000          1.0000 lb/ac        1.0 lb/ac
PASS: the external buildup reads back as set and adds 1.0 lb/ac per day whatever the runoff time step
API-06 5.3.0 patched: PASS
```

## The fix

The value has to be scaled by the runoff step to mean anything. The patch makes it a rate in mass (or count) per acre or hectare per day, the unit of SWMM's own buildup rate constants. It persists until the client changes it, like the node and link pollutant mass fluxes, and the total does not depend on how often the client calls the setter between runoff steps. A one-off addition would need clearing the value after use, and two calls in the same runoff step would then overwrite each other.

```diff
-void surfqual_applyAPIBuildup(int subcatchIndex)
+void surfqual_applyAPIBuildup(int subcatchIndex, double tStep)
 ...
-            newBuildup = MAX(0.0, oldBuildup + Subcatch[subcatchIndex].apiExtBuildup[p] * area);
+            newBuildup = MAX(0.0, oldBuildup + Subcatch[subcatchIndex].apiExtBuildup[p] * area *
+                             tStep / SECperDAY);
 ...
-        surfqual_applyAPIBuildup(j);
+        surfqual_applyAPIBuildup(j, runoffStep);
 ...
-        return subcatch->apiExtBuildup[pollutantIndex] / UCF(LANDAREA);
+        return subcatch->apiExtBuildup[pollutantIndex];
```

The declaration in `funcs.h` and the field comment in `objects.h` now state the unit. The Python wrapper's docstring for `set_external_pollutant_buildup` ("Mass to add in project mass units") should say "rate in mass per unit area per day". Clients that set the value once and relied on the per-step repetition will see much smaller loads (1/288 of the old amount for a day of 5-minute steps). Runs without API calls are unchanged: the value is 0 unless set.
