# API-09: Setting a subcatchment's area, width or slope before the run does not change its runoff

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | `swmm_setValueExpanded()` returns 0 for `swmm_SUBCATCH_WIDTH`, `swmm_SUBCATCH_SLOPE` or `swmm_SUBCATCH_AREA`, and the getter reads the new value back, but the overland flow is still computed with the coefficient of the old geometry. In the test, a width of 5000 ft instead of 500 ft gives a peak runoff of 6.92 cfs instead of 19.28 cfs; a doubled area doubles the peak (13.84 cfs) instead of giving 7.82 cfs. No warning. A calibration loop that adjusts width or slope through the API sees no response. |
| **Reached from** | Toolkit API: `swmm_open()`, then `swmm_setValueExpanded(swmm_SUBCATCH, swmm_SUBCATCH_AREA / _WIDTH / _SLOPE, ...)` (or `swmm_setValue()`), then `swmm_start()` |
| **5.3.0** | `setSubcatchValue()` in [`src/legacy/engine/swmm5.c:2154-2176`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2154-L2176) stores the value; alpha is computed only in `subcatch_validate()`, [`src/legacy/engine/subcatch.c:422`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L422), called from `project_validate()` during `swmm_open()` |
| **5.2.4** | Not affected: `swmm_setValue()` has no subcatchment area, width or slope property ([`src/solver/swmm5.c:867`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L867)) |
| **6.0.0** | Not affected: `swmm_subcatch_set_area/_width/_slope()` are accepted only before `swmm_engine_initialize()`, and `RunoffSolver::init()` computes alpha from the current values ([`src/engine/hydrology/Runoff.cpp:210`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L210)) |
| **Since** | 5.3.0, when the setters were added (fork commit 5b87a2b5, December 2024, #204) |
| **Fix** | Re-run `subcatch_validate()` for the subcatchment in the three setters: [`API-09_swmm530.patch`](API-09_swmm530.patch) |

## The problem

5.3.0 lets a toolkit program change a subcatchment's area, characteristic width and slope after `swmm_open()` and before `swmm_start()`. This is the usual way to calibrate runoff from a script: width in particular is the parameter most often adjusted. The setters accept the value, return 0, and `swmm_getValueExpanded()` returns the new value, but the hydrograph is the one of the unedited model.

The test model is one 10-acre, fully impervious subcatchment (width 500 ft, slope 0.5 %, no depression storage) with 2 in/hr of rain for 10 minutes. Its peak runoff is 6.92 cfs. Setting the width to 5000 ft, or the slope to 5 %, through the API leaves the peak at 6.92 cfs; the same values written in the input file give 19.28 and 14.15 cfs. Setting the area to 20 acres is worse than no change: the runoff volume uses the new area but the overland-flow coefficient does not, so the peak doubles to 13.84 cfs instead of 7.82 cfs.

## Why it happens

Each subarea's outflow is computed from the nonlinear reservoir `q = alpha * (d - ds)^(5/3)`, with alpha fixed by width, area, slope and Manning's n. It is computed once, in `subcatch_validate()`:

```c
// src/legacy/engine/subcatch.c, subcatch_validate()
if ( area > 0.0 && Subcatch[subcatchIndex].subArea[i].N > 0.0 )
{
    Subcatch[subcatchIndex].subArea[i].alpha = PHI * Subcatch[subcatchIndex].width / area *
        sqrt(Subcatch[subcatchIndex].slope) / Subcatch[subcatchIndex].subArea[i].N;
}
```

`subcatch_validate()` is called from `project_validate()` inside `swmm_open()`. `swmm_start()` and `subcatch_initState()` do not recompute alpha. The setters only store the raw value:

```c
// src/legacy/engine/swmm5.c, setSubcatchValue(), simulation not started
case swmm_SUBCATCH_WIDTH:
    if (value >= 0.0)
    {
        Subcatch[index].width = value / UCF(LENGTH);
        return 0;
    }
```

The area is also used directly at run time (rainfall volume, runoff rate `q * area`), so an area change takes effect in those places but not in alpha, which is per unit area.

## How to reproduce

| File | What it is |
|---|---|
| [`API-09_base.inp`](API-09_base.inp) | S1: 10 ac, 100 % impervious, width 500 ft, slope 0.5 %, n = 0.015, no depression storage; 2 in/hr for 10 min; outlet O1 |
| [`API-09_width5000.inp`](API-09_width5000.inp), [`API-09_slope5.inp`](API-09_slope5.inp), [`API-09_area20.inp`](API-09_area20.inp) | The base deck with the width, slope or area edited in `[SUBCATCHMENTS]` |
| [`API-09_test.c`](API-09_test.c) | Legacy API: for each property, opens the base deck, sets the property before `swmm_start()`, runs, and compares S1's peak runoff with a run of the edited deck (5.3.0 only, `#ifdef`; 5.2.4 prints PASS because the setters do not exist) |
| [`API-09_test6.c`](API-09_test6.c) | The same check with `swmm_subcatch_set_width/_slope/_area()` between `swmm_engine_open()` and `swmm_engine_initialize()` |
| [`API-09_swmm530.patch`](API-09_swmm530.patch) | The fix for 5.3.0 |

```sh
tools/run-test.sh API-09            # 5.2.4 PASS, 5.3.0 FAIL, 6.0.0 PASS
tools/run-test.sh API-09 --patched  # 5.3.0 PASS, 6.0.0 PASS
```

**Without the fix**, 5.3.0 reads the new values back but runs the old geometry:

```
Peak runoff of S1 with API-09_base.inp unchanged: 6.9179 cfs

Set before swmm_start    rc  read back   peak via API  peak via .inp      diff
WIDTH 500 -> 5000 ft      0       5000         6.9179        19.2791    64.12%
SLOPE 0.005 -> 0.05       0       0.05         6.9179        14.1516    51.12%
AREA 10 -> 20 ac          0         20        13.8358         7.8161    77.02%
FAIL: 3 of 3 subcatchment geometry setters accepted the value but the runoff did not change as it does when the value is in the input file (largest peak difference 77.0 %)
API-09 5.3.0 base: FAIL
```

6.0.0 already gives the edited-deck result:

```
Set before initialize    rc   peak via API  peak via .inp      diff
width 500 -> 5000 ft      0        19.2791        19.2791     0.00%
slope 0.5 -> 5 %          0        14.1516        14.1516     0.00%
area 10 -> 20 ac          0         7.8161         7.8161     0.00%
PASS: area, width and slope set before initialize give the same runoff as the same values in the input file
API-09 6.0.0 base: PASS
```

**With the fix**, 5.3.0 matches the edited decks exactly, with the same numbers as 6.0.0:

```
WIDTH 500 -> 5000 ft      0       5000        19.2791        19.2791     0.00%
SLOPE 0.005 -> 0.05       0       0.05        14.1516        14.1516     0.00%
AREA 10 -> 20 ac          0         20         7.8161         7.8161     0.00%
PASS: AREA, WIDTH and SLOPE set before swmm_start() give the same runoff as the same values in the input file
API-09 5.3.0 patched: PASS
```

Note that the 5.3.0 slope setter and getter use a fraction (0.05), while the input file and 6.0.0's `swmm_subcatch_set_slope()` use percent (5).

## The fix

Call `subcatch_validate()` for the subcatchment after storing the new value, in each of the three setters:

```diff
         case swmm_SUBCATCH_WIDTH:
             if (value >= 0.0)
             {
                 Subcatch[index].width = value / UCF(LENGTH);
+                subcatch_validate(index);   // recompute overland flow alpha
                 return 0;
             }
```

`subcatch_validate()` recomputes alpha for the three subareas. Its other steps (groundwater defaults, the outlet and ground-elevation checks, marking the gage as used) give the same result on a second call, because none of their inputs can be changed by these setters. Only runs that call the setters change; input files run without the API are unaffected.

An area set smaller than the subcatchment's LID area is still accepted (alpha becomes 0 because the non-LID area is negative). `swmm_open()` rejects that combination in an input file (error `ERR_LID_AREAS`, [`lid.c:1224`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lid.c#L1224)); the setter does not check it, with or without this patch.
