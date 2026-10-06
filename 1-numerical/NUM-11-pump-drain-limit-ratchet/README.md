# NUM-11: A pump cannot draw down a junction wet well fed by a free-falling pipe

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A Type2, Type3 or Type4 pump at a junction is held to the junction's inflow whenever it would draw the level down, so the wet well never empties. In the test, J1 stays at 8.125 ft for 50 minutes with a 5 cfs pump and 1 cfs inflow, when it should empty within seconds. Pump on/off depth controls never see the level drop. Nothing warns. |
| **Reached from** | Dynamic wave routing; a Type2/3/4 pump whose inlet is a junction (not a storage unit) and whose other links add no surface area at that node, typically inflow pipes that discharge into the wet well with a free fall. |
| **5.3.0** | `getModPumpFlow()` in [`src/legacy/engine/dynwave.c:490`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L490) |
| **5.2.4** | Same, [`src/solver/dynwave.c:495`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dynwave.c#L495) |
| **6.0.0** | Reproduces, on purpose: `StructureSolver::computePumpFlowK()` keeps the unfloored area "for parity", [`src/engine/hydraulics/HydStructures.cpp:440`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/HydStructures.cpp#L440) |
| **Since** | 5.1.001 |
| **Fix** | Floor the area at MIN_SURFAREA, as the depth update does: [`NUM-11_swmm530.patch`](NUM-11_swmm530.patch), [`NUM-11_swmm600.patch`](NUM-11_swmm600.patch) |

## The problem

Junction J1 is a wet well 10 ft deep. Conduit C0 enters it 8 ft above the floor, and Type2 pump P1 lifts 5 cfs out of it at any depth. The inflow is 6 cfs for 10 minutes, more than the pump can take, so J1 fills to its rim and floods. The inflow then drops to 1 cfs. The pump should empty J1 almost at once: a junction holds only 12.566 ft² × 10 ft = 126 ft³, which a net 4 cfs removes in about 30 s. After that the pump should run at the 1 cfs inflow with the wet well near empty.

In 5.2.4, 5.3.0 and 6.0.0, J1 drops to 8.125 ft in the first steps after the inflow falls and then stays there for the rest of the hour, with the pump matching the inflow exactly:

```
  time   C0 inflow  J1 depth   P1 flow
 (min)       (cfs)      (ft)     (cfs)
    10       6.000    10.000     5.000
    12       1.020     8.125     1.020
    59       1.000     8.125     1.000
```

The level is a ratchet: it can rise, but the pump never lowers it. A wet well that starts at 5 ft under a steady 1 cfs inflow stays at 5.000 ft for the whole run in the same way.

## Why it happens

`getModPumpFlow()` keeps a pump from emptying a non-storage inlet node. It predicts the node's depth at the end of the step, and if that would be at or below zero it limits the pump to the node's inflow:

```c
// src/legacy/engine/dynwave.c, getModPumpFlow()
case TYPE2_PUMP:
case TYPE4_PUMP:
case TYPE3_PUMP:
   newNetInflow = Node[j].inflow - Node[j].outflow - q;
   netFlowVolume = 0.5 * (Node[j].oldNetInflow + newNetInflow ) * dt;
   y = Node[j].oldDepth + netFlowVolume / Xnode[j].newSurfArea;
   if ( y <= 0.0 ) return Node[j].inflow;
```

`Xnode[j].newSurfArea` is only the surface area that the node's links have added so far in this iteration. A junction adds none of its own. A conduit that discharges into the node with a free fall adds no area at that end, and a pump adds none. So at J1 the area is 0. `setNodeDepth()` would floor it at `MinSurfArea` (`surfArea = MAX(surfArea, MinSurfArea)`, `dynwave.c:666`), but this check does not. With a zero area any net withdrawal gives `y = -inf`, the pump is set to the inflow, the net flow at J1 becomes zero, and its depth never changes. The level drops to 8.125 ft only in the first steps after the inflow falls, while the trapezoidal `netFlowVolume` still includes the previous step's net inflow and is positive.

6.0.0 does this deliberately. The comment at `HydStructures.cpp:440` says it divides "by the RAW Xnode.newSurfArea — NO MinSurfArea floor" so that a Bellinge pump station (G70F11Pp1) matches legacy.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-11_junction-wet-well.inp`](NUM-11_junction-wet-well.inp) | J0 → C0 (falls 8 ft into J1) → junction wet well J1 (10 ft) → Type2 pump P1 (5 cfs) → J2 → C2 → outfall. 6 cfs for 10 min, then 1 cfs; 1 h, fixed 1 s step. |
| [`NUM-11_test.c`](NUM-11_test.c) | Legacy toolkit (5.2.4, 5.3.0). Prints C0's flow, J1's depth and P1's flow, and checks that J1 is below 0.5 ft at 0:15, 0:30 and 0:59. |
| [`NUM-11_test6.c`](NUM-11_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-11            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-11 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (the same in all three):

```
  time   C0 inflow  J1 depth   P1 flow
 (min)       (cfs)      (ft)     (cfs)
     5       6.000    10.000     5.000
    10       6.000    10.000     5.000
    12       1.020     8.125     1.020
    15       1.000     8.125     1.000
    30       1.000     8.125     1.000
    59       1.000     8.125     1.000
Routing continuity error: 1.327 %
FAIL: with 1 cfs inflow and a 5 cfs pump, J1 stays at 8.125 ft instead of being drawn down
NUM-11 5.3.0 base: FAIL
```

**With the fix**, both engines print:

```
    12       1.020     0.125     1.020
    15       1.000     0.125     1.000
    30       1.000     0.125     1.000
    59       1.000     0.125     1.000
Routing continuity error: -0.148 %
PASS: the pump draws J1 down once the inflow drops below its capacity (J1 at most 0.125 ft from 0:15 on)
NUM-11 5.3.0 patched: PASS
NUM-11 6.0.0 patched: PASS
```

## The fix

Use the same minimum area as the depth update:

```diff
-         y = Node[j].oldDepth + netFlowVolume / Xnode[j].newSurfArea;
+         y = Node[j].oldDepth + netFlowVolume /
+             MAX(Xnode[j].newSurfArea, MinSurfArea);
```

The pump is now limited to the inflow only when the depth predicted with that area would fall below zero. That is the case the check was written for: the wet well is close to empty. When the node's links already provide more than `MinSurfArea`, the result is unchanged. The 6.0.0 patch floors the area at the run's MIN_SURFAREA (the `[OPTIONS]` value, or 12.566 ft²) and replaces the parity comment.

The fix_idea's alternative, computing the pump limit after all link areas have been added, would also help where the inflow conduit is not free-falling. It would reorder `findLinkFlows()`, so it was not used.

**Effect on other models.** The four DW regression decks with Type2/3/4 pumps give byte-identical reports in both patched engines: extran7 and extran10 (Type2 and Type3), update_v52/Type5_Pump_Test, and user3 (Type4). 6.0.0's own Bellinge parity case (G70F11Pp1, named in the replaced comment) is expected to change and would need a new baseline.
