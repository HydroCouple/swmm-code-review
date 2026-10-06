# NUM-10: Ideal pumps and DUMMY conduits miss the inflow from links listed after them

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A node drained by an ideal pump or a DUMMY conduit floods when a weir, orifice, outlet or pump that feeds it has a higher link index. In the test, 50-60% of a steady 5 cfs floods at a node that should pass all of it. Reordering sections in the input file changes the result. Nothing warns. |
| **Reached from** | Dynamic wave routing; an ideal pump (no curve, `*`) or a DUMMY conduit whose inlet node receives flow from a non-conduit link with a higher index. The SWMM GUI writes `[CONDUITS]` before `[PUMPS]`, `[ORIFICES]`, `[WEIRS]` and `[OUTLETS]`, so every DUMMY conduit fed by a regulator is affected in GUI-written files. |
| **5.3.0** | `findLinkFlows()` in [`src/legacy/engine/dynwave.c:401`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L401); ideal pump flow in [`link.c:1576`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1576); DUMMY conduit flow through `node_getOutflow()`, [`node.c:394`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L394) |
| **5.2.4** | Same, [`src/solver/dynwave.c:406`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dynwave.c#L406), [`link.c:1573`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L1573) |
| **6.0.0** | Reproduces: the DW non-conduit callback walks `StructureSolver::nc_indices_` in link-index order, [`src/engine/hydraulics/HydStructures.cpp:237`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/HydStructures.cpp#L237) |
| **Since** | 5.1.001, where `findLinkFlows()` first visits non-conduit links in index order |
| **Fix** | Compute DUMMY conduits and ideal pumps after all other links: [`NUM-10_swmm530.patch`](NUM-10_swmm530.patch), [`NUM-10_swmm600.patch`](NUM-10_swmm600.patch) |

## The problem

A steady 5 cfs enters J0, flows over transverse weir W1 into junction J1, and leaves J1 through a pass-through link: ideal pump P1 in one deck, DUMMY conduit D1 in the other. Both should carry all 5 cfs, and J1 should never flood.

When the pass-through link has a lower index than W1 (`[PUMPS]` before `[WEIRS]`, or `[CONDUITS]` before `[WEIRS]` as the GUI writes it), it carries 2.0-2.5 cfs. J1 fills to its 5 ft rim and floods the rest:

| Deck | W1 (cfs) | P1/D1 (cfs) | J1 flooding | Continuity error |
|---|---|---|---|---|
| ideal pump, `[PUMPS]` first | 5.007 | 2.032 | 21,413 ft³ = 59.5% of inflow | -0.105% |
| DUMMY conduit, GUI order | 4.984 | 2.484 | 18,002 ft³ = 50.0% of inflow | 1.294% |

With `[WEIRS]` first, the same networks pass the whole flow and nothing floods. The continuity error does not show the problem in the ideal-pump case, because the lost water is booked as flooding.

## Why it happens

Each Picard iteration, `initNodeStates()` resets every node's `inflow` to its lateral inflow. `findLinkFlows()` then adds the flows of all true conduits and visits the remaining links in link-index order, computing each link's flow and adding it to its end nodes at once:

```c
// src/legacy/engine/dynwave.c, findLinkFlows()
// --- find new flows for all dummy conduits, pumps & regulators
for ( i = 0; i < Nobjects[LINK]; i++)
{
    if ( !isTrueConduit(i) )
    {
        if ( !Link[i].bypassed ) findNonConduitFlow(i, dt);
        updateNodeFlows(i);
    }
}
```

Most of these links compute their flow from node heads, so the order does not matter. Two kinds pass on their inlet node's inflow as it stands when they are visited:

```c
// src/legacy/engine/link.c, pump_getInflow()
if ( Pump[k].type == IDEAL_PUMP )
    qIn = Node[n1].inflow + Node[n1].overflow;

// src/legacy/engine/node.c, node_getOutflow()  (used by a DUMMY conduit)
default:      return Node[nodeIndex].inflow + Node[nodeIndex].overflow;
```

When P1 or D1 is visited before W1, J1's inflow does not yet include the weir, and the next iteration resets it again. The weir's flow therefore never reaches the pass-through link, except as the previous step's overflow, which is why about half of it gets through. `checkDummyLinks()` (`toposort.c:480`) only rejects nodes whose inflow links are all DUMMY or ideal, so this layout is accepted.

6.0.0 does the same. `HydStructures.cpp` lists all non-conduit links in index order in `nc_indices_`, and the DW callback in `SWMMEngine.cpp` computes and scatters them one at a time in that order, for parity with legacy.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-10_ideal-pump.inp`](NUM-10_ideal-pump.inp) | 5 cfs into J0 → transverse weir W1 → J1 (5 ft deep) → ideal pump P1 → J2 → C1 → outfall. `[PUMPS]` before `[WEIRS]`. 2 h, fixed 5 s step. |
| [`NUM-10_dummy-conduit.inp`](NUM-10_dummy-conduit.inp) | The same with DUMMY conduit D1 instead of P1, in GUI section order |
| [`NUM-10_test.c`](NUM-10_test.c) | Legacy toolkit (5.2.4, 5.3.0). Runs both decks, integrates J0's inflow and J1's overflow, and checks that J1 does not flood and that P1/D1 carry W1's flow. |
| [`NUM-10_test6.c`](NUM-10_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-10            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-10 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0 identical; 6.0.0 the same apart from counting the first step's inflow):

```
At 1:00 h: W1 flow into J1, pass-through link (P1 or D1) flow out of J1;
over the run: inflow volume, J1 flooding volume, routing continuity error
deck                            W1    P1/D1 J1 depth     inflow   J1 flood   flood cont.err
                             (cfs)    (cfs)     (ft)      (ft3)      (ft3)     (%)      (%)
NUM-10_ideal-pump.inp        5.007    2.032     5.00      35975      21413    59.5   -0.105
NUM-10_dummy-conduit.inp     4.984    2.484     5.00      35975      18002    50.0    1.294
FAIL: J1 floods; the weir's flow is not passed on by ideal pump P1 or by DUMMY conduit D1
NUM-10 5.3.0 base: FAIL
```

**With the fix**, 5.3.0 and 6.0.0 print the same flows:

```
NUM-10_ideal-pump.inp        4.949    4.930     0.34      35975          0     0.0   -0.080
NUM-10_dummy-conduit.inp     4.963    4.967     0.61      35975          0     0.0   -0.024
PASS: the ideal pump and the DUMMY conduit pass on all of the weir's flow; J1 does not flood
NUM-10 5.3.0 patched: PASS
NUM-10 6.0.0 patched: PASS
```

## The fix

Leave DUMMY conduits and ideal pumps out of the existing loop, and visit them in a second loop once every other link has added its flow:

```diff
     for ( i = 0; i < Nobjects[LINK]; i++)
     {
-        if ( !isTrueConduit(i) )
+        if ( !isTrueConduit(i) && !isPassThruLink(i) )
         {
             if ( !Link[i].bypassed ) findNonConduitFlow(i, dt);
             updateNodeFlows(i);
         }
     }
+
+    // --- dummy conduits & ideal pumps pass on their inlet node's inflow,
+    //     so find their flows after all other links have added to it
+    for ( i = 0; i < Nobjects[LINK]; i++)
+    {
+        if ( isPassThruLink(i) )
+        {
+            if ( !Link[i].bypassed ) findNonConduitFlow(i, dt);
+            updateNodeFlows(i);
+        }
+    }
```

`isPassThruLink()` is a new three-line helper: a DUMMY conduit or an ideal pump. In 6.0.0 the fix builds `nc_indices_` in two passes with the same split, so the callback visits the links in the same order as patched 5.3.0.

The ordering among the pass-through links themselves is still link-index order. A chain such as an ideal pump that feeds a node drained by a DUMMY conduit can still depend on index order when that node also has a conduit inflow (`checkDummyLinks()` rejects the chain only when all of the node's inflow links are pass-through). Fully ordering such chains would need a topological sort, which `toposort_sortLinks()` skips under dynamic wave.

**Effect on other models.** None of the DW regression decks has a DUMMY conduit or an ideal pump, so the patch changes none of them. That was confirmed on extran6 and user3 (with pumps, weirs and orifices): both reports are byte-identical in both patched engines. For a model without these links the order of every other link is unchanged.
