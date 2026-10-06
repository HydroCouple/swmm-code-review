# NUM-38: Permeable pavement drains water it does not have once pavement and soil are saturated

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A permeable pavement with a soil layer that percolates more slowly than the underdrain can carry reports far more drain outflow than reaches the drain. In the test, 9.0 in of rain gives 10.115 in of underdrain flow, of which only 2.253 in reached the gravel bed; the runoff continuity error is -85.465 %. The continuity error is the only sign; there is no warning. |
| **Reached from** | `[LID_CONTROLS]` type `PP` with a `SOIL` layer and a `DRAIN` line, whenever the pavement and soil layers are full and the storage layer is not (a slow soil or sand filter over a gravel bed with a fast perforated pipe) |
| **5.3.0** | `pavementFluxRates()`, branch "soil and pavement layers are full", in [`src/legacy/engine/lidproc.c:985`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L985); the storage depth is then clamped at 0 in `modpuls_solve()` at [`lidproc.c:1593`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lidproc.c#L1593) |
| **5.2.4** | Same code, [`src/solver/lidproc.c:985`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lidproc.c#L985) |
| **6.0.0** | Reproduces with the same numbers: `LIDSolver::pavementFluxRates()` in [`src/engine/hydrology/LID.cpp:1048`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/LID.cpp#L1048) copies the legacy branch |
| **Since** | 5.1.008, which added the optional soil layer to permeable pavement; the 5.2.4 rework of the flux limits gave this branch an exfiltration limit but no drain limit |
| **Fix** | Limit the drain by the water above the drain offset, as the other branches do: [`NUM-38_swmm530.patch`](NUM-38_swmm530.patch), [`NUM-38_swmm600.patch`](NUM-38_swmm600.patch) |

## The problem

A permeable pavement can have three layers below its surface: pavement, an optional soil (sand) layer, and a gravel storage bed with an underdrain. Water reaches the gravel bed only by percolating through the soil. When the soil is slower than the drain, the drain can carry no more, on average, than the soil delivers.

The test deck is a 1-acre permeable pavement: 2 in of pavement over 2 in of soil (Ksat 0.5 in/hr) over a 12 in gravel bed with an impermeable floor and an underdrain at its bottom (C = 50, n = 0.5). 9.0 in of rain falls at 3 in/hr for 3 hours. There is no evaporation and no exfiltration, so the drain can release at most what is left after surface runoff and final storage: 2.253 in. SWMM reports 10.115 in of LID drainage and a runoff continuity error of -85.465 %. The LID Performance Summary shows the same: inflow 9.00 in, surface outflow 6.55 in, drain outflow 10.12 in, continuity error -85.47 %.

The LID report file shows how. Once the pavement fills at 00:23, the soil passes 0.5 in/hr to the gravel bed, but the drain flips between 6.972 in/hr and 0 every minute while the storage level flips between 0.019 in and 0:

```
                     Elapsed  Total     Soil   Drain    Storage
Date        Time       Hours  Inflow    Perc   OutFlow  Level
 01/01/2020 00:24:00   0.400  3.000    0.500   6.972    0.000
 01/01/2020 00:25:00   0.417  3.000    0.500   0.000    0.019
 01/01/2020 00:26:00   0.433  3.000    0.500   6.972    0.000
 01/01/2020 00:27:00   0.450  3.000    0.500   0.000    0.019
```

(columns selected from `NUM-38_lid.txt`, 5.3.0). On average the drain releases 3.49 in/hr from a bed that receives 0.5 in/hr, for as long as the pavement stays saturated.

## Why it happens

`pavementFluxRates()` computes each layer's potential rates and then limits them in one of four branches, chosen by which layers are full. In every branch but one the underdrain is limited by the water available to it. The branch for "soil and pavement layers are full" (the storage bed is not full, or the previous branch would have been taken) limits only the exfiltration:

```c
// src/legacy/engine/lidproc.c, pavementFluxRates()
    //... soil and pavement layers are full
    else if ( soilThickness > 0.0 &&
              paveDepth >= paveThickness &&
              soilTheta >= soilPorosity )
    {
        PavePerc = MIN(PavePerc, SoilPerc);
        SoilPerc = PavePerc;
        SurfaceInfil = MIN(SurfaceInfil,PavePerc); 
        maxRate = MAX(StorageVolume / Tstep + SoilPerc - StorageEvap, 0.0);
	    StorageExfil = MIN(StorageExfil, maxRate); 
    }
```

`StorageDrain` keeps the raw value of the drain equation, `C*(h - offset)^n`. At 0.019 in of water in the bed that is 50 x 0.019^0.5 = 6.97 in/hr. The bed's void fraction is 0.75/1.75 = 0.43, so it holds 0.008 in of water, while one 1-minute step at 6.97 in/hr removes 0.116 in. `modpuls_solve()` clamps the new storage depth at its lower limit of 0 (`x[i] = MAX(x[i], xMin[i])`), but the drain flux that drove it there is reported in full. The difference is created. With the bed empty the next step's drain is 0, the soil refills it to 0.019 in, and the cycle repeats.

The "no adjoining layers are full" branch right below has the limit that is missing here:

```c
        //... limit underdrain flow by volume above drain offset
        if ( StorageDrain > 0.0 )
        {
            maxRate = -StorageExfil - StorageEvap;
            if (storageDepth >= storageThickness ) maxRate += SoilPerc;
            if ( theLidProc->drain.offset <= storageDepth ) 
            {
                maxRate += (storageDepth - theLidProc->drain.offset) *
                           storageVoidFrac/Tstep;
            }
            maxRate = MAX(maxRate, 0.0);
            StorageDrain = MIN(StorageDrain, maxRate);
        }
```

`biocellFluxRates()` and `trenchFluxRates()` limit their drains the same way. Without a soil layer this branch is never taken. The same pavement and storm without the soil layer, or with C = 1 instead of 50 (a drain slower than the soil), closes its water balance (continuity error 0.004 %).

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-38_pp-slow-soil.inp`](NUM-38_pp-slow-soil.inp) | 1-acre permeable pavement: 2 in pavement, 2 in soil with Ksat 0.5 in/hr, 12 in gravel bed with no seepage, drain C = 50, n = 0.5 at the bottom; 9.0 in of rain at 3 in/hr; no evaporation; writes the LID report file `NUM-38_lid.txt` |
| [`NUM-38_test.c`](NUM-38_test.c) | Runs the deck through the legacy toolkit and checks from the runoff continuity table that the LID drainage does not exceed rain + initial storage - surface runoff - final storage, and that the continuity error is below 1 % |
| [`NUM-38_test6.c`](NUM-38_test6.c) | The same against the 6.0.0 C API |

```sh
tools/run-test.sh NUM-38            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-38 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4, 5.3.0 and 6.0.0 print the same):

```
Runoff continuity (in)
  initial LID storage     0.200
  precipitation           9.000
  surface runoff          6.547
  final storage           0.400
  LID drainage           10.115   (water available for it: 2.253)
  continuity error (%)  -85.465
FAIL: the underdrain released 10.115 in but only 2.253 in reached it; continuity error -85.465 %
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Runoff continuity (in)
  initial LID storage     0.200
  precipitation           9.000
  surface runoff          6.547
  final storage           0.400
  LID drainage            2.252   (water available for it: 2.253)
  continuity error (%)    0.004
PASS: the underdrain releases only the water that reaches it (2.252 in), continuity error 0.004 %
```

With the fix the LID report file shows a steady drain of 0.500 in/hr, equal to the soil percolation, at a storage level of 0.019 in from 00:19 to 03:33, after which both recede.

## The fix

Add to the "soil and pavement layers are full" branch the drain limit of the "no adjoining layers are full" branch. The storage bed is never full in this branch, so the `SoilPerc` term of that limit does not apply:

```diff
         maxRate = MAX(StorageVolume / Tstep + SoilPerc - StorageEvap, 0.0);
 	    StorageExfil = MIN(StorageExfil, maxRate); 
+
+        //... limit underdrain flow by volume above drain offset
+        if ( StorageDrain > 0.0 )
+        {
+            maxRate = -StorageExfil - StorageEvap;
+            if ( theLidProc->drain.offset <= storageDepth )
+            {
+                maxRate += (storageDepth - theLidProc->drain.offset) *
+                           storageVoidFrac/Tstep;
+            }
+            maxRate = MAX(maxRate, 0.0);
+            StorageDrain = MIN(StorageDrain, maxRate);
+        }
     }
```

The 6.0.0 patch adds the same lines to `LIDSolver::pavementFluxRates()`. The drain of a pavement whose soil limits the flow now carries what the soil delivers, and the water balance closes.

Effect on other models: the change only acts in permeable pavements with a soil layer while the pavement and soil are saturated. No regression deck has such a pavement. As a check, the 5.3.0 and 6.0.0 CLIs built with the patch give binary output files identical to the unpatched builds for `examples/Example4.inp` and `update_v5111/porous_pavement.inp`, and the same runoff and routing continuity errors for the 24 `swc/` decks (rain gardens, green roofs, planters and permeable pavement without soil).
