# CON-03: Run-on onto a subcatchment that is only partly LID is counted twice

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | Every LID unit in a subcatchment that receives run-on gets the run-on rate added to its inflow, although the same run-on is already spread over the subcatchment's non-LID area. The extra water, (upstream flow) x lidArea / nonLidArea, has no source in the water balance. In the test, S2's runoff rises from 3.78 in to 5.78 in, its peak from 20.00 to 30.21 cfs, and the runoff continuity error goes from 0.000 % to -49.363 %. Only the continuity error shows it. |
| **Reached from** | A subcatchment whose outlet is another subcatchment that holds LID units covering less than its full area |
| **5.3.0** | `lid_getRunoff()` in [`src/legacy/engine/lid.c:1714`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lid.c#L1714) |
| **5.2.4** | Not affected: it adds the run-on only when the LID fills the subcatchment, [`src/solver/lid.c:1671`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lid.c#L1671) |
| **6.0.0** | Reproduces with the same numbers: `SWMMEngine::stepRunoff()` in [`src/engine/core/SWMMEngine.cpp:2511`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L2511) copies the 5.3.0 test on purpose ("the pinned oracle is 5.3.0") |
| **Since** | 5.3.0, fork commit 9d3bb8d1 ("Add runoff from closed rain barrel to return flow #166", 2024-03-15), which changed `==` to `>=` |
| **Fix** | Restore the full-coverage test: [`CON-03_swmm530.patch`](CON-03_swmm530.patch), [`CON-03_swmm600.patch`](CON-03_swmm600.patch) |

## The problem

When a subcatchment drains onto another, SWMM spreads the upstream runoff over the receiving subcatchment's non-LID area. LID units in the receiving subcatchment see that water only through the share of the non-LID runoff they capture (`FromImp`, `FromPerv`). The one exception is a subcatchment that is entirely LID: it has no non-LID area, so the run-on goes onto the LID units directly.

5.3.0 applies the exception to every subcatchment with LIDs. The test deck has S1 (10 ac, impervious) draining onto S2 (10 ac, pervious, no infiltration). 2.0 in of rain falls on both. S2 holds a rain garden with no seepage, first over half of S2, then over all of it. No water leaves except as S2's runoff, so runoff plus final storage must equal the rain plus the rain garden's initial moisture:

| S2 rain garden | Rain | Runoff | Final storage | Balance error | S2 runoff | S2 peak |
|---|---|---|---|---|---|---|
| 5 ac, 5.2.4 | 3.333 ac-ft | 3.152 ac-ft | 0.223 ac-ft | 0.000 % | 3.78 in | 20.00 cfs |
| 5 ac, 5.3.0 and 6.0.0 | 3.333 ac-ft | 4.816 ac-ft | 0.225 ac-ft | -49.363 % | 5.78 in | 30.21 cfs |
| 10 ac, all engines | 3.333 ac-ft | 2.964 ac-ft | 0.453 ac-ft | -0.029 % | | |

With half of S2 in rain garden, S2 sends 4.816 ac-ft to the outfall, more than all the rain that fell on both subcatchments (3.333 ac-ft). The extra 1.66 ac-ft is S1's run-on a second time: S1 delivers 2.0 in over 10 ac, i.e. 4.0 in over S2's 5 ac of non-LID area, and the rain garden receives the same 4.0 in rate over its own 5 ac.

## Why it happens

`subcatch_addRunonFlow()` turns the upstream flow into a depth rate over the non-LID area and adds it to the inflow of all three non-LID subareas, and `subcatch_getRunoff()` books it as run-on over that area:

```c
// src/legacy/engine/subcatch.c, subcatch_addRunonFlow()
nonLidArea = Subcatch[subcatchIndex].area - Subcatch[subcatchIndex].lidArea;
if ( nonLidArea > 0.0 ) flow = flow / nonLidArea;
else                    flow = flow / Subcatch[subcatchIndex].area;
Subcatch[subcatchIndex].runon += flow;
for (i = IMPERV0; i <= PERV; i++)
    Subcatch[subcatchIndex].subArea[i].inflow += flow;

// subcatch_getRunoff()
vRunon = Subcatch[subcatchIndex].runon * tStep * nonLidArea;
// --- find LID runon only if LID occupies full subcatchment
if ( nonLidArea <= 0.0 )
    vRunon = Subcatch[subcatchIndex].runon * tStep * Subcatch[subcatchIndex].area;
```

`lid_getRunoff()` then adds the same rate to each LID unit's inflow. 5.2.4 did this only when the LID filled the subcatchment. Commit 9d3bb8d1 changed the comparison to `>=`, which is true for every validated input (`validateLidGroup()` rejects a LID area more than 0.1 % above the subcatchment area and sets one within 0.1 % of it to the area), while the comment still describes the old rule:

```c
// src/legacy/engine/lid.c, lid_getRunoff()
// ... add upstream runon only if LID occupies full subcatchment
if (subcatch->area >= subcatch->lidArea)
{
    lidInflow += subcatch->runon;
}
```

The water the LID units receive this way enters no balance term, so it shows up as surface runoff, drain flow or LID storage that was never rained or run on.

6.0.0 ports the 5.3.0 rule, with a comment that notes it is always true and that 5.2.4 tested `==`:

```cpp
// src/engine/core/SWMMEngine.cpp, SWMMEngine::stepRunoff()
g.inflow[uu] += ctx_.subcatches.runon_rate[usc];
```

## How to reproduce

| File | What it is |
|---|---|
| [`CON-03_partial-lid.inp`](CON-03_partial-lid.inp) | S1 (10 ac impervious) drains onto S2 (10 ac pervious, no infiltration) with a 5 ac rain garden; 1 in/hr for 2 h |
| [`CON-03_full-lid.inp`](CON-03_full-lid.inp) | The same with a 10 ac rain garden that fills S2 (the case the run-on rule is for) |
| [`CON-03_test.c`](CON-03_test.c) | Runs both decks through the legacy toolkit (5.2.4 and 5.3.0), reads the Runoff Quantity Continuity table and checks runoff + storage = rain + initial storage within 1 % |
| [`CON-03_test6.c`](CON-03_test6.c) | The same with the 6.0.0 engine API |

```sh
tools/run-test.sh CON-03            # 5.2.4: PASS; 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-03 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 6.0.0 prints the same numbers):

```
S2 LID              rain    init LID  runoff  final    balance
                   (ac-ft)  (ac-ft)  (ac-ft)  (ac-ft)  error (%)
5 ac rain garden    3.333    0.042    4.816    0.225    -49.363
10 ac rain garden   3.333    0.083    2.964    0.453     -0.029
FAIL: S2's runoff and storage do not balance the rain for 1 of 2 decks (more than 1 % off); run-on onto a partial LID is counted twice
CON-03 5.3.0 base: FAIL
CON-03 6.0.0 base: FAIL
```

5.2.4 balances both decks:

```
5 ac rain garden    3.333    0.042    3.152    0.223      0.000
10 ac rain garden   3.333    0.083    2.964    0.453     -0.029
PASS: runoff + storage = rain within 1 % with a partial and a full LID
CON-03 5.2.4 base: PASS
```

**With the fix** (5.3.0 and 6.0.0 print the same values as 5.2.4):

```
S2 LID              rain    init LID  runoff  final    balance
                   (ac-ft)  (ac-ft)  (ac-ft)  (ac-ft)  error (%)
5 ac rain garden    3.333    0.042    3.152    0.223      0.000
10 ac rain garden   3.333    0.083    2.964    0.453     -0.029
PASS: runoff + storage = rain within 1 % with a partial and a full LID
CON-03 5.3.0 patched: PASS
CON-03 6.0.0 patched: PASS
```

## The fix

5.3.0: restore the 5.2.4 comparison. `validateLidGroup()` sets `lidArea` to exactly `area` when the LIDs cover more than 99.9 % of it, so the equality is the same test as `nonLidArea <= 0` in `subcatch_addRunonFlow()` and `subcatch_getRunoff()`:

```diff
             // ... add upstream runon only if LID occupies full subcatchment
-            if (subcatch->area >= subcatch->lidArea)
+            if (subcatch->area == subcatch->lidArea)
```

6.0.0: add the run-on rate only when the subcatchment has no non-LID area (`rsoa.area`, the area `add_runon()` divides by), and use the same rate for the unit's water-age and temperature mixing, which took the run-on share from the same expression:

```diff
-                g.inflow[uu] += ctx_.subcatches.runon_rate[usc];
+                const double runon_lid = (rsoa.area[usc] > 0.0)
+                    ? 0.0 : ctx_.subcatches.runon_rate[usc];
+                g.inflow[uu] += runon_lid;
 ...
-                const double q_runon_unit =
-                    ctx_.subcatches.runon_rate[usc] * lid_area;
+                const double q_runon_unit = runon_lid * lid_area;
```

Subcatchments entirely covered by LIDs, and subcatchments without run-on, are unchanged.

**Effect on other models.** No regression deck routes run-on onto a partly LID-covered subcatchment. `examples/Example4.inp` (run-on onto three subcatchments that are 100 % swale) and `update_v5111/rain_garden.inp` (run-on onto a 100 % bio-retention cell) give byte-identical reports and output files with the patched 5.3.0 and 6.0.0 command-line programs.
