# CON-24: The snow pack's cold content is capped 12 times below the manual's limit

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | After a cold spell a snow pack holds at most 1/12 of the cold content the Reference Manual allows, so it starts melting almost as soon as the air warms. In the test, a 4 in pack after 72 h at 2 °F holds 0.070 in of cold content instead of 0.840 in, and in the next 6 h at 50 °F it melts 1.010 in instead of 0.240 in. Nothing in the report shows it. |
| **Reached from** | Any `[SNOWPACKS]` model with air temperatures below the base melt temperature (cold content builds only then) |
| **5.3.0** | `updateColdContent()` in [`src/legacy/engine/snow.c:844`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/snow.c#L844) |
| **5.2.4** | Same code, [`src/solver/snow.c:789`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/snow.c#L789) |
| **6.0.0** | Reproduces in `updateColdContent()`, [`src/engine/hydrology/Snow.cpp:251`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Snow.cpp#L251) |
| **Since** | SWMM 5.0 (present in the 5.0.022 code of the repository's first commit), so every release |
| **Fix** | Drop the `/ 12.0`: [`CON-24_swmm530.patch`](CON-24_swmm530.patch), [`CON-24_swmm600.patch`](CON-24_swmm600.patch) |

## The problem

While the air is colder than the pack's antecedent temperature index (ATI), the pack builds "cold content": the depth of melt that has to be cancelled before liquid water leaves the pack once it warms. The Reference Manual (Vol. I, Sec. 6.6, step 8) limits it to

    COLDC <= 0.007 WSNOW (Tbase - ATI)

"which assumes a specific heat of snow of 0.007 inches water equivalent per °F". COLDC and WSNOW are both depths in inches of water equivalent, so 0.007 is a ratio per °F: the cold content of a pack per inch of its water equivalent and per degree below Tbase. It is the specific heat of water divided by the latent heat of fusion (1 cal/g/°C ÷ 80 cal/g ÷ 1.8 = 0.0069 per °F); the ice value is about half of that, 0.0035 per °F.

SWMM divides that limit by 12. The cap is 0.00058 × WSNOW × (Tbase - ATI), 12 times below the manual and 6 times below even the specific heat of ice.

In the test a 4 in pack sits for 72 h at 2 °F (Tbase 32 °F, ATI weight 0.5, melt coefficient 0.01 in/hr/°F, RNM 1) and then 6 h at 50 °F with no rain. ATI falls to 2.007 °F, and the cold content reaches its cap in the first time step and follows it from then on. The 6 warm hours have a degree-day melt potential of 0.01 × 18 × 6 = 1.08 in, of which the cold content is paid first:

| Cap | Cold content after the cold spell | Melt in 6 h at 50 °F | Pack left |
|---|---|---|---|
| Manual, 0.007 × 4 × 29.993 | 0.840 in | 0.240 in | 3.760 in |
| SWMM, the same / 12 | 0.070 in | 1.010 in | 2.990 in |

The test uses RNM = 1. With a smaller RNM each inch of cold content cancels 1/RNM inches of potential melt (`reduceColdContent()`), so the default 0.6 widens the gap. The effect is on the timing of melt after cold periods (earlier, larger melt and rain-on-snow runoff); the volume of snow is not changed.

## Why it happens

```c
// src/legacy/engine/snow.c, updateColdContent()
    double cc;                         // snow pack cold content (ft)
    double ccMax;                      // max. possible cold content (ft)
    ...
    // --- maximum cold content based on assumed specific heat of snow
    //     of 0.007 in. water equiv. per deg. F
    ccMax = snowpack->wsnow[i] * 0.007 / 12.0 * (Snowmelt[k].tbase[i] - ati);
    cc = MIN(cc, ccMax);
```

`wsnow` is in ft (`objects.h`: "depth of snow pack (ft)") and so is `cc` (it grows by `rnm × dhm × (ati - ta) × tStep` with `dhm` in ft/sec/°F, and is paid back in ft of melt by `reduceColdContent()`). Both sides of the comparison are in the same unit, so the `/ 12.0` is not an inch-to-foot conversion. The same constant is used correctly in `getRainmelt()` a few lines up, where `0.007 * rainfall * (Ta - 32)` is the melt in in/hr caused by rain falling at Ta, again as a plain ratio per °F.

6.0.0 copied the line:

```cpp
// src/engine/hydrology/Snow.cpp, updateColdContent()
    double ccMax = soa.wsnow[ui] * 0.007 / 12.0 * (soa.tbase[ui] - ati);
```

## How to reproduce

| File | What it is |
|---|---|
| [`CON-24_cold-spell.inp`](CON-24_cold-spell.inp) | A 4 in pack on a 1 ac pervious subcatchment; 72 h at 2 °F, then 6 h at 50 °F; no rain, infiltration, evaporation or free-water holding; 1-minute steps |
| [`CON-24_test.c`](CON-24_test.c) | Runs the deck through the legacy toolkit, reads "Final Snow Cover" from the report and compares it with the snow left by the manual's cap (3.760 in, ±0.05 in) |
| [`CON-24_test6.c`](CON-24_test6.c) | The same through the 6.0.0 C API (`swmm_get_runoff_total(SWMM_RUNOFF_FINALSNOW)`) |

```sh
tools/run-test.sh CON-24            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-24 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

The "This run" cold content is inferred from the snow left: melt potential − melt.

**Without the fix** (5.2.4 prints the same as 5.3.0; 6.0.0 shows 0.0701 in because it reads the unrounded volume):

```
4 in pack: 72 h at 2 degF (ATI -> 2.0073 degF), then 6 h at 50 degF
                                    Cold content (in)  Pack left (in)
Manual 0.007 x WSNOW x (Tbase-ATI)       0.8398             3.760
Same cap divided by 12                   0.0700             2.990
This run                                 0.0700             2.990
FAIL: 2.990 in of snow left instead of 3.760 in: the cold content was capped at 0.0700 in, not 0.007 x WSNOW x (Tbase - ATI) = 0.8398 in
CON-24 5.2.4 base: FAIL
CON-24 5.3.0 base: FAIL
CON-24 6.0.0 base: FAIL
```

**With the fix** (6.0.0 prints 0.8399 in the "This run" row):

```
4 in pack: 72 h at 2 degF (ATI -> 2.0073 degF), then 6 h at 50 degF
                                    Cold content (in)  Pack left (in)
Manual 0.007 x WSNOW x (Tbase-ATI)       0.8398             3.760
Same cap divided by 12                   0.0700             2.990
This run                                 0.8400             3.760
PASS: the cold content reaches 0.007 x WSNOW x (Tbase - ATI) = 0.8398 in and 3.760 in of snow is left
CON-24 5.3.0 patched: PASS
CON-24 6.0.0 patched: PASS
```

## The fix

```diff
     // --- maximum cold content based on assumed specific heat of snow
-    //     of 0.007 in. water equiv. per deg. F
-    ccMax = snowpack->wsnow[i] * 0.007 / 12.0 * (Snowmelt[k].tbase[i] - ati);
+    //     of 0.007 in. water equiv. per in. of snow per deg. F
+    ccMax = snowpack->wsnow[i] * 0.007 * (Snowmelt[k].tbase[i] - ati);
```

The 6.0.0 patch makes the same change in `Snow.cpp`. This follows the manual. A physically based cap would use the specific heat of ice, 0.0035 per °F; that would be a change to the documented model, not a bug fix, and is left to the maintainers.

Snow packs that go through cold spells will hold more cold content and melt later; models calibrated with the old cap may need their melt coefficients or RNM revisited. None of the 197 regression decks uses snow packs. Of the two snow decks in the 6.0.0 test data, `python/tests/data/solver/site_drainage_snow.inp` gives identical reports and output files before and after the patch in both engines, and `tests/parity/snow/snow_parity.inp` changes slightly at the outfall (flow frequency 86.97% to 86.94%, wet weather inflow 3.588 to 3.589 10⁶ gal), the same in both engines.
