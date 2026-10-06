# NUM-42: RDII ignores a gage's rain scale factor when the gage shares its series with an earlier gage

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | RDII from the unit hydrograph is computed with the other gage's scale factor: in the test it is half of what it should be (scale 2.0 replaced by 1.0). Subcatchments on the same gage do get their own factor. No warning. |
| **Reached from** | A [HYDROGRAPHS] gage that has the 5.3.0 rain scale factor (last token of its [RAINGAGES] line) and reads the same time series as a gage declared before it that is in use |
| **5.3.0** | `initGageData()` in [`src/legacy/engine/rdii.c:1101`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rdii.c#L1101) |
| **5.2.4** | Not affected: the same re-pointing ([`src/solver/rdii.c:1064`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/rdii.c#L1064)) is harmless because gages sharing a series always have the same rainfall; 5.2.4 has no scale factor |
| **6.0.0** | Not affected: each RDII group reads its own gage's rainfall ([`src/engine/hydrology/RDII.cpp:317`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/RDII.cpp#L317)), which includes the scale factor |
| **Since** | 5.3.0, fork commit e791d7ed ("feat(gage): add rainfall scaling factor with co-gage support") |
| **Fix** | Keep the UH's own gage and rescale the co-gage's rainfall: [`NUM-42_swmm530.patch`](NUM-42_swmm530.patch) (requires [NUM-41](../NUM-41-rdii-rainfall-point-sampled/)) |

## The problem

5.3.0 added a rain scale factor to each gage. Two gages may read the same time series with different factors, for example a measured record transferred to a second catchment at 2.0 times its depth. Subcatchments on the second gage get the scaled rain. An RDII unit hydrograph on the same gage does not, as soon as an earlier gage that is in use reads the same series.

In the test, unit hydrograph UH1 (R = 0.1, 100 ac) is on gage G2, which reads 1.0 in from series TS1 with scale factor 2.0. RDII must be 0.1 x 2.0 in x 100 ac = 72,600 ft3. That is what 5.3.0 gives when G2 is the only gage on TS1. Add gage G1 (scale 1.0) on TS1 before G2, used by a subcatchment elsewhere in the model, and RDII on G2 drops to 36,301 ft3. The RDII continuity table shows the halved figure as "Sewershed Rainfall" (8.333 instead of 16.667 ac-ft), so it is consistent with itself.

## Why it happens

Two gages that read the same series cannot both step through it, so `gage_validate()` makes the later gage a "co-gage" of the first one, and `gage_setState()` copies the first gage's rainfall into it. 5.3.0 added the scale factor to that copy:

```c
// src/legacy/engine/gage.c, gage_setState()
    if ( Gage[j].coGage >= 0)
    {
        int cg = Gage[j].coGage;
        // Apply rainfall scaling factor to co-gage's rainfall
        Gage[j].rainfall = Gage[cg].rainfall * Gage[j].scaleFactor / Gage[cg].scaleFactor;
        return;
    }
```

RDII does not go through that path. Before the RDII pass, `initGageData()` re-points the unit hydrograph to the co-gage itself, and `getRainfall()` then reads the co-gage's rainfall, scaled by the co-gage's factor:

```c
// src/legacy/engine/rdii.c, initGageData()
        g = UnitHyd[i].rainGage;
        if ( g >= 0 )
        {
            // --- if UH's gage uses same time series as a previous gage,
            //     then assign the latter gage to the UH
            if ( Gage[g].coGage >= 0 )
            {
                UnitHyd[i].rainGage = Gage[g].coGage;
            }
        }
```

Up to 5.2.4 a gage and its co-gage always had the same rainfall, so this was only a shortcut. The commit that added the scale factor updated `gage_setState()` and `gage_setReportRainfall()` but not this.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-42_own-series.inp`](NUM-42_own-series.inp) | Control: UH1 on gage G2 (scale 2.0), the only gage on series TS1 (1.0 in in the first hour) |
| [`NUM-42_shared-series.inp`](NUM-42_shared-series.inp) | The same plus gage G1 (scale 1.0) on TS1, declared first and used by subcatchment S1, which drains to a separate outfall |
| [`NUM-42_test.c`](NUM-42_test.c) | Runs both decks through the legacy toolkit and integrates the lateral inflow of node J1 (RDII only) |
| [`NUM-42_test6.c`](NUM-42_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-42            # 5.2.4: PASS, 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh NUM-42 --patched  # 5.3.0 and 6.0.0 with NUM-41 and NUM-42 applied: PASS
```

**Without the fix**, 5.3.0:

```
Expected RDII volume R x P x SF x A = 0.1 x 1 in x 2.0 x 100 ac = 72600 ft3

deck                       RDII volume (ft3)  ratio
NUM-42_own-series.inp         72602.7         1.000
NUM-42_shared-series.inp      36301.3         0.500
FAIL: RDII on gage G2 is 36301 ft3 when G2 shares its series with G1 and 72603 ft3 when it does not (expected 72600)
NUM-42 5.3.0 base: FAIL
```

6.0.0 gives 72602.7 ft3 for both decks. 5.2.4 has no scale factor and ignores the extra token, so the test expects the unscaled volume there and gets it for both decks:

```
SWMM 52004 has no rain scale factor: expecting the unscaled volume
Expected RDII volume R x P x SF x A = 0.1 x 1 in x 1.0 x 100 ac = 36300 ft3

deck                       RDII volume (ft3)  ratio
NUM-42_own-series.inp         36301.3         1.000
NUM-42_shared-series.inp      36301.3         1.000
NUM-42 5.2.4 base: PASS
```

**With the fix**, 5.3.0 (and 6.0.0, unchanged):

```
deck                       RDII volume (ft3)  ratio
NUM-42_own-series.inp         72602.7         1.000
NUM-42_shared-series.inp      72602.7         1.000
PASS: RDII on G2 is R x P x SF x A whether or not G2 shares its series
NUM-42 5.3.0 patched: PASS
```

The RDII continuity table of the shared deck then reads 16.667 ac-ft of sewershed rainfall and 1.667 ac-ft of RDII.

## The fix

`initGageData()` no longer re-points the unit hydrograph. In `getRainfall()`, a gage with a co-gage takes the co-gage's average rainfall over the RDII step (computed once per step, so the co-gage's record is still read forward only) times the ratio of the scale factors, the same rule as `gage_setState()`:

```diff
             if (!Gage[g].isCurrent)
             {
-                GageRain[g] = getGageRainfall(g, prevDate, currentDate);
+                // --- a gage sharing its rainfall record with an earlier
+                //     gage (its co-gage) uses the co-gage's rainfall,
+                //     rescaled by the ratio of their scale factors
+                cg = Gage[g].coGage;
+                if ( cg >= 0 )
+                {
+                    if (!Gage[cg].isCurrent)
+                    {
+                        GageRain[cg] = getGageRainfall(cg, prevDate, currentDate);
+                        Gage[cg].isCurrent = TRUE;
+                    }
+                    GageRain[g] = GageRain[cg] * Gage[g].scaleFactor /
+                                  Gage[cg].scaleFactor;
+                }
+                else GageRain[g] = getGageRainfall(g, prevDate, currentDate);
                 Gage[g].isCurrent = TRUE;
             }
```

The patch is written on top of [NUM-41](../NUM-41-rdii-rainfall-point-sampled/), which replaces the same lines (`GageRain[]` and `getGageRainfall()` come from it). When the two gages have equal scale factors, `x * 1.0 / 1.0` is `x`, so models without scale factors give bit-identical results. 6.0.0 needs no change. No regression-suite deck uses RDII.
