# NUM-45: A dry node with no inflow reports its pollutant load rate as its concentration

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A junction or outfall that holds no water and receives none reports a non-zero concentration whenever it has a MASS-type inflow (or, in 5.3.0, an API mass flux): the load in mass/s divided by 28.317 L/ft³, printed as mg/L. A 10 mg/s load gives 0.353 mg/L, 100 mg/s gives 3.53 mg/L, in the binary output and through the API getters. Nothing warns. |
| **Reached from** | Any `[INFLOWS]` entry of type `MASS` (or a positive node API flux) at a non-storage node while it has no flow inflow and is dry; also the tiny `q × c` loads of links carrying less than 1e-10 cfs |
| **5.3.0** | `findNodeQual()` in [`src/legacy/engine/qualrout.c:266`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L266) |
| **5.2.4** | Not affected: the no-inflow branch sets a dry node to 0 ([`src/solver/qualrout.c:242`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/qualrout.c#L242)) |
| **6.0.0** | Reproduces, on purpose: the non-storage branch of `QualitySolver::mixAtNodes()` publishes the dry node's load rate as its concentration ([`src/engine/quality/QualityRouting.cpp:1114`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1114)) |
| **Since** | 5.3.0, fork commit 5b87a2b5 (10 December 2024, "WIP API bindings for pollutants #204, #162, #163, #184, #180") |
| **Fix** | Restore the zero for a dry node: [`NUM-45_swmm530.patch`](NUM-45_swmm530.patch), [`NUM-45_swmm600.patch`](NUM-45_swmm600.patch) |

## The problem

Two junctions, J2 and J3, receive MASS-type loads of 10 and 100 mg/s of a pollutant but no water. They stay at depth 0 with zero inflow for the whole two-hour run. 5.2.4 reports their concentration as 0. 5.3.0 and 6.0.0 report 0.353145 and 3.531448 mg/L at every reporting period:

| Node | MASS load (mg/s) | Depth, inflow | P1, 5.2.4 | P1, 5.3.0 and 6.0.0 | load / 28.317 L/ft³ |
|---|---|---|---|---|---|
| J2 | 10 | 0, 0 | 0 | 0.353145 mg/L | 0.353145 |
| J3 | 100 | 0, 0 | 0 | 3.531448 mg/L | 3.531448 |

The number is the node's mass inflow rate in SWMM's internal units (mass/s per cfs, i.e. mg/s ÷ 28.317), not a concentration of anything: there is no water at the node. It scales with the load, so a dry manhole with a dosing pump or an industrial MASS load, or a node fed by the 5.3.0 API flux, shows whatever the load happens to be. The value goes to the `.out` file, the time series plots, and `swmm_getValue`/`swmm_node_get_quality()`.

The load itself is lost from the mass balance (100 % quality continuity error); that is a separate defect, [CON-08](../../2-conceptual/CON-08-mass-load-at-dry-node-discarded/). This issue is about the concentration reported for the node.

## Why it happens

During a routing step `Node[j].newQual[p]` is first used as an accumulator: `findLinkMassFlow()` adds `qLink × c` for each inflowing link and `addExternalInflows()` adds external loads. A CONCEN inflow is multiplied by its flow, a MASS inflow is added as it is:

```c
// src/legacy/engine/routing.c, addExternalInflows()
                w = inflow_getExtInflow(inflow, currentDate);
                if ( inflow->type == CONCEN_INFLOW ) w *= q;
                Node[j].newQual[p] += w;
```

`findNodeQual()` then turns the accumulator into a concentration by dividing by the node's inflow. If the inflow is at or below `ZERO` (1e-10 cfs), there is nothing to divide by. 5.2.4 keeps the old concentration for a wet node and sets 0 for a dry one:

```c
// src/solver/qualrout.c (5.2.4), findNodeQual()
    else for (p = 0; p < Nobjects[POLLUT]; p++)
    {
        if (Node[j].newDepth > ZeroDepth)
            Node[j].newQual[p] = Node[j].oldQual[p];
        else
            Node[j].newQual[p] = 0.0;
    }
```

Commit 5b87a2b5, which added the API pollutant fluxes, rewrote this loop with braces and dropped the `else`. A later commit added an arm for outfalls, but the dry case still has no assignment:

```c
// src/legacy/engine/qualrout.c (5.3.0), findNodeQual()
    else
    {
        for (p = 0; p < Nobjects[POLLUT]; p++)
        {
            if ( ZeroOutfallBackflowQual && Node[j].type == OUTFALL )
                Node[j].newQual[p] = 0.0;
            else if (Node[j].newDepth > ZeroDepth)
                Node[j].newQual[p] = Node[j].oldQual[p];
                                        // dry node: newQual keeps the mass rate
        }
    }
```

6.0.0 reproduces the 5.3.0 result deliberately, with a comment that names it:

```cpp
// src/engine/quality/QualityRouting.cpp, QualitySolver::mixAtNodes()
                } else {
                    // A DRY one leaves Node.newQual as the accumulator
                    // (qualrout.c findNodeQual): the summed q*c load rate,
                    // published as its concentration. ...
                    nodes.conc[idx] = (idx < nodes.qual_mass_in.size())
                                    ? nodes.qual_mass_in[idx] : 0.0;
                }
```

Without a MASS or API load the accumulator of a dry node holds only `q × c` for link flows below 1e-10 cfs, so the reported value is tiny (the 6.0.0 comment cites 1e-10 to 1e-13 mg/L on one deck). In 5.3.0 it can also hold the NaN that [NUM-01](../NUM-01-dry-conduit-quality-nan/) writes into dry conduits, which then appears at the dry junctions below them.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-45_dry-junction-mass-load.inp`](NUM-45_dry-junction-mass-load.inp) | Junctions J2 and J3, each draining through a conduit to its own free outfall; MASS loads of 10 and 100 mg/s of P1 and no flow. DYNWAVE, 10 s step, 2 h |
| [`NUM-45_test.c`](NUM-45_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0) and reads depth, inflow and P1 of J2 and J3 for every reporting period of the `.out` file |
| [`NUM-45_test6.c`](NUM-45_test6.c) | The same through the 6.0.0 C API, reading `swmm_node_get_depth/inflow/quality()` after every routing step |

A node that holds no water and receives none has no pollutant in it, and SWMM reports such a node as 0. The test requires P1 = 0 (within 1e-6 mg/L) wherever depth and inflow are both 0; the bug gives 0.353 and 3.53 mg/L.

```sh
tools/run-test.sh NUM-45            # 5.2.4: PASS, 5.3.0: FAIL, 6.0.0: FAIL
tools/run-test.sh NUM-45 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.2.4 reports 0:

```
Node  MASS load   max depth  max inflow  max P1 reported  load / 28.317 L/ft3
      (mg/s)      (ft)       (cfs)       (mg/L)           (mg/s per cfs)
J2        10.0      0.0000      0.0000         0.000000         0.353145
J3       100.0      0.0000      0.0000         0.000000         3.531448
(8 reporting periods)
PASS: nodes with no water and no inflow report a zero concentration
NUM-45 5.2.4 base: PASS
```

5.3.0 and 6.0.0 report the load rate:

```
J2        10.0      0.0000      0.0000         0.353145         0.353145  <-- wrong
J3       100.0      0.0000      0.0000         3.531448         3.531448  <-- wrong
(8 reporting periods)
FAIL: 2 of 2 dry nodes with no inflow report a non-zero concentration (J2 0.353145, J3 3.531448 mg/L), equal to their mass load rate
NUM-45 5.3.0 base: FAIL
...
J2        10.0      0.0000      0.0000         0.353145         0.353145  <-- wrong
J3       100.0      0.0000      0.0000         3.531448         3.531448  <-- wrong
(721 routing steps)
FAIL: 2 of 2 dry nodes with no inflow report a non-zero concentration (J2 0.353145, J3 3.531448 mg/L), equal to their mass load rate
NUM-45 6.0.0 base: FAIL
```

**With the fix** both engines report 0:

```
J2        10.0      0.0000      0.0000         0.000000         0.353145
J3       100.0      0.0000      0.0000         0.000000         3.531448
(8 reporting periods)
PASS: nodes with no water and no inflow report a zero concentration
NUM-45 5.3.0 patched: PASS
...
(721 routing steps)
PASS: nodes with no water and no inflow report a zero concentration
NUM-45 6.0.0 patched: PASS
```

## The fix

5.3.0: put back the `else` that 5.2.4 has.

```diff
             else if (Node[j].newDepth > ZeroDepth)
                 Node[j].newQual[p] = Node[j].oldQual[p];
+            else
+                Node[j].newQual[p] = 0.0;
```

6.0.0: the same in the dry arm of the non-storage branch.

```diff
                 } else {
-                    ...
-                    nodes.conc[idx] = (idx < nodes.qual_mass_in.size())
-                                    ? nodes.qual_mass_in[idx] : 0.0;
+                    // A DRY one holds no water, so no concentration
+                    // (5.2.4 findNodeQual). Its summed load rate
+                    // (qual_mass_in) is not a concentration.
+                    nodes.conc[idx] = 0.0;
                 }
```

Only nodes that are dry and have no inflow change; the mass balance does not use their concentration (their volume is 0), so the continuity tables are unchanged. Effect on other models, with private patched builds of both engines: `events_example.inp` (regression suite), the parity deck `steady_quality.inp` and the 6.0.0 `site_drainage_example.inp` give byte-identical `.out` files in both engines. EPA's Example1 is byte-identical in 6.0.0; in 5.3.0, 8 junctions that were NaN at the first reporting period (TSS and Lead, the NaN that [NUM-01](../NUM-01-dry-conduit-quality-nan/) writes into the dry conduits above them) now read 0, and nothing else changes.

6.0.0's unit test `QualityRoutingTest.ResidualInflowBelowLegacyZeroIsNotDividedBy` expected the 5.3.0 residue at its dry node (the load rate 1e-14 x 1e5, "legacy leaves a dry node's accumulator in place"); the 6.0.0 patch changes the expected value to 0. The test's point, that an inflow below legacy's ZERO is not divided by, still holds.
