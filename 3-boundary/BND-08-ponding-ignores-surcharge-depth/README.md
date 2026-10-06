# BND-08: With ponding allowed, a junction ponds at its rim and ignores its surcharge depth

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | Switching ALLOW_PONDING on turns a junction with a surcharge depth (a bolted cover or a force-main fitting) into an open pond at its rim. Its head, and the flow it drives into the outlet pipes, drop. In the test the head stays 3-10 ft below the 15 ft the junction should reach, the outlet pipe carries 6.2-8.7 cfs instead of 9.8-9.9 cfs, and 0.186 ac-ft is left ponded instead of 0.016 ac-ft. The Ysur value is silently ignored. |
| **Reached from** | Dynamic wave routing with ALLOW_PONDING YES and a junction or divider with both Ysur > 0 and Apond > 0 |
| **5.3.0** | `setNodeDepth()` in [`src/legacy/engine/dynwave.c:658`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L658) and [`:746`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L746); `node_getPondedArea()` in [`node.c:544`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L544) |
| **5.2.4** | Same, [`src/solver/dynwave.c:662`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dynwave.c#L662) and [`:748`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dynwave.c#L748) |
| **6.0.0** | Reproduces in `DWSolver::setNodeDepth()` and `commitNodeDepthState()`, [`src/engine/hydraulics/DynamicWave.cpp:3812`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/DynamicWave.cpp#L3812) and [`:4208`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/DynamicWave.cpp#L4208) |
| **Since** | Every release: 5.0.022 already has `if ( canPond == FALSE ) yMax += Node[i].surDepth;` |
| **Fix** | Use Ymax + Ysur as the ponding threshold: [`BND-08_swmm530.patch`](BND-08_swmm530.patch), [`BND-08_swmm600.patch`](BND-08_swmm600.patch) |

## The problem

The input file reference defines the two junction parameters together ([JUNCTIONS], and the same for [DIVIDERS]):

> **Ysur** maximum additional pressure head above the ground elevation that the junction can sustain under surcharge conditions
> **Apond** area subjected to surface ponding once water depth exceeds Ymax + Ysur

Junction J1 has Ymax = 5 ft, Ysur = 10 ft (a bolted cover) and Apond = 1,000 ft², with ALLOW_PONDING YES. It receives 10 cfs, and its 1 ft outlet pipe C1 cannot carry that at the rim. By the definition above, J1 should pressurize up to 15 ft, which drives about 9.8 cfs through C1, and pond only the small excess above 15 ft.

In 5.2.4, 5.3.0 and 6.0.0, J1 starts to pond at 5 ft. Its head then rises only as fast as the 1,000 ft² pond fills, from 6.1 ft at 5 min to 12 ft at 50 min. C1 carries 6.2-8.7 cfs over that time, and 0.186 ac-ft is ponded at the end of the hour. The same junction with ALLOW_PONDING NO pressurizes to 15 ft as intended. So turning ponding on changes the junction from a sealed manhole into an open one.

## Why it happens

Whether a node can pond decides both the ponding threshold and the flood threshold, and the surcharge depth is added only when the node cannot pond:

```c
// src/legacy/engine/dynwave.c, setNodeDepth()
canPond = (AllowPonding && Node[i].pondedArea > 0.0);
isPonded = (canPond && Node[i].newDepth > Node[i].fullDepth);
...
// --- don't allow a newly ponded node to rise much above full depth
if ( canPond && yNew > Node[i].fullDepth )
    yNew = Node[i].fullDepth + FUDGE;
...
yMax = Node[i].fullDepth;
if ( canPond == FALSE ) yMax += Node[i].surDepth;
```

`node_getPondedArea()` likewise switches to the ponded area at `depth > Node[nodeIndex].fullDepth`. For a pondable node the band between Ymax and Ymax + Ysur never exists: water spreads over Apond from the rim up. 6.0.0 copies these rules in `DWSolver::setNodeDepth()`, `commitNodeDepthState()` (`if (!can_pond) y_max += t.sur_depth;`) and `node::getPondedArea()`.

The Hydraulics Reference Manual (Vol. II, section 3.3.7) describes ponding only for "a junction node with no surcharge depth". It does not say that a surcharge depth is dropped when both are given. The input file reference gives the rule this test checks. The SWMM 5 user's manual agrees: it describes Surcharge Depth as the extra depth allowed before the junction floods, and Ponded Area as the area occupied by ponded water after flooding occurs.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-08_bolted-cover-pond.inp`](BND-08_bolted-cover-pond.inp) | J1 (Ymax 5 ft, Ysur 10 ft, Apond 1,000 ft²), ALLOW_PONDING YES, 10 cfs into J1, 1 ft pipe C1 (200 ft) to a free outfall. 1 h, fixed 2 s step. |
| [`BND-08_test.c`](BND-08_test.c) | Legacy toolkit (5.2.4, 5.3.0). Prints J1's depth, its stored volume (a junction stores only ponded water) and C1's flow. Checks that whenever J1 holds more than 1 ft³, its depth is at least Ymax + Ysur = 15 ft. |
| [`BND-08_test6.c`](BND-08_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh BND-08            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-08 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0 shown; 5.2.4 and 6.0.0 print the same rows, and 6.0.0, whose step loop also records the last step, reports up to 7948 ft³ ponded):

```
  time   J1 depth   J1 ponded vol   C1 flow
 (min)       (ft)           (ft3)     (cfs)
     5       6.12            1182      6.22
    10       7.14            2237      6.72
    15       8.02            3157      7.13
    20       8.80            3965      7.47
    30      10.10            5315      8.00
    40      11.13            6388      8.40
    50      11.96            7250      8.71
Lowest J1 depth while ponded: 5.00 ft (Ymax + Ysur = 15.00 ft)
FAIL: J1 ponds at a depth of 5.00 ft, below Ymax + Ysur = 15 ft (up to 7946 ft3 ponded)
BND-08 5.3.0 base: FAIL
```

**With the fix**, both engines print:

```
  time   J1 depth   J1 ponded vol   C1 flow
 (min)       (ft)           (ft3)     (cfs)
     5      15.09             107      9.79
    10      15.15             168      9.80
    15      15.20             224      9.82
    20      15.25             275      9.84
    30      15.33             364      9.87
    40      15.41             438      9.89
    50      15.46             499      9.91
Lowest J1 depth while ponded: 15.00 ft (Ymax + Ysur = 15.00 ft)
PASS: J1 pressurizes to Ymax + Ysur before any water ponds
BND-08 5.3.0 patched: PASS
BND-08 6.0.0 patched: PASS
```

In the report, outfall outflow rises from 0.642 to 0.812 ac-ft and the final stored (ponded) volume falls from 0.186 to 0.016 ac-ft. The continuity error is -0.252% in both runs.

## The fix

Use Ymax + Ysur as the depth above which a node ponds, and keep Ymax + Ysur as the flood depth whether or not the node can pond:

```diff
     canPond = (AllowPonding && Node[i].pondedArea > 0.0);
-    isPonded = (canPond && Node[i].newDepth > Node[i].fullDepth);
+    yPond = Node[i].fullDepth + Node[i].surDepth;
+    isPonded = (canPond && Node[i].newDepth > yPond);
 ...
-        if ( canPond && yNew > Node[i].fullDepth )
-            yNew = Node[i].fullDepth + FUDGE;
+        if ( canPond && yNew > yPond )
+            yNew = yPond + FUDGE;
 ...
-    yMax = Node[i].fullDepth;
-    if ( canPond == FALSE ) yMax += Node[i].surDepth;
+    //     (a node can pond only above its surcharge depth)
+    yMax = yPond;
```

The same threshold goes into the other ponding clamp, into `node_getPondedArea()`, and, under dynamic wave, into the initial ponded volume in `initNodes()` (`flowrout.c`). That initial-volume change matters because input validation allows an initial depth up to Ymax + Ysur. Between Ymax and Ymax + Ysur a pondable junction now surcharges like any other junction. Above that it ponds over Apond as before.

The 6.0.0 patch makes the same changes in `DWSolver::setNodeDepth()`, `commitNodeDepthState()`, `initNodeStates()` and `node::getPondedArea()`. A 2D-coupled junction keeps its rim threshold, because its ponded area is the footprint of its 2D cell and 6.0.0 uses it to let the head follow the 2D surface.

Nodes with Ysur = 0, and nodes that cannot pond, are unchanged. If EPA intends the current behaviour instead, the input manual and the GUI help need correcting, and SWMM should warn when Ysur and Apond are both given with ponding on. The Node Flooding Summary's "Maximum Ponded Depth" column keeps its definition, maximum depth − Ymax, for every node.

**Effect on other models.** No regression deck has a junction with both Ysur > 0 and Apond > 0. The two DW decks that pond (update_v52/CoS-Reduced-Outlets and CoS-Reduced-Inlets, 16 and 11 pondable junctions with Ysur = 0) and extran5 give byte-identical reports in both patched engines.
