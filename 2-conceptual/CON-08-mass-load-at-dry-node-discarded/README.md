# CON-08: A MASS-type pollutant load at a node with no flow is booked as inflow and then thrown away

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | A MASS-type `[INFLOWS]` load (or, in 5.3.0, a positive node API mass flux) at a node whose flow inflow is zero never reaches the network. Dosing 10 mg/s into a pond holding 2,000 ft³ of still water for 2 h should raise it to 1.2713 mg/L; it stays at 0. The 0.159 lb is still booked as External Inflow, so the quality continuity error is 100 %. A dry junction with the same load gives the same 100 %. |
| **Reached from** | `[INFLOWS]` entries of type `MASS` whose time series is non-zero while the node receives no flow (dry weather, a pond between events, a dry manhole); 5.3.0's node API flux under the same conditions |
| **5.3.0** | `getMixedQual()` returns the old concentration when the inflow is at or below `ZERO` ([`src/legacy/engine/qualrout.c:165`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L165)), called from `findStorageQual()` ([`qualrout.c:548`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L548)); `findNodeQual()` uses the load only if the inflow exceeds `ZERO` ([`qualrout.c:239`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L239)); the load is booked in `addExternalInflows()` ([`routing.c:590`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L590)) |
| **5.2.4** | Same code: [`src/solver/qualrout.c:160`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/qualrout.c#L160), [`qualrout.c:233`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/qualrout.c#L233), [`qualrout.c:461`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/qualrout.c#L461), [`routing.c:487`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L487) |
| **6.0.0** | Reproduces: `QualitySolver::mixAtNodes()` skips `qual_mass_in` in the same two places ([`src/engine/quality/QualityRouting.cpp:1092`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1092), [`:1211`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1211)) |
| **Since** | 5.0 (both no-flow tests are in the repository's first commit, 2014) |
| **Fix** | Mix a no-flow load into the stored water, or keep it as residue at a node with none: [`CON-08_swmm530.patch`](CON-08_swmm530.patch) (requires NUM-45, NUM-46), [`CON-08_swmm600.patch`](CON-08_swmm600.patch) (requires NUM-45) |

## The problem

A MASS-type external inflow specifies a load in mass per second, independent of flow. That is how a chemical dosing pump, an industrial discharge given as a load, or an atmospheric load on a pond is entered. SWMM books it as External Inflow at every step, whether or not the node receives any water. If it does not, the load is dropped.

A storage unit SU1 holds 2,000 ft³ of still water (1,000 ft², 2 ft deep, outlet 6 ft up). It receives 10 mg/s of P1 for 2 hours and no flow. 72,000 mg in 2,000 ft³ (56,634 L) is 1.2713 mg/L. All three engines report:

```
  External Inflow ..........         0.159
  Final Stored Mass ........         0.000
  Continuity Error (%) .....       100.000
```

and SU1 at 0.000 mg/L for the whole run. The same happens at a junction that has no water at all: 0.159 lb in, nothing anywhere, 100 %. Any MASS time series that stays on while the receiving node is dry, between storms for example, loses its load the same way, and in a longer run the loss is mixed with everything else in a continuity error that is merely "too high".

## Why it happens

`addExternalInflows()` scales a CONCEN-type load by the node's external flow but adds a MASS-type load as it is, to both the node's mass accumulator and the ledger:

```c
// src/legacy/engine/routing.c, addExternalInflows()
                w = inflow_getExtInflow(inflow, currentDate);
                if ( inflow->type == CONCEN_INFLOW ) w *= q;
                Node[j].newQual[p] += w;
                massbal_addInflowQual(EXTERNAL_INFLOW, p, w);
```

5.3.0 does the same with a positive node API flux a few lines further down. The mixing step then looks at the flow, not at the load. For a storage unit (or any node holding volume), `findStorageQual()` calls `getMixedQual()`, which returns before it looks at the mass inflow:

```c
// src/legacy/engine/qualrout.c, getMixedQual()
    // --- if no inflow then reactor concentration is unchanged
    if (qIn <= ZERO)
        return c;
```

For a node without storage, `findNodeQual()` divides the accumulator by the inflow only `if (qNode > ZERO)`; otherwise the node keeps its old concentration (5.2.4 sets 0 if it is dry) and the accumulator is overwritten. In both cases the mass was booked as inflow and is never added to a node, a link, or a loss term. The 1e-10 cfs threshold is the right guard against dividing by zero, but a MASS load does not need a flow to be mixed into water that is already there.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-08_pond-mass-load.inp`](CON-08_pond-mass-load.inp) | Storage SU1 with 2,000 ft³ of still water, MASS load 10 mg/s of P1, no flow; DYNWAVE, 2 h |
| [`CON-08_dry-junction-mass-load.inp`](CON-08_dry-junction-mass-load.inp) | Junction J2 with no water, the same MASS load |
| [`CON-08_test.c`](CON-08_test.c) | Runs both decks through the legacy toolkit (5.2.4 and 5.3.0), reads the Quality Routing Continuity table and SU1's concentration from the `.out` file |
| [`CON-08_test6.c`](CON-08_test6.c) | The same through the 6.0.0 C API |

The test applies conservation of mass: the load must end up somewhere the mass balance can see. It requires the quality continuity error, both reported and recomputed from the table's own lines, within 1 %, and SU1 within 1 % of 1.2713 mg/L.

```sh
tools/run-test.sh CON-08            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-08 --patched  # 5.3.0 and 6.0.0 (with NUM-45, NUM-46 applied first): PASS
```

**Without the fix** all three engines print:

```
Deck                                 External  Final    Error from  Reported   SU1 P1 at end
                                     inflow    stored   the table   error      (mg/L, expected
                                     (lb)      (lb)     (%)         (%)        1.2713)
CON-08_pond-mass-load.inp               0.159    0.000     100.000   100.000      0.0000  <-- wrong
CON-08_dry-junction-mass-load.inp       0.159    0.000     100.000   100.000         n/a  <-- wrong
FAIL: in 2 of 2 decks the MASS load is booked as inflow but goes nowhere (continuity error 100 %, pond concentration not raised)
CON-08 5.2.4 base: FAIL
CON-08 5.3.0 base: FAIL
CON-08 6.0.0 base: FAIL
```

**With the fix:**

```
CON-08_pond-mass-load.inp               0.159    0.159       0.000    -0.003      1.2713
CON-08_dry-junction-mass-load.inp       0.159    0.159       0.000    -0.003         n/a
PASS: a MASS load at a node with no flow is kept and the quality mass balance closes
CON-08 5.3.0 patched: PASS
...
CON-08_pond-mass-load.inp               0.159    0.159       0.000     0.000      1.2713
CON-08_dry-junction-mass-load.inp       0.159    0.159       0.000     0.000         n/a
PASS: a MASS load at a node with no flow is kept and the quality mass balance closes
CON-08 6.0.0 patched: PASS
```

The −0.003 % left in 5.3.0 is its half-step integration of the inflow rate at the first and last steps (the first DYNWAVE step is 0.5 s), not part of this issue.

## The fix

Where there is water, mix the load into it; where there is none, keep it at the node as residue, the way `qualrout.c` already books the contents of a node or link that dries out (`massbal_addToFinalStorage()`):

```diff
 // findStorageQual()
         wIn = Node[j].newQual[p];
         c2 = getMixedQual(c1, v1, wIn, qIn, tStep);
 
+        // --- a mass load that arrives with no flow (MASS-type or API
+        //     inflow) mixes into the stored water, or stays at the node
+        //     as residue if it holds none
+        if (qIn <= ZERO && wIn != 0.0)
+        {
+            if (v1 > ZeroVolume) c2 = MAX(0.0, c2 + wIn * tStep / v1);
+            else massbal_addToFinalStorage(p, wIn * tStep);
+        }

 // findNodeQual(j, tStep), no-inflow branch
         for (p = 0; p < Nobjects[POLLUT]; p++)
         {
+            // --- a mass load with no flow to carry it (MASS-type or API
+            //     inflow) stays at the node as residue, like the contents
+            //     of a node that dries out
+            massbal_addToFinalStorage(p, Node[j].newQual[p] * tStep);
+
```

`findNodeQual()` gains a `tStep` argument for this. A residue is not put back into the water when flow returns, the same as for a dried-out node; a modeller who wants the load delivered with the next flow should use a CONCEN-type inflow or give the node storage. Rejecting MASS loads at nodes that can run dry, or warning about them, would be the alternative.

The 5.3.0 patch is written on top of [NUM-45](../../1-numerical/NUM-45-dry-node-reports-mass-rate-as-concentration/) (same lines of `findNodeQual()`) and needs [NUM-46](../../1-numerical/NUM-46-quality-final-storage-double-counted/), without which every residue booked through `massbal_addToFinalStorage()` is counted twice. The 6.0.0 patch makes the same two changes in `QualitySolver::mixAtNodes()`, booking residue in `qual_routing_final_dry` (which 6.0.0 already counts once), on top of NUM-45.

Effect on other models: with private patched builds of both engines (CON-08 with its prerequisites), EPA's Example1, `events_example.inp`, the parity deck `steady_quality.inp` and the 6.0.0 `site_drainage_example.inp` show only the changes of NUM-45 and NUM-46; CON-08 adds no difference to their `.out` files or reports (they have no MASS-type inflows).
