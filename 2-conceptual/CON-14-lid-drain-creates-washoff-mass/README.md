# CON-14: Water released by a LID underdrain is counted as removed and as delivered, so pollutant mass is created

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | The runoff quality ledger of a subcatchment with a draining LID does not close: the BMP Removal row is overstated by the mass of water the LID later releases, so removal plus delivery exceeds the pollutant generated. In the test (a 1 ac bio-retention cell with an underdrain on a 10 ac subcatchment, EMC 100 mg/L) 46.58 lbs are generated, 39.12 lbs reported removed and 26.93 lbs delivered to the network: a -41.8 % runoff quality continuity error in 5.3.0 and 6.0.0, -8.1 % in all three engines when the cell takes half of the impervious runoff. The loads delivered to the network are not affected; the reported LID removal is too high. |
| **Reached from** | Any subcatchment with LID units and pollutants in which, in some runoff step, surface outflow plus LID drain flow exceeds the runoff generated before LID treatment (typically an underdrain emptying after a storm) |
| **5.3.0** | `surfqual_getWashoff()` in [`src/legacy/engine/surfqual.c:343`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/surfqual.c#L343) |
| **5.2.4** | Same booking, [`src/solver/surfqual.c:267`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/surfqual.c#L267). With full capture its error is +1.9 % instead, because its low-runoff cutoff ([NUM-58](../../1-numerical/NUM-58-washoff-cutoff-drops-load-524/)) loses about as much mass as this creates |
| **6.0.0** | Reproduces with the same numbers: `SWMMEngine::bookWashoffLoads()` in [`src/engine/core/SWMMEngine.cpp:4392`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L4392) |
| **Since** | 5.1.014, which added the `massLoad > 0.0` test; 5.1.013 booked the signed difference |
| **Fix** | Book the signed difference, so that a release reduces the BMP removal: [`CON-14_swmm530.patch`](CON-14_swmm530.patch), [`CON-14_swmm600.patch`](CON-14_swmm600.patch) |

## The problem

SWMM does not track pollutant mass inside LID units. Each runoff step, `surfqual_getWashoff()` turns the load generated on the subcatchment into one concentration over the runoff volume *before* LID treatment, `vOut1`, and charges that concentration to the volume that actually leaves, `vOut2` (surface outflow plus LID drain flow). The difference `vOut1 - vOut2` is the water the LIDs kept, and its mass is booked as infiltration or BMP removal.

That difference is booked only when it is positive. After a storm an underdrain keeps releasing stored water while the subcatchment's own runoff recedes, so `vOut2 > vOut1`. The released water leaves with the current concentration, which is delivered to the network, but no ledger term is reduced to pay for it. Its mass was already booked as BMP removal when the water was captured, so it is counted twice: once as removed, once as delivered.

The test subcatchment S1 is 10 ac, 50 % impervious, with a 1 ac bio-retention cell (12 in soil, 12 in storage, underdrain, no seepage) and an EMC of 100 mg/L under a 1.25 in storm. Nothing is swept, deposited or left on the surface, so generated = removed + delivered:

| Cell takes | Engine | Generated | BMP removal | Delivered to J1 | Balance error |
|---|---|---|---|---|---|
| all impervious runoff | 5.3.0, 6.0.0 | 46.576 lbs | 39.122 | 26.932 | -41.820 % |
| all impervious runoff | 5.2.4 | 46.579 lbs | 18.764 | 26.936 | +1.887 % |
| half of it | all engines | 46.576 lbs | 22.540 | 27.797 | -8.075 % |
| all impervious runoff | fixed | 46.576 lbs | 19.645 | 26.932 | -0.002 % |
| half of it | fixed | 46.576 lbs | 18.779 | 27.797 | 0.000 % |

The delivered load is the same before and after the fix; only the removal changes, to generated minus delivered.

In 5.2.4 the full-capture case happens to look right. When the cell captures all runoff and its drain has not started, the post-LID outflow is below `MIN_RUNOFF`, and 5.2.4 sets the concentration to zero for that step, so the load generated in those steps is booked nowhere (NUM-58). That loss and this creation nearly cancel (+1.9 %). The vendored 5.3.0 removed the cutoff (fork commit e8a8d107), which exposes the full error. When the cell takes only half of the runoff, the cutoff never applies and 5.2.4 gives the same -8.1 % as the other engines.

## Why it happens

```c
// src/legacy/engine/surfqual.c, surfqual_getWashoff()
cOut = 0.0;
if (vOut1 > 0.0)
    cOut = OutflowLoad[p] / vOut1;
...
if (Subcatch[subcatchIndex].lidArea > 0.0)
{
    vLost = vOut1 - vOut2;
    if (vLost < 0.0) vLost = 0.0;
    vInfil = MIN(vLidInfil, vLost);

    massLoad = cOut * vInfil * Pollut[p].mcf;
    if (massLoad > 0.0)
        massbal_updateLoadingTotals(INFIL_LOAD, p, massLoad);

    massLoad = cOut * (vLost - vInfil) * Pollut[p].mcf;
    if (massLoad > 0.0)
        massbal_updateLoadingTotals(BMP_REMOVAL_LOAD, p, massLoad);
}
...
massLoad = cOut * vSurfOut * Pollut[p].mcf;        // RUNOFF_LOAD
massbal_updateLoadingTotals(RUNOFF_LOAD, p, massLoad);
```

The drain's share is booked in `lid_addDrainLoads()` with the same concentration, `w = lidUnit->newDrainFlow * c[p] * tStep * ...`. Over a step the ledger therefore receives `cOut * vOut1` as generated load and `cOut * vOut2 + cOut * max(vOut1 - vOut2, 0)` as removed plus delivered, which is more whenever `vOut2 > vOut1`. Summed over the run the excess is `cOut * (vOut2 - vOut1)` over the release steps: 19.48 lbs in the full-capture deck (39.122 + 26.932 - 46.576).

5.2.4 has the same `if (massLoad > 0.0)` test on `cOut * (vOut1 - vOut2)`. 6.0.0 ports the 5.3.0 split with `std::max(v_out1 - v_out2, 0.0)` and `if (m_bmp > 0.0 ...)`.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-14_full-capture.inp`](CON-14_full-capture.inp) | 10 ac, 50 % impervious, 1 ac bio-retention cell with underdrain taking all impervious runoff, EMC 100 mg/L, 1 in/hr for 1.25 h, 2-day run (the review's deck) |
| [`CON-14_half-capture.inp`](CON-14_half-capture.inp) | The same with the cell taking half of the impervious runoff |
| [`CON-14_test.c`](CON-14_test.c) | Runs both decks through the legacy toolkit (5.2.4 and 5.3.0); reads the Runoff Quality Continuity table and the Wet Weather Inflow of the Quality Routing Continuity table and checks generated = removed + delivered within 1 % |
| [`CON-14_test6.c`](CON-14_test6.c) | The same with the 6.0.0 engine API |

The test takes the delivered load from the routing table rather than from the runoff table's Surface Runoff row, because 6.0.0's Surface Runoff row leaves out LID drain loads sent to the subcatchment's default outlet (it prints 0.000 for the full-capture deck), a separate reporting gap.

```sh
tools/run-test.sh CON-14            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-14 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 6.0.0 prints the same numbers):

```
LID takes        generated  BMP      delivered  balance
                  (lbs)     removal  to J1      error (%)
                            (lbs)    (lbs)
all imperv.       46.576    39.122    26.932   -41.820
half imperv.      46.576    22.540    27.797    -8.075
FAIL: removal + delivery does not equal the generated pollutant mass: full-capture 46.576 lbs generated, 39.122 removed + 26.932 delivered (-41.8 %); half-capture 46.576 lbs generated, 22.540 removed + 27.797 delivered (-8.1 %)
CON-14 5.3.0 base: FAIL
CON-14 6.0.0 base: FAIL
```

5.2.4:

```
all imperv.       46.579    18.764    26.936     1.887
half imperv.      46.579    22.542    27.798    -8.074
FAIL: removal + delivery does not equal the generated pollutant mass: full-capture 46.579 lbs generated, 18.764 removed + 26.936 delivered (1.9 %); half-capture 46.579 lbs generated, 22.542 removed + 27.798 delivered (-8.1 %)
CON-14 5.2.4 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same values):

```
LID takes        generated  BMP      delivered  balance
                  (lbs)     removal  to J1      error (%)
                            (lbs)    (lbs)
all imperv.       46.576    19.645    26.932    -0.002
half imperv.      46.576    18.779    27.797     0.000
PASS: generated mass = LID removal + mass delivered to the network within 1 %
CON-14 5.3.0 patched: PASS
CON-14 6.0.0 patched: PASS
```

## The fix

Book the signed difference: in a step where the LIDs release more than the subcatchment generates, the mass the extra water carries is taken back out of the BMP removal (a return from LID storage). Infiltration keeps its non-negative share:

```diff
+            //     (vLost < 0 when the LIDs release more water than the
+            //      subcatchment generates; the mass it carries is booked
+            //      as a negative BMP removal, i.e. a return from storage)
             vLost = vOut1 - vOut2;
-            if (vLost < 0.0) vLost = 0.0;
-            vInfil = MIN(vLidInfil, vLost);
+            vInfil = MIN(vLidInfil, MAX(vLost, 0.0));
 ...
             massLoad = cOut * (vLost - vInfil) * Pollut[p].mcf;
-            if (massLoad > 0.0)
+            if (massLoad != 0.0)
                 massbal_updateLoadingTotals(BMP_REMOVAL_LOAD, p, massLoad);
```

The 6.0.0 patch makes the same change in `bookWashoffLoads()`. This is what 5.1.013 did (`massLoad = cOut * (vOut1 - vOut2 - VlidReturn) * mcf` booked without a sign test). No concentration, load or flow changes; only the BMP Removal row of the Runoff Quality Continuity table and its continuity error do. A more physical treatment would carry a pollutant store in each LID unit and release it with the drain flow; that is a model change beyond this fix.

**Effect on other models.** No regression deck combines LID units with pollutants. `examples/Example1.inp` (pollutants, no LIDs) gives byte-identical results with the patched 5.3.0 and 6.0.0 command-line programs. A copy of `examples/Example4.inp` with an EMC pollutant (TSS, 100 mg/L on all subcatchments) gives identical output files; in the report only the runoff quality ledger changes: BMP Removal 130.025 -> 98.468 lbs and continuity error -13.872 % -> 0.000 % (5.3.0), -13.740 % -> 0.132 % (6.0.0, whose remaining 0.13 % is the Surface Runoff row gap noted above).
