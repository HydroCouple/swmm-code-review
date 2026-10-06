# NUM-39: A bio-retention cell or rain garden without a storage layer drains water that no layer loses

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | The underdrain runs for as long as the soil is above field capacity, but its flow is taken from no layer, so it creates water. In the test, 2.0 in of rain on a 1-acre unit gives 17.173 in of LID drainage over 12 hours, while the soil keeps all the water it received; the runoff continuity error is -536.667 %. There is no error or warning. |
| **Reached from** | `[LID_CONTROLS]` type `BC` with a `STORAGE` thickness of 0 or no `STORAGE` line, or type `RG`, together with a `DRAIN` line whose coefficient is > 0 |
| **5.3.0** | `biocellFluxRates()` in [`src/legacy/engine/lidproc.c:649`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L649): the drain rate from [line 644](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L644) is not limited, `f[STOR]` is 0 ([line 728](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L728)), yet the rate is reported as drain flow ([line 352](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L352)) |
| **5.2.4** | Same code, [`src/solver/lidproc.c:649`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lidproc.c#L649) |
| **6.0.0** | Reproduces with the same numbers: `LIDSolver::biocellFluxRates()` in [`src/engine/hydrology/LID.cpp:860`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/LID.cpp#L860) copies the legacy branch |
| **Since** | At least 5.1.011, the oldest source in the repository history |
| **Fix** | Pass the soil percolation to the drain, after exfiltration, as for a full storage layer: [`NUM-39_swmm530.patch`](NUM-39_swmm530.patch), [`NUM-39_swmm600.patch`](NUM-39_swmm600.patch) |

## The problem

A rain garden is a bio-retention cell with only a surface and a soil layer. SWMM accepts a `DRAIN` line for it, and for a bio-retention cell whose storage layer is 0 in thick or missing (the EPA Stormwater Calculator writes its rain gardens as `BC` with a 0 in storage layer). Nothing rejects or warns about the drain. A designer reads this as an underdrain at the bottom of the soil.

The test deck is a 1-acre unit: 6 in of ponding over 12 in of soil (porosity 0.5, field capacity 0.2, Ksat 0.5 in/hr), no seepage, and a drain with C = 1, n = 0.5. 2.0 in of rain falls in 2 hours, with no evaporation. The soil starts at its wilting point (1.2 in of water) and ends with 3.2 in: it kept every inch of rain. The run also reports 17.173 in of LID drainage, 8.6 times the rainfall, and a runoff continuity error of -536.667 %. The rain garden (`RG`) and the bio-retention cell with a 0 in storage layer give the same numbers, and so does a `BC` with no `STORAGE` line.

## Why it happens

`biocellFluxRates()` computes the drain rate before it looks at the storage layer:

```c
// src/legacy/engine/lidproc.c, biocellFluxRates()
    //... underdrain flow rate
    StorageDrain = 0.0;
    if ( theLidProc->drain.coeff > 0.0 )
    {
        StorageDrain = getStorageDrainRate(storageDepth, soilTheta, 0.0,
                                           surfaceDepth);
    }

    //... special case of no storage layer present
    if ( storageThickness == 0.0 )
    {
        StorageEvap = 0.0;
        maxRate = MIN(SoilPerc, StorageExfil);
        SoilPerc = maxRate;
        StorageExfil = maxRate;
        ...
    }
```

With a storage depth of 0 and a thickness of 0, `getStorageDrainRate()` treats the storage layer as full and adds the head of the soil water above field capacity, so the drain runs whenever the soil is wetter than field capacity. The no-storage branch then ties the soil percolation to the exfiltration rate and leaves `StorageDrain` alone. The layer balances are

```c
    f[SOIL] = (SurfaceInfil - SoilEvap - SoilPerc) / theLidProc->soil.thickness;
    if ( storageThickness == 0.0 ) f[STOR] = 0.0;
```

so the drain flow is subtracted from no layer. `lidproc_getOutflow()` still returns it as `*lidDrain = StorageDrain`, and it is added to the subcatchment's outflow and to the LID drainage in the water balance. In the test the soil never falls back to field capacity (there is no seepage and its percolation is capped at the exfiltration rate of 0). The drain starts at 01:14, when the soil passes field capacity, and by 02:32 it reaches 1.633 in/hr, which it holds to the end of the run: the drain equation at the head of the soil water above field capacity (moisture 0.267: (0.267 - 0.2)/(0.5 - 0.2) x 12 in = 2.67 in, and 1 x 2.67^0.5 = 1.63 in/hr).

`validateLidProc()` only sets the storage void fraction to 1 and the drain offset to 0 when there is no storage layer. It does not reject a drain there, and it does not require the storage layer that the input reference lists as required for `BC` (the Users Manual lists it as optional, and the Stormwater Calculator relies on that).

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-39_bc-no-storage.inp`](NUM-39_bc-no-storage.inp) | 1-acre bio-retention cell: 6 in ponding, 12 in soil (Ksat 0.5 in/hr), `STORAGE` thickness 0 with no seepage, drain C = 1, n = 0.5; 2.0 in of rain in 2 hours; no evaporation |
| [`NUM-39_rg-drain.inp`](NUM-39_rg-drain.inp) | The same as a rain garden (`RG`, no `STORAGE` line) |
| [`NUM-39_test.c`](NUM-39_test.c) | Runs both decks through the legacy toolkit and checks from the runoff continuity table that the LID drainage does not exceed rain + initial storage - surface runoff - final storage, and that the continuity error is below 1 % |
| [`NUM-39_test6.c`](NUM-39_test6.c) | The same against the 6.0.0 C API |

```sh
tools/run-test.sh NUM-39            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-39 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4, 5.3.0 and 6.0.0 print the same):

```
Runoff continuity (in)
LID                       init    rain  runoff   final    drain available  error (%)
BC, 0 in storage         1.200   2.000   0.000   3.200   17.173     0.000   -536.667
RG with a drain          1.200   2.000   0.000   3.200   17.173     0.000   -536.667
FAIL: BC, 0 in storage: the underdrain released 17.173 in but only 0.000 in left the unit's layers; continuity error -536.667 %
```

**With the fix**, 5.3.0:

```
Runoff continuity (in)
LID                       init    rain  runoff   final    drain available  error (%)
BC, 0 in storage         1.200   2.000   0.000   2.774    0.427     0.426     -0.015
RG with a drain          1.200   2.000   0.000   2.774    0.427     0.426     -0.015
PASS: without a storage layer the underdrain releases only water that leaves the soil; continuity closes
```

and 6.0.0:

```
Runoff continuity (in)
LID                       init    rain  runoff   final    drain available  error (%)
BC, 0 in storage         1.200   2.000   0.000   2.773    0.427     0.427      0.002
RG with a drain          1.200   2.000   0.000   2.773    0.427     0.427      0.002
PASS: without a storage layer the underdrain releases only water that leaves the soil; continuity closes
```

The drain now carries the 0.427 in that percolates out of the soil. The 0.001 in difference in final storage between the two patched engines is not from the patch: the same unit with 0.1 in/hr of seepage and no drain, where the same 0.427 in leaves as exfiltration, gives 2.774 in and -0.015 % in unpatched 5.3.0 and 2.773 in and 0.002 % in unpatched 6.0.0.

## The fix

A storage layer of zero thickness is always full, and `getStorageDrainRate()` already treats it that way. Apply the rule of the "storage and soil layers are full" branch below it: the soil percolates no faster than exfiltration and drain together can take, exfiltration takes its share first, and the drain takes the rest:

```diff
     if ( storageThickness == 0.0 )
     {
         StorageEvap = 0.0;
-        maxRate = MIN(SoilPerc, StorageExfil);
+        maxRate = MIN(SoilPerc, StorageExfil + StorageDrain);
         SoilPerc = maxRate;
-        StorageExfil = maxRate;
+        StorageExfil = MIN(StorageExfil, maxRate);
+        StorageDrain = maxRate - StorageExfil;
```

The drain flow now leaves the soil layer, so the water balance closes. Without a drain (`StorageDrain` = 0) the new lines compute exactly what the old ones did. The 6.0.0 patch makes the same change in `LIDSolver::biocellFluxRates()`.

The patch does not add an input check. Rejecting a `BC` without a storage layer, as the input reference's table would suggest, would break the Stormwater Calculator's rain gardens.

Effect on other models: the change only acts in bio-retention cells and rain gardens without a storage layer that have a drain, and no regression deck has one. The rain gardens of `swc/swc12.inp` and `swc/swc17.inp` (a `BC` with a 0 in storage layer and no drain) and `update_v5111/bioretention.inp` and `update_v5111/rain_garden.inp` give binary output files identical to the unpatched builds in both 5.3.0 and 6.0.0.
