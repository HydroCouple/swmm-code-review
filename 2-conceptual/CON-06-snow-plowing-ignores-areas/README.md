# CON-06: Snow plowed to another subcatchment changes volume by the ratio of the two subcatchments' areas

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | Plowing snow from one subcatchment to another multiplies its volume by A_receiving / A_donor. In the test, 1 in of snow plowed from 10 ac onto a 100 ac subcatchment becomes 1 in over 100 ac: the runoff continuity error is -81.8% (16.55 ac-ft of runoff from 9.17 ac-ft of precipitation). Onto a 1 ac subcatchment, 90% of the snow vanishes (+81.8%). The continuity error in the report is the only sign. |
| **Reached from** | `[SNOWPACKS]` REMOVAL with Fsub > 0 and a receiving subcatchment whose area (less LID area) differs from the donor's |
| **5.3.0** | `snow_plowSnow()` in [`src/legacy/engine/snow.c:481`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/snow.c#L481) and [`:486`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/snow.c#L486) |
| **5.2.4** | Same code, [`src/solver/snow.c:437`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/snow.c#L437) and [`:442`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/snow.c#L442) |
| **6.0.0** | Reproduces in `SnowSolver::plowSnow()`, [`src/engine/hydrology/Snow.cpp:546`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Snow.cpp#L546) |
| **Since** | SWMM 5.0 (present in the 5.0.022 code of the repository's first commit), so every release |
| **Fix** | Multiply each area fraction by its subcatchment's non-LID area: [`CON-06_swmm530.patch`](CON-06_swmm530.patch), [`CON-06_swmm600.patch`](CON-06_swmm600.patch) |

## The problem

A snow pack's REMOVAL line can send a fraction Fsub of the plowable snow to the pervious area of another subcatchment. Each subcatchment's pack stores depths on three surfaces, and `fArea[]` holds each surface's fraction of that subcatchment's area. To move a volume from the plowable area of subcatchment j onto the pervious area of subcatchment m, the depth has to be scaled by the ratio of the two areas in ft²:

    plowable area of j / pervious area of m = fArea_j[PLOWABLE] × A_j / (fArea_m[PERV] × A_m)

SWMM uses only the ratio of the fractions, `fArea_j[PLOWABLE] / fArea_m[PERV]`, so the volume is multiplied by A_m / A_j. The other plowing destinations (out of the system, onto the same subcatchment's impervious or pervious area) are handled correctly; only the transfer between subcatchments is wrong.

In the test, 1 in of snow falls on S1 (10 ac, all plowable impervious) and S2, and everything S1 collects is plowed onto S2. All snow melts within the 10-day run, with no infiltration or evaporation:

| Receiving S2 | Precipitation | Runoff | Runoff continuity error |
|---|---|---|---|
| 100 ac | 9.167 ac-ft | 16.554 ac-ft | -81.840% |
| 1 ac | 0.917 ac-ft | 0.167 ac-ft | +81.772% |

S1's 0.833 ac-ft of snow arrives on the 100 ac S2 as 1 in over 100 ac, 8.33 ac-ft. On the 1 ac S2 it arrives as 1 in over 1 ac, 0.083 ac-ft.

## Why it happens

```c
// src/legacy/engine/snow.c, snow_plowSnow()
            // --- plow out of system
            f = snowpack->fArea[SNOW_PLOWABLE] *
                (Subcatch[subcatchIndex].area - Subcatch[subcatchIndex].lidArea);
            Snow.removed += Snowmelt[k].sfrac[0] * exc * f;      // volume: uses the area
            ...
            // --- send to another subcatchment
            if ( Snowmelt[k].sfrac[4] > 0.0 )
            {
                m = Snowmelt[k].toSubcatch;
                if ( Subcatch[m].snowpack )
                {
                    f = Subcatch[m].snowpack->fArea[SNOW_PERV];  // fraction of A_m
                }
                else f = 0.0;
                if ( f > 0.0 )
                {
                    f = snowpack->fArea[SNOW_PLOWABLE] / f;      // fraction of A_j / fraction of A_m
                    Subcatch[m].snowpack->wsnow[SNOW_PERV] +=
                        Snowmelt[k].sfrac[4] * exc * f;
```

The snow cover in the runoff continuity balance (`snow_getSnowCover()`) is `wsnow × fArea × (area - lidArea)` for each surface, so the depth added to subcatchment m carries A_m where the depth removed from j carried A_j.

6.0.0's `SnowSolver::plowSnow()` has the same line:

```cpp
// src/engine/hydrology/Snow.cpp, SnowSolver::plowSnow()
                    double f = soa_.fArea[plow_idx] / soa_.fArea[target_perv];
                    const double moved = soa_.sfrac[sf + 4] * exc * f;
```

## How to reproduce

| File | What it is |
|---|---|
| [`CON-06_to-larger.inp`](CON-06_to-larger.inp) | 1 in of snow on S1 (10 ac, 100% plowable impervious) and S2 (100 ac pervious); S1's REMOVAL sends all plowed snow (Dplow 0.1 in) to S2; 60 °F from day 2; no infiltration or evaporation; 10 days |
| [`CON-06_to-smaller.inp`](CON-06_to-smaller.inp) | The same with S2 = 1 ac |
| [`CON-06_test.c`](CON-06_test.c) | Runs both decks through the legacy toolkit and checks the runoff continuity error from `swmm_getMassBalErr()` is within 1% |
| [`CON-06_test6.c`](CON-06_test6.c) | The same through the 6.0.0 C API (`swmm_get_runoff_continuity_error()`, `swmm_get_runoff_total()`) |

```sh
tools/run-test.sh CON-06            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-06 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4, 5.3.0 and 6.0.0 print the same):

```
Snow plowed from S1 (10 ac) to S2      volumes in acre-feet
Case          Precipitation  Runoff   Final snow  Final storage  Continuity error
S2 = 100 ac        9.167     16.554      0.000        0.115         -81.840 %
S2 = 1 ac          0.917      0.167      0.000        0.000          81.772 %
FAIL: plowing snow to another subcatchment changes its volume: runoff continuity errors -81.840% and 81.772%
CON-06 5.3.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Snow plowed from S1 (10 ac) to S2      volumes in acre-feet
Case          Precipitation  Runoff   Final snow  Final storage  Continuity error
S2 = 100 ac        9.167      9.056      0.000        0.112          -0.014 %
S2 = 1 ac          0.917      0.917      0.000        0.000          -0.055 %
PASS: plowed snow keeps its volume (runoff continuity errors -0.014% and -0.055%)
CON-06 5.3.0 patched: PASS
CON-06 6.0.0 patched: PASS
```

## The fix

```diff
                 if ( Subcatch[m].snowpack )
                 {
-                    f = Subcatch[m].snowpack->fArea[SNOW_PERV];
+                    f = Subcatch[m].snowpack->fArea[SNOW_PERV] *
+                        (Subcatch[m].area - Subcatch[m].lidArea);
                 }
                 else f = 0.0;
                 if ( f > 0.0 )
                 {
-                    f = snowpack->fArea[SNOW_PLOWABLE] / f;
+                    // --- ratio of plowable area to receiving pervious area
+                    f = snowpack->fArea[SNOW_PLOWABLE] *
+                        (Subcatch[subcatchIndex].area -
+                         Subcatch[subcatchIndex].lidArea) / f;
```

The existing `f > 0.0` test now also skips a receiving subcatchment that is entirely LID. The 6.0.0 patch computes the same two non-LID areas in `SnowSolver::plowSnow()`, the way its "plow out of system" branch already does; when the context holds no subcatchment areas (the solver driven on its own, as in the unit test `TransportSnowTest.PlowedSnowCarriesTheDonorsAgeAcrossSubcatchments`) it uses a ratio of 1.

Only models that plow snow between subcatchments of different sizes change. None of the 73 regression-suite decks uses snow packs; the two snow decks from the 6.0.0 test data (`tests/parity/snow/snow_parity.inp`, `python/tests/data/solver/site_drainage_snow.inp`), which do not use Fsub, give identical reports before and after the patch in both engines.
