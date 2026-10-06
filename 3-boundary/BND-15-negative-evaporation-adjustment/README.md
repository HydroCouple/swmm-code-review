# BND-15: A negative evaporation adjustment larger than the base rate makes evaporation add water

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | If a monthly `[ADJUSTMENTS] EVAPORATION` value is more negative than the base evaporation rate, the adjusted rate is negative and subcatchment surfaces gain water from it. In the test deck the runoff balance books -0.100 in of evaporation, depression storage ends above its 0.2 in capacity, and runoff is 0.396 in instead of 0.301 in. There is no error or warning. |
| **Reached from** | `[ADJUSTMENTS] EVAPORATION` with a negative value whose magnitude exceeds the month's evaporation rate from any source (constant, monthly, time series, climate file or temperature). A negative `[EVAPORATION]` rate, which is also accepted, takes the same path. |
| **5.3.0** | `setEvap()` in [`src/legacy/engine/climate.c:1266`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/climate.c#L1266) |
| **5.2.4** | Same code, [`src/solver/climate.c:910`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/climate.c#L910) |
| **6.0.0** | Reproduces: `climate::updateDailyClimate()` in [`src/engine/hydrology/Climate.cpp:150`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Climate.cpp#L150), and for time series and pan evaporation [`src/engine/core/SWMMEngine.cpp:2021`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L2021) and [`:2031`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L2031) |
| **Since** | 5.1.007, which added `[ADJUSTMENTS]` |
| **Fix** | Stop the adjusted rate at zero: [`BND-15_swmm530.patch`](BND-15_swmm530.patch), [`BND-15_swmm600.patch`](BND-15_swmm600.patch) |

## The problem

`[ADJUSTMENTS] EVAPORATION` adds a monthly amount to the evaporation rate. The input reference describes the values as "adjustments to evaporation rate in January, February, etc., as plus or minus in/day (mm/day)". Climate-change scenarios use negative values to lower evaporation. When the reduction is larger than the month's rate, SWMM does not stop at zero. It carries on with a negative evaporation rate, and a negative evaporation loss puts water onto every subcatchment surface that has ponded or depression-stored water.

The test deck has a 10-acre impervious subcatchment with 0.2 in of depression storage, a monthly evaporation rate of 0.05 in/day, an adjustment of -0.10 in/day and 0.5 in of rain in the first hour of a 2-day run. With no evaporation, 0.2 in should stay in depression storage and 0.3 in should run off. All three engines report:

```
  Total Precipitation ......         0.417         0.500
  Evaporation Loss .........        -0.083        -0.100
  Infiltration Loss ........         0.000         0.000
  Surface Runoff ...........         0.330         0.396
  Final Storage ............         0.171         0.206
  Continuity Error (%) .....        -0.304
```

Over the 2 days, 0.05 in/day x 2 days = 0.100 in of water appears from nowhere. Once depression storage is full it keeps running off, so the subcatchment produces runoff for two days after a one-hour storm.

Only subcatchment surfaces are affected. Storage nodes and conduits use evaporation only when `Evap.rate > 0.0`, and the LID and groundwater routines clamp their evaporation at zero. The reported potential evaporation in the binary output is negative too.

## Why it happens

`setEvap()` takes the rate from its source and then adds the monthly adjustment with no lower bound:

```c
// src/legacy/engine/climate.c, setEvap()
    case MONTHLY_EVAP:
        Evap.rate = Evap.monthlyEvap[mon - 1] / UCF(EVAPRATE);
        break;
    ...
    // --- apply climate change adjustment
    Evap.rate += Adjust.evap[mon - 1];
```

`climate_readAdjustments()` reads the values without a range check, and nothing checks the sum. `getSubareaRunoff()` then limits evaporation only from above, by the water available:

```c
// src/legacy/engine/subcatch.c, getSubareaRunoff()
    surfMoisture = subarea->depth / tStep;
    surfEvap = MIN(surfMoisture, evap);
    ...
    Vevap += surfEvap * area * tStep;
```

With `evap` = -0.05 in/day, `surfEvap` is negative. It is booked as negative evaporation and, in the subarea's water balance, raises the ponded depth.

6.0.0 applies the adjustment in three places: in `updateDailyClimate()` for constant, monthly and temperature-based evaporation, and in `SWMMEngine::stepRunoff()` for time series and pan evaporation. None of them sets a lower bound.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-15_negative-adjustment.inp`](BND-15_negative-adjustment.inp) | 10-acre impervious subcatchment with 0.2 in of depression storage, monthly evaporation 0.05 in/day, adjustment -0.10 in/day, 0.5 in of rain in the first hour of a 2-day run |
| [`BND-15_test.c`](BND-15_test.c) | Runs the deck through the legacy toolkit. Tracks the lowest subcatchment evaporation rate (`swmm_getValue(swmm_SUBCATCH_EVAP)`) and reads the evaporation loss and final storage from the report's runoff balance. Expects a rate >= 0, an evaporation loss of 0 and a final storage of 0.200 in. |
| [`BND-15_test6.c`](BND-15_test6.c) | The same check for 6.0.0 with `swmm_subcatch_get_evap()` and `swmm_get_runoff_total()` |

```sh
tools/run-test.sh BND-15            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-15 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, all three engines print:

```
Base rate 0.05 in/day, adjustment -0.10 in/day
Lowest subcatchment evaporation rate    -0.0500 in/day  (expected >= 0)
Runoff balance: evaporation loss         -0.100 in      (expected 0.000)
Runoff balance: final storage             0.206 in      (expected 0.200)
FAIL: the adjusted evaporation rate is negative and adds water: rate -0.0500 in/day, evaporation loss -0.100 in, final storage 0.206 in with 0.2 in of depression storage
BND-15 5.3.0 base: FAIL
```

**With the fix**, 5.3.0 and 6.0.0 print the same:

```
Base rate 0.05 in/day, adjustment -0.10 in/day
Lowest subcatchment evaporation rate     0.0000 in/day  (expected >= 0)
Runoff balance: evaporation loss          0.000 in      (expected 0.000)
Runoff balance: final storage             0.200 in      (expected 0.200)
PASS: the adjusted evaporation rate stops at zero and no water is created
BND-15 5.3.0 patched: PASS
BND-15 6.0.0 patched: PASS
```

The report's runoff balance then shows 0.301 in of runoff and 0.200 in of final storage.

## The fix

5.3.0, in `setEvap()`:

```diff
-    // --- apply climate change adjustment
-    Evap.rate += Adjust.evap[mon - 1];
+    // --- apply climate change adjustment (the rate cannot go below 0)
+    Evap.rate = MAX(0.0, Evap.rate + Adjust.evap[mon - 1]);
```

6.0.0 clamps once in `SWMMEngine::stepRunoff()`, after every source and adjustment and before the API forcing, which covers all three places where the adjustment is added:

```diff
+        // A negative [ADJUSTMENTS] EVAPORATION value larger than the source
+        // rate stops the rate at zero (legacy setEvap).
+        ctx_.climate_state.evap_rate = std::max(0.0, ctx_.climate_state.evap_rate);
```

A rate prescribed through the API still overrides the result, as before. Non-negative rates pass through unchanged, so results change only for models whose adjusted rate went below zero. None of the regression decks uses an evaporation adjustment.
