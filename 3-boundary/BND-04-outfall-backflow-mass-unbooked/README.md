# BND-04: Pollutant mass carried in by an outfall's reverse flow is not booked

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | The quality continuity error is wrong whenever an outfall feeds water into the network. The reverse flow carries the outfall's concentration (by default its last discharged value) into the network, but only the water is booked. In the test, 34 lb of pollutant enter unbooked when only 4.5 lb were ever loaded: continuity error -762 %. The report shows that error, but nothing points to its cause. |
| **Reached from** | Any FIXED, TIDAL or TIMESERIES outfall (or any outfall with backflow) in a model with pollutants |
| **5.3.0** | `removeOutflows()` in [`src/legacy/engine/routing.c:1026`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L1026) |
| **5.2.4** | Same code, [`src/solver/routing.c:911`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L911) |
| **6.0.0** | Reproduces: the system-outflow pass books only the water ([`src/engine/core/SWMMEngine.cpp:5671`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L5671)); the mass is booked only when the reverse flow enters an active LID storage node ([`src/engine/quality/QualityRouting.cpp:1014`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1014)) |
| **Since** | 5.1.013, which began booking an outfall's reverse flow as external inflow, for the water only |
| **Fix** | Book the mass with the water: [`BND-04_swmm530.patch`](BND-04_swmm530.patch), [`BND-04_swmm600.patch`](BND-04_swmm600.patch) |

## The problem

When the stage at an outfall rises above the water level in the network, water flows back from the outfall into the network. SWMM books that water as external inflow. The water also carries pollutant: the reversed link takes the outfall node's concentration, and an outfall that receives no flow keeps its last concentration. That mass is not booked anywhere, so the outfall becomes an unlimited, invisible source.

In the test deck, a 20,000 ft2 pond is loaded with 1 cfs at 10 mg/L for 2 h (4.5 lb), drains to a TIMESERIES outfall, and then the outfall stage rises to 3 ft. About 55,000 ft3 flows back into the pond at the outfall's held 10 mg/L, carrying 34.2 lb. The pond ends with 37.8 lb, but the quality mass balance shows only 4.5 lb of inflow: continuity error -761.7 %. The flow continuity error is -0.4 %.

## Why it happens

```c
// src/legacy/engine/routing.c, removeOutflows()
q = node_getSystemOutflow(i, &isFlooded);   // < 0 when an outfall feeds the network
if ( q > 0.0 )
{
    massbal_addOutflowFlow(q, isFlooded);
    for ( p = 0; p < Nobjects[POLLUT]; p++ )
    {
        w = q * Node[i].newQual[p];
        massbal_addOutflowQual(p, w, isFlooded);    // mass leaving: booked
    }
}
else massbal_addInflowFlow(EXTERNAL_INFLOW, -q);    // water entering: booked, mass: not
```

The concentration the reverse flow carries comes from `findNodeQual()`: with no inflow, a wet outfall keeps its old quality ([`qualrout.c:267`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L267)), and `findLinkQual()` takes `wIn = Node[j].newQual[p] * qIn` from the outfall end of the reversed conduit ([`qualrout.c:364`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L364)). 5.3.0 has an option, `OUTFALL_BACKFLOW_QUALITY ZERO`, that makes this concentration 0; the default keeps the held value, and either way nothing is booked. 6.0.0 mirrors the legacy booking. It books the mass only in one special case: reverse flow into an active LID storage node.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-04_pond-backflow.inp`](BND-04_pond-backflow.inp) | Pond SU1 -> conduit C1 -> TIMESERIES outfall O1. SU1 gets 1 cfs at 10 mg/L for 2 h; the outfall stage rises from 0 to 3 ft between 2:00 and 2:30. |
| [`BND-04_test.c`](BND-04_test.c) | Runs the deck through the legacy toolkit (5.2.4, 5.3.0) and checks the quality continuity error from `swmm_getMassBalErr()`, printing the report's ledger |
| [`BND-04_test6.c`](BND-04_test6.c) | The same through the 6.0.0 C API (`swmm_get_quality_continuity_error()`) |

Pollutant mass must be conserved, so the test requires a quality continuity error within 1 %.

```sh
tools/run-test.sh BND-04            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-04 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0; 6.0.0 books 4.512 lb and reports -761.948 %):

```
Quality routing continuity (P1, lb)
  External Inflow         4.513
  External Outflow        1.059
  Final Stored Mass      37.830
  Continuity Error     -761.682 %   (swmm_getMassBalErr: -761.682 %)
Flow routing continuity error -0.401 %
FAIL: pollutant mass is not conserved: 4.513 lb booked in, 1.059 lb out, 37.830 lb stored at the end (continuity error -761.682 %)
```

**With the fix** the 34.2 lb carried in by the reverse flow is booked, and the quality error equals the flow error:

```
Quality routing continuity (P1, lb)
  External Inflow        38.734
  External Outflow        1.059
  Final Stored Mass      37.830
  Continuity Error       -0.401 %   (swmm_getMassBalErr: -0.401 %)
Flow routing continuity error -0.401 %
PASS: the mass carried by the outfall's reverse flow is booked (quality continuity error -0.401 %)
BND-04 5.3.0 patched: PASS
```

6.0.0 prints 38.733 lb and -0.404 % (its flow error is -0.404 %) and also passes.

## The fix

Book the mass with the water, at the outfall's concentration, which is the one the reversed link took:

```diff
-        else massbal_addInflowFlow(EXTERNAL_INFLOW, -q);
+        else
+        {
+            massbal_addInflowFlow(EXTERNAL_INFLOW, -q);
+
+            // --- pollutant mass carried into the system by an outfall's
+            //     reverse flow
+            for ( p = 0; p < Nobjects[POLLUT]; p++ )
+                massbal_addInflowQual(EXTERNAL_INFLOW, p, -q * Node[i].newQual[p]);
+        }
```

Only the mass balance changes; no concentration or flow changes. The patch does not change which concentration the reverse flow carries: that stays the outfall's held value (or 0 with `OUTFALL_BACKFLOW_QUALITY ZERO`, or a CONCEN inflow at the outfall with [BND-03](../BND-03-outfall-backflow-concentration-ignored/)). The 6.0.0 patch makes the same change and removes the LID-only booking in `accumulateLinkLoads()`, which would now count that mass twice.

When the outfall also receives a lateral inflow while it backflows, `node_getSystemOutflow()` returns 0 and nothing is booked, for the water or the mass; that is [NUM-19](../../1-numerical/NUM-19-outfall-two-way-flow-not-booked/).

**Effect on other models.** `Example1` and `events_example`, the regression decks with pollutants, have no outfall backflow; their reports are unchanged in both engines. `_obq_default.inp` from the 6.0.0 unit-test data (a storage unit whose TIMESERIES outfall backflows; run without its 6.0.0-only `QUALITY_SOLVER` and `WATER_AGE` options) keeps every summary table, and its quality continuity error goes from -1303.99 % to 0.31 % in 5.3.0 (-1305.15 % to 0.31 % in 6.0.0), with external inflow 64.7 lb -> 910.6 lb.
