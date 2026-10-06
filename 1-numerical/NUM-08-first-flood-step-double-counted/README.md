# NUM-08: The step on which a storage unit starts to flood counts its fill volume twice

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Each time a non-ponding storage unit (or a Type-1 pump wet well) starts to flood, the flooding total gains the volume that filled the node on that step, up to one step of inflow. Flooding is over-reported and the routing continuity error goes negative (-7.58% in the test). Nothing warns. |
| **Reached from** | Dynamic wave routing; any node with a full volume above zero that floods without ponding: storage units with no ponded area or ALLOW_PONDING NO, and Type-1 pump inlet junctions. Plain junctions (full volume 0) are not affected. |
| **5.3.0** | `getFloodedDepth()` in [`src/legacy/engine/dynwave.c:817`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L817) |
| **5.2.4** | Same code, [`src/solver/dynwave.c:782`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dynwave.c#L782) |
| **6.0.0** | Reproduces: `DWSolver::commitNodeDepthState()` keeps the legacy rule, [`src/engine/hydraulics/DynamicWave.cpp:4227`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/DynamicWave.cpp#L4227) |
| **Since** | 5.0.014, which introduced `getFloodedDepth()`; every 5.1 and 5.2 release |
| **Fix** | Book as overflow only what is left after the node fills: [`NUM-08_swmm530.patch`](NUM-08_swmm530.patch), [`NUM-08_swmm600.patch`](NUM-08_swmm600.patch) |

## The problem

An open storage unit of 1,000 ft² and 10 ft depth (10,000 ft³) with no outlet receives 100 cfs for 5 minutes, routed with a fixed 30 s step. It holds 7,500 ft³ after three steps. On the fourth step another 3,000 ft³ arrives: 2,500 ft³ fill the tank and 500 ft³ should overflow. SWMM sets the volume to 10,000 ft³ and also reports 100 cfs × 30 s = 3,000 ft³ of overflow, so the step books 5,500 ft³ for 3,000 ft³ of inflow.

All later steps start full and balance. The run reports 0.585 ac-ft (25,480 ft³) of flooding instead of 0.528 ac-ft (23,000 ft³), and a routing continuity error of -7.576%, in 5.2.4, 5.3.0 and 6.0.0.

The extra volume is the part of the onset step's inflow that went into storage, so it is at most one step of inflow per flooding event. A fixed long step makes it large. With variable steps it is smaller, but it recurs every time a node goes from below full to flooding.

## Why it happens

`setNodeDepth()` computes the net inflow volume of the step, `dV = 0.5*(oldNetInflow + dQ)*dt`, and calls `getFloodedDepth()` when the new depth exceeds the node's maximum. For a node that cannot pond, the whole `dV` becomes overflow and the volume is capped at full:

```c
// src/legacy/engine/dynwave.c, getFloodedDepth()
if ( canPond == FALSE )
{
    Node[i].overflow = dV / dt;                  // all of the step's net inflow
    Node[i].newVolume = Node[i].fullVolume;      // ... and the node also fills
    yNew = yMax;
}
else
{
    Node[i].newVolume = MAX((Node[i].oldVolume+dV), Node[i].fullVolume);
    Node[i].overflow = (Node[i].newVolume -
        MAX(Node[i].oldVolume, Node[i].fullVolume)) / dt;
}
```

`dV / dt` is right only if the node was already full at the start of the step. If `oldVolume < fullVolume`, the `fullVolume - oldVolume` that filled the node is booked as a gain in storage and again as overflow. The ponded branch just below already uses the volume difference. Junctions have `fullVolume = 0` and `oldVolume = 0`, so for them `dV / dt` is the correct difference. Storage units and Type-1 pump wet wells (whose `fullVolume` is set from the pump curve, `link.c:1529`) have a real full volume and hit the defect.

6.0.0 has the same two lines in `commitNodeDepthState()`: `nodes.overflow[ui] = dV / dt; nodes.volume[ui] = t.rpt_full_volume;`.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-08_open-tank-floods.inp`](NUM-08_open-tank-floods.inp) | Storage S1 (1,000 ft², 10 ft, no surcharge depth, no ponding, no links), 100 cfs for 5 min, fixed 30 s step, 20 min run. J1-C1-O1 is there only because SWMM needs an outfall. |
| [`NUM-08_test.c`](NUM-08_test.c) | Legacy toolkit (5.2.4, 5.3.0). Records S1's inflow, volume and overflow every step and checks that the change in volume plus the overflow volume equals the step's inflow volume, `0.5*(q_old + q_new)*dt`. Also checks the routing continuity error. |
| [`NUM-08_test6.c`](NUM-08_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-08            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-08 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (identical in 5.2.4, 5.3.0 and 6.0.0; 5.3.0 shown):

```
  time   inflow    volume  overflow   dV+ovf*dt   inflow vol   imbalance
   (s)    (cfs)     (ft3)     (cfs)       (ft3)        (ft3)       (ft3)
    30   100.00    1500.0      0.00      1500.0       1500.0         0.0
    60   100.00    4500.0      0.00      3000.0       3000.0         0.0
    90   100.00    7500.0      0.00      3000.0       3000.0         0.0
   120   100.00   10000.0    100.00      5500.0       3000.0      2500.0   <-- first flooding step
   150   100.00   10000.0    100.00      3000.0       3000.0         0.0
   ...
Routing continuity error: -7.576 %
FAIL: a step books 2500 ft3 more storage+overflow than its 3000 ft3 inflow; routing continuity error -7.576 %
NUM-08 5.3.0 base: FAIL
```

The report agrees: External Inflow 0.757 ac-ft, Flooding Loss 0.585, Final Stored Volume 0.230, Continuity Error -7.576%.

**With the fix**, the onset step overflows 16.67 cfs (500 ft³) and the other steps are unchanged. Both engines print the same table; 6.0.0 shows the continuity error as -0.000 %:

```
   120   100.00   10000.0     16.67      3000.0       3000.0         0.0   <-- first flooding step
   150   100.00   10000.0    100.00      3000.0       3000.0         0.0
   ...
Routing continuity error: 0.000 %
PASS: every step's storage change plus overflow equals its inflow; continuity error 0.000 %
NUM-08 5.3.0 patched: PASS
NUM-08 6.0.0 patched: PASS
```

Flooding Loss is now 0.528 ac-ft (23,000 ft³ = 33,000 ft³ in − 10,000 ft³ stored).

## The fix

Set the overflow to what is left after the node fills, never below zero:

```diff
     if ( canPond == FALSE )
     {
-        Node[i].overflow = dV / dt;
+        // --- overflow is what is left after the node fills to fullVolume
+        Node[i].overflow = MAX(0.0, Node[i].oldVolume + dV -
+                                    Node[i].fullVolume) / dt;
         Node[i].newVolume = Node[i].fullVolume;
```

The 6.0.0 patch makes the same change in `commitNodeDepthState()` with `old_volume` and `rpt_full_volume`. For steps that start full the result is unchanged, and for plain junctions (`oldVolume = fullVolume = 0`) it is bit-identical.

**Effect on other models.** On the DW regression decks with flooding (extran5, extran6, routing/test3, user3; patched 5.3.0 and 6.0.0 CLIs vs unpatched), only two change, both slightly and in the right direction, and identically in both engines. extran6 (Type-1 pump wet wells) goes from -0.054% to -0.050% with flooding 6.840 → 6.839 ac-ft, and user3 (130 storage units) from -0.018% to -0.017%.

A run that ends while a storage unit is still flooding can show a larger continuity error after the fix. The same tank fed 100 cfs for the whole 10 minutes (`flood.inp` in the review notes) goes from -1.709% to +2.564%. That remainder is a separate booking offset. `routing_execute()` books every step's totals with trapezoid half-step weights (`massbal_updateRoutingTotals(routingStep/2.)` before and after the step), but the node overflow is already a step average, so the flooding total misses half of the last step's overflow (0.5 × 100 cfs × 30 s = 1,500 ft³ = 2.56% here). Before the fix, the double count partly cancelled it. When flooding ends before the run does, as in the test deck, the offset cancels out.
