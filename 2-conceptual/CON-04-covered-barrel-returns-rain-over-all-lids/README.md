# CON-04: A covered rain barrel returns the rain that fell on every LID unit of its subcatchment

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | Each covered rain barrel row sends the rain falling on the subcatchment's whole LID area to the pervious area, although the other LID units also take that rain as their own inflow. The duplicated rain has no source in the water balance. In the test (20 covered barrels and a 2.3 ac rain garden on a 10 ac subcatchment) the runoff is 1.968 ac-ft instead of 1.585 ac-ft and the runoff continuity error is -22.7 %. Only the continuity error shows it. |
| **Reached from** | A subcatchment with a covered rain barrel (`Covrd YES`) and any other LID unit, or with more than one covered-barrel row in `[LID_USAGE]` |
| **5.3.0** | `lid_getRunoff()` in [`src/legacy/engine/lid.c:1705`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lid.c#L1705) |
| **5.2.4** | Not affected: a covered barrel's rain is not returned at all ([`src/solver/lid.c:1668`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lid.c#L1668)); it is lost instead, see [CON-05](../CON-05-covered-barrel-rain-lost/) |
| **6.0.0** | The same formula is ported in `SWMMEngine::stepRunoff()` ([`src/engine/core/SWMMEngine.cpp:2497`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L2497)) but never runs, because 6.0.0 ignores the Covrd token ([IO-24](../../4-io/IO-24-rain-barrel-covered-default/)). With IO-24's fix alone, 6.0.0 reproduces 5.3.0 exactly |
| **Since** | 5.3.0, fork commit 9d3bb8d1 ("Add runoff from closed rain barrel to return flow #166", 2024-03-15; not in USEPA develop) |
| **Fix** | Use the barrel's own area: [`CON-04_swmm530.patch`](CON-04_swmm530.patch), [`CON-04_swmm600.patch`](CON-04_swmm600.patch) (requires IO-24) |

## The problem

A covered rain barrel takes no direct rainfall. 5.3.0 sends the rain falling on its cover to the subcatchment's pervious area instead, so that it is not lost (5.2.4 drops it, [CON-05](../CON-05-covered-barrel-rain-lost/)). The amount returned is computed over the wrong area: the total area of all LID units in the subcatchment instead of the barrel's own.

The test subcatchment is 10 ac, half pervious, with no infiltration, evaporation or seepage, under 2.0 in of rain. It holds 20 covered barrels of 50 ft2 (1,000 ft2, no drain, FromImp = 0) and, in the second deck, a 100,000 ft2 (2.3 ac) rain garden that also takes no runoff. Runoff plus final storage must equal rain plus initial LID storage:

| S1 LIDs | Engine | Rain | Initial LID storage | Runoff | Final storage | Balance error |
|---|---|---|---|---|---|---|
| barrels only | 5.3.0 | 1.667 ac-ft | 0.000 | 1.661 | 0.006 | 0.000 % |
| barrels + rain garden | 5.3.0 | 1.667 ac-ft | 0.019 | 1.968 | 0.101 | -22.716 % |
| barrels + rain garden | 5.2.4 | 1.667 ac-ft | 0.019 | 1.581 | 0.101 | +0.237 % |

With the rain garden, S1 sends out 0.383 ac-ft more than it should: the 2.0 in that fell on the rain garden's 100,000 ft2, which the rain garden took as inflow and the barrel row returned to the pervious area a second time. With the barrels alone the two areas coincide and the balance closes. 5.2.4's +0.237 % is the rain on the 1,000 ft2 of barrel covers, which it drops (CON-05).

The same error appears when a subcatchment has two covered-barrel rows: each returns the rain on both.

## Why it happens

`lid_getRunoff()` loops over the subcatchment's LID units. `lidArea` is the area of the unit being evaluated (`area * number`); `subcatch->lidArea` is the summed area of all units:

```c
// src/legacy/engine/lid.c, lid_getRunoff()
lidArea = lidUnit->area * lidUnit->number;
...
TLidProc* lidProc = &LidProcs[lidUnit->lidIndex];
if (lidProc->lidType == RAIN_BARREL &&
    lidProc->storage.covered == TRUE)
{
    // Add runoff from closed rain barrel to return (cfs)
    qReturn += subcatch->rainfall * subcatch->lidArea;
}
else
{
    //... add rainfall onto LID inflow (ft/s)
    lidInflow = lidInflow + subcatch->rainfall;
}
```

`qReturn` becomes the group's `flowToPerv`, which `subcatch_getRunon()` adds to the pervious subarea's inflow at the next step. The rain on the rain garden therefore enters the rain garden through the `else` branch and the pervious area through the barrel's branch.

6.0.0 copies the expression with `total_lid_area_ft2`:

```cpp
// src/engine/core/SWMMEngine.cpp, SWMMEngine::stepRunoff()
if (g.type == lid::LIDType::RAIN_BARREL && g.stor_covered[uu]) {
    g.inflow[uu] = q_from_sc;
    if (usc < ctx_.subcatches.total_lid_area_ft2.size())
        ctx_.subcatches.lid_return_to_perv_cfs[usc] +=
            rain * ctx_.subcatches.total_lid_area_ft2[usc];
}
```

`stor_covered` is never set from the input (IO-24), so the branch is dead in 6.0.0 as released. Run with IO-24's patch alone, 6.0.0's report gives the same continuity errors as 5.3.0's for both decks (-22.704 % and -0.008 %).

## How to reproduce

| File | What it is |
|---|---|
| [`CON-04_barrel-only.inp`](CON-04_barrel-only.inp) | 10 ac subcatchment, 50 % impervious, no infiltration, 20 covered barrels of 50 ft2 (control case); 1 in/hr for 2 h |
| [`CON-04_barrel-plus-rg.inp`](CON-04_barrel-plus-rg.inp) | The same plus a 100,000 ft2 rain garden with no seepage |
| [`CON-04_test.c`](CON-04_test.c) | Runs both decks through the legacy toolkit (5.2.4 and 5.3.0), reads the Runoff Quantity Continuity table and the barrels' Total Inflow from the LID Performance Summary; checks runoff + storage = rain within 1 % and that the covered barrels take no water |
| [`CON-04_test6.c`](CON-04_test6.c) | The same with the 6.0.0 engine API |

```sh
tools/run-test.sh CON-04            # 5.2.4: PASS; 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-04 --patched  # 5.3.0 and 6.0.0 (with IO-24) patched: PASS
```

**Without the fix**, 5.3.0 creates the rain garden's rain a second time:

```
S1 LIDs                 rain    init LID  runoff  final    balance   barrel
                       (ac-ft)  (ac-ft)  (ac-ft)  (ac-ft)  error (%) inflow (in)
barrels only            1.667    0.000    1.661    0.006      0.000     0.00
barrels + rain garden   1.667    0.019    1.968    0.101    -22.716     0.00
FAIL: S1's runoff and storage do not balance the rain for 1 of 2 decks (more than 1 % off); a covered barrel returns the rain on every LID unit
CON-04 5.3.0 base: FAIL
```

5.2.4 balances within 1 % (the +0.24 % is CON-05):

```
barrels only            1.667    0.000    1.657    0.006      0.240     0.00
barrels + rain garden   1.667    0.019    1.581    0.101      0.237     0.00
PASS: runoff + storage = rain within 1 % and the covered barrels take no rain, with and without a rain garden
CON-04 5.2.4 base: PASS
```

6.0.0 balances, but only because its barrels are not covered (IO-24):

```
barrels only            1.667    0.000    1.657    0.010      0.000     2.00
barrels + rain garden   1.667    0.019    1.581    0.105      0.000     2.00
FAIL: the barrels declared covered (Covrd YES) collect rain
CON-04 6.0.0 base: FAIL
```

**With the fix** (5.3.0; 6.0.0 with IO-24 and this patch prints the same values):

```
S1 LIDs                 rain    init LID  runoff  final    balance   barrel
                       (ac-ft)  (ac-ft)  (ac-ft)  (ac-ft)  error (%) inflow (in)
barrels only            1.667    0.000    1.661    0.006      0.000     0.00
barrels + rain garden   1.667    0.019    1.585    0.101      0.000     0.00
PASS: runoff + storage = rain within 1 % and the covered barrels take no rain, with and without a rain garden
CON-04 5.3.0 patched: PASS
CON-04 6.0.0 patched: PASS
```

## The fix

Return the rain falling on the barrel's own footprint:

```diff
                 // Add runoff from closed rain barrel to return (cfs)
-                qReturn += subcatch->rainfall * subcatch->lidArea;
+                qReturn += subcatch->rainfall * lidArea;
```

6.0.0 makes the same change with the unit's `lid_area`; its patch carries `Requires: IO-24`, without which the branch does not run:

```diff
                 if (g.type == lid::LIDType::RAIN_BARREL && g.stor_covered[uu]) {
                     g.inflow[uu] = q_from_sc;
-                    if (usc < ctx_.subcatches.total_lid_area_ft2.size())
-                        ctx_.subcatches.lid_return_to_perv_cfs[usc] +=
-                            rain * ctx_.subcatches.total_lid_area_ft2[usc];
+                    ctx_.subcatches.lid_return_to_perv_cfs[usc] += rain * lid_area;
```

A subcatchment whose only LID is a single covered-barrel row is unchanged, since there the two areas are equal. The returned rain is still dropped when the subcatchment has no pervious area; [CON-05](../CON-05-covered-barrel-rain-lost/) fixes that on top of this patch.

**Effect on other models.** Only subcatchments with a covered rain barrel change, and no regression deck has one. `examples/Example4.inp` (a token-less, i.e. uncovered, rain barrel and five other LID types) gives byte-identical report and output files with the patched 5.3.0 and 6.0.0 command-line programs.
