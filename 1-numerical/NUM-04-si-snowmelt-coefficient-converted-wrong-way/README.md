# NUM-04: SI snow melt coefficients are multiplied by 1.8 instead of divided, so SI snow packs melt 3.24 times too fast

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | In every SI model with snow packs, dry-weather (degree-day) melt and cold-content build-up run 1.8² = 3.24 times faster than the `[SNOWPACKS]` coefficients say. In the test a 25.4 mm pack 10 °C above its base temperature melts 14.81 mm in an hour instead of 4.572 mm; the same model in US units melts 4.572 mm. No warning. US models are not affected. |
| **Reached from** | `[SNOWPACKS]` Cmin/Cmax in a model with CMS, LPS or MLD flow units |
| **5.3.0** | `setMeltParams()` in [`src/legacy/engine/snow.c:331`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/snow.c#L331), with the factor from the `Ucf` table in [`swmm5.c:149`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L149) |
| **5.2.4** | Same code, [`src/solver/snow.c:305`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/snow.c#L305) |
| **6.0.0** | Reproduces: `SWMMEngine::initHydrology()` copies the conversion, [`src/engine/core/SWMMEngine.cpp:8654`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L8654) |
| **Since** | SWMM 5.0 (the line is in the 5.0 `snow.c` of the repository's first commit), so every release |
| **Fix** | Divide by `UCF(TEMPERATURE)`: [`NUM-04_swmm530.patch`](NUM-04_swmm530.patch), [`NUM-04_swmm600.patch`](NUM-04_swmm600.patch) |

## The problem

The `[SNOWPACKS]` melt coefficients Cmin and Cmax are given in in/hr-°F (US) or mm/hr-°C (SI) (input file reference, `[SNOWPACKS]`). SWMM keeps temperatures in °F internally, so an SI coefficient has to become a coefficient per °F. A temperature difference of 1 °C is 1.8 °F, so a coefficient of C mm/hr per °C is C/1.8 mm/hr per °F. SWMM multiplies by 1.8 instead, so SI coefficients end up 3.24 times too large.

The coefficient (`dhm`) drives both the degree-day melt on dry days and the "negative melt" that builds cold content while the air is below the pack temperature. Rain-on-snow melt uses a separate energy balance and is not affected.

The test runs the same pack in both unit systems: 25.4 mm (1 in) of snow water equivalent on a pervious subcatchment, base temperature 0 °C (32 °F), air at 10 °C (50 °F), no rain, one hour, melt coefficient 0.4572 mm/hr-°C = 0.01 in/hr-°F. The degree-day equation gives 0.4572 × 10 × 1 = 4.572 mm (0.180 in):

| Units | Pack left after 1 h | Melt | Expected |
|---|---|---|---|
| US (CFS) | 0.820 in | 4.572 mm | 4.572 mm |
| SI (CMS) | 10.587 mm | 14.813 mm | 4.572 mm |

In a 24-hour version of the run, the SI pack is gone after 1.7 hours instead of 5.6, and the subcatchment's peak runoff is 0.13 m³/s; the US model peaks at 1.79 cfs (0.05 m³/s), as does the SI model once fixed.

## Why it happens

```c
// src/legacy/engine/snow.c, setMeltParams()
        // --- min/max melt coeffs.
        Snowmelt[j].dhmin[k]     = x[0] * UCF(TEMPERATURE) / UCF(RAINFALL);
        Snowmelt[j].dhmax[k]     = x[1] * UCF(TEMPERATURE) / UCF(RAINFALL);
```

```c
// src/legacy/engine/swmm5.c, Ucf[][]
        {1.0, 1.8},              // TEMPERATURE (deg F, deg C --> deg F)
```

`UCF(RAINFALL)` turns mm/hr into ft/sec correctly. The coefficient is then used against temperature differences in °F (`Temp.ta` and `tbase` are both converted to °F on input):

```c
// src/legacy/engine/snow.c, meltSnowpack()
         smelt = Snowmelt[k].dhm[i] * (Temp.ta - Snowmelt[k].tbase[i]);
```

In the test, `Temp.ta - tbase` = 18 °F. With `dhm` = 0.4572 × 1.8 mm/hr per °F the melt rate is 14.81 mm/hr; with the correct 0.4572 / 1.8 it is 4.572 mm/hr. `UCF(TEMPERATURE)` is used nowhere else in the engine.

6.0.0 copies the line, with a comment that gives the same reasoning:

```cpp
// src/engine/core/SWMMEngine.cpp, SWMMEngine::initHydrology()
                // legacy setMeltParams (snow.c): the melt coefficients are
                // per DEGREE, so an SI deck's mm/hr-degC also carries
                // UCF(TEMPERATURE) = 1.8; ...
                soa.dhmin[idx]  = p[0] * ucf::Ucf[ucf::TEMPERATURE][unit_sys_snow]
                                       / ucf::Ucf[ucf::RAINFALL][unit_sys_snow];
```

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-04_si.inp`](NUM-04_si.inp) | CMS: 4.0469 ha pervious subcatchment with a 25.4 mm pack, Cmin = Cmax = 0.4572 mm/hr-°C, Tbase 0 °C, air 10 °C, no rain, 1 hour |
| [`NUM-04_us.inp`](NUM-04_us.inp) | The same model in CFS: 10 ac, 1 in pack, 0.01 in/hr-°F, Tbase 32 °F, air 50 °F |
| [`NUM-04_test.c`](NUM-04_test.c) | Runs both decks through the legacy toolkit, reads "Final Snow Cover" from each report and checks the melt against 4.572 mm (5% tolerance; the bug gives 3.24 times as much) |
| [`NUM-04_test6.c`](NUM-04_test6.c) | The same through the 6.0.0 C API, reading the pack with `swmm_subcatch_get_snow_state()` |

```sh
tools/run-test.sh NUM-04            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-04 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 5.2.4 leaves 10.591 mm and 6.0.0 prints the same as 5.3.0):

```
Melt of a 25.4 mm pack in 1 h at 10 degC (18 degF) above base
Units  Pack left        Melt (mm)  Expected (mm)  Ratio
SI      10.587 mm        14.813      4.572       3.240
US       0.820 in         4.572      4.572       1.000
FAIL: the pack melted 14.813 mm in SI units and 4.572 mm in US units; the degree-day equation gives 4.572 mm in both
NUM-04 5.3.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Melt of a 25.4 mm pack in 1 h at 10 degC (18 degF) above base
Units  Pack left        Melt (mm)  Expected (mm)  Ratio
SI      20.828 mm         4.572      4.572       1.000
US       0.820 in         4.572      4.572       1.000
PASS: both unit systems melt 4.572 mm, as the degree-day equation gives
NUM-04 5.3.0 patched: PASS
NUM-04 6.0.0 patched: PASS
```

## The fix

```diff
         // --- min/max melt coeffs.
-        Snowmelt[j].dhmin[k]     = x[0] * UCF(TEMPERATURE) / UCF(RAINFALL);
-        Snowmelt[j].dhmax[k]     = x[1] * UCF(TEMPERATURE) / UCF(RAINFALL);
+        //     (a coefficient per deg C is 1.8 times one per deg F)
+        Snowmelt[j].dhmin[k]     = x[0] / UCF(TEMPERATURE) / UCF(RAINFALL);
+        Snowmelt[j].dhmax[k]     = x[1] / UCF(TEMPERATURE) / UCF(RAINFALL);
```

The 6.0.0 patch makes the same change in `SWMMEngine::initHydrology()` and corrects its comment.

US models do not change (`UCF(TEMPERATURE)` = 1). None of the 73 regression-suite decks uses snow packs; two snow decks from the 6.0.0 test data (`python/tests/data/solver/site_drainage_snow.inp` and `tests/parity/snow/snow_parity.inp`, both CFS) give identical reports before and after the patch in both engines. SI models with snow will melt more slowly. An SI model whose melt coefficients were calibrated against the old behaviour has to have Cmin and Cmax multiplied by 3.24 to reproduce its old results; the release notes should say so.
