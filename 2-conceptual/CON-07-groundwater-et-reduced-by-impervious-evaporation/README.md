# CON-07: Evaporation from the impervious surface reduces groundwater ET

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | Water ponded on a subcatchment's impervious area is charged against the evaporation available to its aquifer. Two subcatchments with identical pervious areas and aquifers: the one whose impervious half holds 0.5 in of ponded water loses 0.61 in to groundwater ET over 10 days, the one without ponding 0.81 in (-25 %), and its water table ends 0.04 ft higher. No warning; the totals look plausible. |
| **Reached from** | Any `[GROUNDWATER]` subcatchment with an impervious area that holds water (depression storage, or runoff still on the surface) while evaporation is active |
| **5.3.0** | `getSubareaRunoff()` adds the running total of all subareas' evaporation at the pervious call, [`src/legacy/engine/subcatch.c:984`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L984); `gwater_getGroundwater()` subtracts it from the aquifer's ET budget, [`subcatch.c:738`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L738), [`gwater.c:532`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L532) |
| **5.2.4** | Same code, [`src/solver/subcatch.c:951`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L951) and [`subcatch.c:716`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L716) |
| **6.0.0** | Reproduces on purpose for parity; the comment describes the effect, [`src/engine/hydrology/Runoff.cpp:483`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L483) |
| **Since** | 5.1.008 (fork history commit c70dd4fe). Before that only the pervious subarea's evaporation was passed on (`if ( i == PERV ) pervEvapVol += Vevap * area`, with `Vevap` per subarea) |
| **Fix** | Add only the pervious subarea's own evaporation to `Vpevap`: [`CON-07_swmm530.patch`](CON-07_swmm530.patch), [`CON-07_swmm600.patch`](CON-07_swmm600.patch) |

## The problem

Vol I sec. 5.3.2 allocates the potential evaporation in a fixed order: land surface evaporation first, then upper-zone ET, then lower-zone ET. Upper-zone ET is

f<sub>EU</sub> = min(e<sub>max</sub> - e<sub>s</sub>, UEF e<sub>max</sub>),  e<sub>max</sub> = e F<sub>perv</sub>   (5-11)

where e<sub>s</sub> "is the evaporation loss ... seen by any rainfall and ponded water on the pervious subcatchment surface". Lower-zone ET is limited to e<sub>max</sub> - e<sub>s</sub> - f<sub>EU</sub>. The groundwater zones lie under the pervious area and evaporate through it, so water on the impervious surface plays no part.

The test deck has two 1-acre subcatchments, 50 % impervious, each over its own copy of the same aquifer. It rains 0.5 in in the first hour, all of which infiltrates on the pervious halves (f = 5 in/hr), then evaporation runs at 0.2 in/day for 10 days. The only difference: S1's impervious half has 2 in of depression storage and keeps its 0.5 in until it evaporates, about 2.5 days; S2's has none, so its impervious rain runs off. The pervious halves and aquifers are identical, and so groundwater ET should be. All three engines report:

| Subcatchment | Impervious ponding | Groundwater ET (in) | Final water table (ft) |
|---|---|---|---|
| S1 | 0.5 in | 0.61 | 5.96 |
| S2 | none | 0.81 | 5.92 |

While S1's impervious surface evaporates at the full potential rate over half the area, its aquifer is left with no evaporation at all, because the impervious evaporation alone uses up e<sub>max</sub> = 0.5 e.

## Why it happens

`subcatch_getRunoff()` calls `getSubareaRunoff()` for `IMPERV0`, `IMPERV1` and `PERV`, in that order. Each call adds its surface evaporation to the shared total `Vevap`, and the pervious call then adds the total so far to `Vpevap`:

```c
// src/legacy/engine/subcatch.c, getSubareaRunoff()
    Vevap += surfEvap * area * tStep;
    if ( i == PERV ) Vpevap += Vevap;      // all three subareas, not just PERV
```

`Vpevap` is declared as "pervious area evaporation" and is passed to the aquifer as the surface evaporation already exerted:

```c
// subcatch_getRunoff()
        gwater_getGroundwater(subcatchIndex, Vpevap,  Vinfil+VlidInfil, tStep);

// src/legacy/engine/gwater.c, gwater_getGroundwater()
    evap = evap / Area / tStep;
    MaxEvap = Evap.rate * FracPerv;
    AvailEvap = MAX((MaxEvap - evap), 0.0);
```

In 5.1.007 and earlier `Vevap` was per subarea and the pervious total was `if ( i == PERV ) pervEvapVol += Vevap * area`. When 5.1.008 made `Vevap` a running total, the pervious line kept adding `Vevap`. LID units still add only the evaporation of pervious LIDs to `Vpevap` (`lid.c`, `if ( isLidPervious(...) ) Vpevap += lidEvap * tStep * lidArea`), which is the intended meaning.

6.0.0 reproduces the line for parity with legacy.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-07_impervious-ponding.inp`](CON-07_impervious-ponding.inp) | S1 (impervious depression storage 2 in) and S2 (none), otherwise identical, 10 days; the trailing `*` in `[GROUNDWATER]` works around IO-15 |
| [`CON-07_test.c`](CON-07_test.c) | Runs the deck through the legacy toolkit and compares the Total Evap column of S1 and S2 in the report's Groundwater Summary. Tolerance 0.01 in (the column has 2 decimals) |
| [`CON-07_test6.c`](CON-07_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh CON-07            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh CON-07 --patched  # 5.3.0, 6.0.0 with the fix: PASS
```

**Without the fix**, all three engines:

```
Subcatchment  Impervious ponding  Groundwater ET (in)
S1            0.5 in              0.61
S2            none                0.81
FAIL: ponded water on the impervious area changes groundwater ET (0.61 in vs 0.81 in with identical pervious areas)
CON-07 5.2.4 base: FAIL
CON-07 5.3.0 base: FAIL
CON-07 6.0.0 base: FAIL
```

**With the fix**, both engines:

```
Subcatchment  Impervious ponding  Groundwater ET (in)
S1            0.5 in              0.81
S2            none                0.81
PASS: groundwater ET does not depend on impervious ponding (0.81 in vs 0.81 in)
CON-07 5.3.0 patched: PASS
CON-07 6.0.0 patched: PASS
```

In the groundwater continuity table, Upper/Lower Zone ET go from 0.305/0.405 in to 0.349/0.463 in in both patched engines.

## The fix

```diff
     Vevap += surfEvap * area * tStep;
-    if ( i == PERV ) Vpevap += Vevap;
+    if ( i == PERV ) Vpevap += surfEvap * area * tStep;
```

6.0.0 makes the same change in `RunoffSolver::execute()`, `if (isPervious) Vpevap += surfEvap * subarea_area * dt;`, and replaces the parity comment. Total surface evaporation (`Vevap`, the runoff continuity table) is unchanged; only the evaporation the aquifer is charged for changes.

Effect on other models: groundwater ET rises wherever impervious surfaces hold water while evaporation is active, including the short time impervious runoff stays on the surface. The R4 deck `t3b_imperv_dry.inp` (no impervious depression storage) goes from Upper/Lower Zone ET 0.347/0.460 in to 0.349/0.463 in; its twin with 2 in of impervious storage, `t3a_imperv_ponded.inp`, goes from 0.263/0.349 in to the same 0.349/0.463 in, final aquifer storage from 50.038 to 49.839 in. `examples/Example5.inp` (the only regression deck with groundwater, 0 % impervious) and `examples/Example1.inp` give identical reports with both patched engines.
