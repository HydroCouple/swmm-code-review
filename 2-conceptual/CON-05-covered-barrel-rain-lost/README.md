# CON-05: The rain falling on a covered rain barrel disappears from the water balance

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | The rain on a covered barrel's footprint is counted as precipitation on the subcatchment but goes nowhere: it is neither stored, nor run off, nor infiltrated. The runoff continuity error equals the barrels' share of the subcatchment area, +11.4 % in the test, where 100 barrels of 50 ft2 cover 11.5 % of a 1 ac subcatchment. 5.2.4 loses it in every configuration; 5.3.0 only when the subcatchment has no pervious non-LID area to return it to. Small for realistic barrel areas, but systematic, and only the continuity error shows it. |
| **Reached from** | A rain barrel with `Covrd YES`: always in 5.2.4; in 5.3.0 on a 100 % impervious subcatchment or one whose LID units fill it |
| **5.3.0** | `lid_getRunoff()` in [`src/legacy/engine/lid.c:1705`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lid.c#L1705) returns the rain to the pervious area, and `subcatch_getRunon()` ([`src/legacy/engine/subcatch.c:593`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L593)) drops it when there is none |
| **5.2.4** | `getRainInflow()` returns 0 for a covered barrel ([`src/solver/lid.c:1745`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lid.c#L1745)) and the rain is not sent anywhere else |
| **6.0.0** | Ported from 5.3.0 (`RunoffSolver` drops the return flow when the pervious area is zero, [`src/engine/hydrology/Runoff.cpp:595`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L595)) but not reached, because 6.0.0 ignores the Covrd token ([IO-24](../../4-io/IO-24-rain-barrel-covered-default/)). With IO-24's fix alone, 6.0.0 loses the same rain as 5.3.0 |
| **Since** | 5.2.0, which added covered barrels; 5.3.0 (fork commit 9d3bb8d1, #166) returns the rain to the pervious area, which fixes it only where a pervious area exists |
| **Fix** | Send the rain to the LID surface runoff when it cannot be returned: [`CON-05_swmm530.patch`](CON-05_swmm530.patch), [`CON-05_swmm600.patch`](CON-05_swmm600.patch) (both require CON-04) |

## The problem

A covered rain barrel collects no rain, but rain still falls on its cover, and the runoff balance counts it: `subcatch_getRunoff()` books `rainfall * area` over the whole subcatchment as Total Precipitation. That water has to leave the cover somewhere.

- 5.2.4 only withholds it from the barrel. It is not added to any other store or flow, so it vanishes.
- 5.3.0 adds it to the LID group's flow returned to the pervious area. `subcatch_getRunon()` applies that flow only when the subcatchment has pervious non-LID area. On a fully impervious subcatchment, or one entirely covered by LID units, the water vanishes as in 5.2.4.

The test subcatchment is 1 ac with no infiltration, evaporation or seepage under 2.0 in of rain, with 100 covered barrels of 50 ft2 (5,000 ft2, 11.5 % of the area, no drain, FromImp = 0). Runoff plus final storage must equal rain plus initial LID storage:

| S1 | 5.2.4 | 5.3.0 | Fixed |
|---|---|---|---|
| half pervious | +11.377 % | 0.000 % | 0.000 % |
| 100 % impervious | +11.377 % | +11.377 % | 0.000 % |
| barrels + 38,560 ft2 rain garden fill S1 | +10.920 % | +10.920 % | 0.000 % |

On the impervious subcatchment S1 sends out 0.148 ac-ft (1.77 in) of the 0.167 ac-ft (2.00 in) that fell, and stores nothing. The missing 0.23 in is the rain on the barrel covers.

## Why it happens

5.2.4 takes the rain off the barrel's inflow and nothing else:

```c
// src/solver/lid.c (5.2.4), lid_getRunoff() and getRainInflow()
lidInflow = lidInflow + getRainInflow(j, lidUnit);
...
if (lidProc->lidType == RAIN_BARREL &&
    lidProc->storage.covered == TRUE) return 0.0;
return Subcatch[j].rainfall;
```

5.3.0 puts it into the return flow (`qReturn`, saved as `flowToPerv`):

```c
// src/legacy/engine/lid.c, lid_getRunoff()
if (lidProc->lidType == RAIN_BARREL &&
    lidProc->storage.covered == TRUE)
{
    // Add runoff from closed rain barrel to return (cfs)
    qReturn += subcatch->rainfall * subcatch->lidArea;     // area: CON-04
}
```

and the next step's `subcatch_getRunon()` spreads that flow over the pervious non-LID area, if there is one:

```c
// src/legacy/engine/subcatch.c, subcatch_getRunon()
if ( Subcatch[subcatchIndex].lidArea > 0.0 && Subcatch[subcatchIndex].fracImperv < 1.0 )
{
    pervArea = Subcatch[subcatchIndex].subArea[PERV].fArea *
               (Subcatch[subcatchIndex].area - Subcatch[subcatchIndex].lidArea);
    q = lid_getFlowToPerv(subcatchIndex);
    if ( pervArea > 0.0 )
    {
        Subcatch[subcatchIndex].subArea[PERV].inflow += q / pervArea;
    }
}
```

`evalLidUnit()` avoids the same trap for the units' own surface runoff: it returns it to the pervious area only if `toPerv` is set and `area > lidArea`, and `validateLidGroup()` clears `toPerv` on subcatchments that are 99.9 % impervious or more. Otherwise the runoff goes to `qRunoff`, the LID surface outflow. The covered-barrel branch has no such test.

6.0.0 has the same structure: `stepRunoff()` adds the rain to `lid_return_to_perv_cfs`, and `RunoffSolver` uses it only if `total_area > 0.0 && fp > 0.0` and then resets it. Run with IO-24's patch alone, 6.0.0's report shows continuity errors of 11.460 % and 10.995 % for the impervious and filled decks, the same as 5.3.0.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-05_pervious.inp`](CON-05_pervious.inp) | 1 ac subcatchment, 50 % impervious, no infiltration, 100 covered barrels of 50 ft2; 1 in/hr for 2 h |
| [`CON-05_imperv.inp`](CON-05_imperv.inp) | The same, 100 % impervious |
| [`CON-05_full-lid.inp`](CON-05_full-lid.inp) | The barrels plus a 38,560 ft2 rain garden that together fill the subcatchment |
| [`CON-05_test.c`](CON-05_test.c) | Runs the three decks through the legacy toolkit (5.2.4 and 5.3.0), reads the Runoff Quantity Continuity table and the barrels' Total Inflow; checks runoff + storage = rain within 1 % and that the covered barrels take no water |
| [`CON-05_test6.c`](CON-05_test6.c) | The same with the 6.0.0 engine API |

```sh
tools/run-test.sh CON-05            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-05 --patched  # 5.3.0 and 6.0.0 with IO-24, CON-04 and this fix: PASS
```

**Without the fix**, 5.2.4 loses the rain in all three decks:

```
S1                      rain    init LID  runoff  final    balance   barrel
                       (ac-ft)  (ac-ft)  (ac-ft)  (ac-ft)  error (%) inflow (in)
half pervious           0.167    0.000    0.148    0.000     11.377     0.00
100 % impervious        0.167    0.000    0.148    0.000     11.377     0.00
LIDs fill S1            0.167    0.007    0.118    0.037     10.920     0.00
FAIL: S1's runoff and storage do not balance the rain for 3 of 3 decks (more than 1 % off); the rain on the covered barrels is lost
CON-05 5.2.4 base: FAIL
```

5.3.0 loses it where there is no pervious area:

```
half pervious           0.167    0.000    0.167    0.000      0.000     0.00
100 % impervious        0.167    0.000    0.148    0.000     11.377     0.00
LIDs fill S1            0.167    0.007    0.118    0.037     10.920     0.00
FAIL: S1's runoff and storage do not balance the rain for 2 of 3 decks (more than 1 % off); the rain on the covered barrels is lost
CON-05 5.3.0 base: FAIL
```

6.0.0 balances because its barrels are not covered at all (IO-24):

```
half pervious           0.167    0.000    0.148    0.019      0.000     2.00
100 % impervious        0.167    0.000    0.148    0.019      0.000     2.00
LIDs fill S1            0.167    0.007    0.118    0.056      0.000     2.00
FAIL: the barrels declared covered (Covrd YES) collect rain
CON-05 6.0.0 base: FAIL
```

**With the fix** (5.3.0; 6.0.0 with IO-24, CON-04 and this patch prints the same values):

```
S1                      rain    init LID  runoff  final    balance   barrel
                       (ac-ft)  (ac-ft)  (ac-ft)  (ac-ft)  error (%) inflow (in)
half pervious           0.167    0.000    0.167    0.000      0.000     0.00
100 % impervious        0.167    0.000    0.167    0.000      0.000     0.00
LIDs fill S1            0.167    0.007    0.137    0.037      0.000     0.00
PASS: runoff + storage = rain within 1 % and the covered barrels take no rain, with and without a pervious area
CON-05 5.3.0 patched: PASS
CON-05 6.0.0 patched: PASS
```

## The fix

Keep 5.3.0's return to the pervious area where one exists, using the same test as `subcatch_getRunon()`, and otherwise add the rain to the LID group's surface runoff, which leaves through the subcatchment outlet like the units' own overflow. The patch is written on top of CON-04's:

```diff
-                // Add runoff from closed rain barrel to return (cfs)
-                qReturn += subcatch->rainfall * lidArea;
+                // Add runoff from closed rain barrel to return (cfs),
+                // or to LID surface runoff if there is no pervious area
+                // to return it to (see subcatch_getRunon)
+                if (subcatch->area > subcatch->lidArea &&
+                    subcatch->fracImperv < 1.0)
+                    qReturn += subcatch->rainfall * lidArea;
+                else qRunoff += subcatch->rainfall * lidArea;
```

6.0.0 makes the same choice with the non-LID area of the runoff solver and adds the rain to `lid_qsurf_cfs_` (legacy's `qRunoff`):

```diff
                     g.inflow[uu] = q_from_sc;
-                    ctx_.subcatches.lid_return_to_perv_cfs[usc] += rain * lid_area;
+                    // ... or to the LID surface runoff when there is no
+                    // pervious area to return it to (legacy subcatch_getRunon)
+                    if (rsoa.area[usc] > 0.0 && ctx_.subcatches.frac_imperv[usc] < 1.0)
+                        ctx_.subcatches.lid_return_to_perv_cfs[usc] += rain * lid_area;
+                    else
+                        lid_qsurf_cfs_[usc] += rain * lid_area;
```

5.2.4 is not patched. The return to the pervious area ignores the barrel row's ToPerv flag, as in 5.3.0; this patch leaves that choice alone.

**Effect on other models.** Only subcatchments with a covered rain barrel and no pervious non-LID area change, and no regression deck has a covered barrel. `examples/Example4.inp` gives byte-identical report and output files with the patched (IO-24 + CON-04 + CON-05) 5.3.0 and 6.0.0 command-line programs.
