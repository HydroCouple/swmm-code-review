# BND-07: A culvert on an adverse slope loses inlet control and gets it at its outlet instead

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | Under dynamic wave, a culvert whose outlet invert is even slightly higher than its inlet invert is never inlet controlled. In the test (3 ft concrete culvert, 60 cfs) the headwater drops from 4.88 ft to 2.37 ft when the outlet invert is raised by 0.04 ft. Reverse flow through the same culvert is instead inlet controlled at the barrel outlet, with the inlet's coefficients (2.02 ft becomes 4.88 ft). No warning. |
| **Reached from** | `FLOW_ROUTING DYNWAVE`, a conduit with a culvert code in [XSECTIONS] and an outlet invert above its inlet invert (inverts plus offsets), which survey noise produces easily on flat culverts |
| **5.3.0** | `conduit_validate()` reverses the conduit in [`src/legacy/engine/link.c:1089`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1089); `dwflow_findConduitFlow()` applies inlet control only to positive flow at node1, [`src/legacy/engine/dwflow.c:328`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dwflow.c#L328); `culvert_getInflow()` measures the head from node1, [`src/legacy/engine/culvert.c:215`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/culvert.c#L215) |
| **5.2.4** | Same code, [`src/solver/link.c:1086`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L1086) and [`src/solver/dwflow.c:248`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dwflow.c#L248) |
| **6.0.0** | Reproduces with the same numbers: the reversal in [`src/engine/input/PostParseResolver.cpp:3490`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/PostParseResolver.cpp#L3490) and the culvert branch of `DWSolver::applyFlowLimits()` in [`src/engine/hydraulics/DynamicWave.cpp:2589`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/DynamicWave.cpp#L2589) follow the legacy code |
| **Since** | 5.1, when culvert inlet control was added (present in v5.1.1, the oldest 5.1 tag in the repository) |
| **Fix** | Apply inlet control at the inlet end of a reversed culvert, to its negative flow: [`BND-07_swmm530.patch`](BND-07_swmm530.patch), [`BND-07_swmm600.patch`](BND-07_swmm600.patch) |

## The problem

Assigning a culvert code to a conduit makes SWMM check, at every dynamic wave iteration, whether the flow into the culvert is limited by its inlet (FHWA HDS-5 inlet-control curves, Hydraulics Reference Manual section 7.4). The manual evaluates the curve with the head at "the inlet node of the culvert link" and the invert "at its inlet end".

Dynamic wave routing also reverses every conduit whose slope is adverse, so that its node1 is the lower end. The culvert check was written for the conduit's internal orientation: it applies to positive flow, at node1. After the reversal, the user's inlet node is node2 and the user's forward flow is negative. So:

- **Forward flow is never inlet controlled.** The culvert behaves as a plain pipe, with the headwater set by the barrel alone.
- **Reverse flow is inlet controlled at the barrel outlet,** using the inlet's entrance coefficients and the head at the outlet node.

The test culvert is 3 ft circular concrete, code 1 (square edge with headwall), 60 ft long, inlet invert 100.00 ft at J1. Its outlet node J2 is at 99.98 ft (slope +0.033%) or 100.02 ft (-0.033%), nothing else changes. For 60 cfs the HDS-5 submerged inlet-control headwater is 4.878 ft.

| Flow | J2 invert | Unpatched | With the fix |
|---|---|---|---|
| forward, J1 headwater | 99.98 (+0.033%) | 4.877 ft | 4.877 ft |
| forward, J1 headwater | 100.02 (-0.033%) | **2.371 ft** | 4.878 ft |
| reverse, J2 headwater | 99.98 (+0.033%) | 2.371 ft | 2.371 ft |
| reverse, J2 headwater | 100.02 (-0.033%) | **4.877 ft** | 2.023 ft |

A 0.04 ft change in one invert moves the headwater by 2.5 ft. The adverse-slope culvert gives exactly the numbers of the positive-slope culvert with the flow direction swapped: SWMM treats J2 as its inlet. A designer checking whether a road crossing overtops would see 2.4 ft of headwater where HDS-5 gives 4.9 ft, and the report gives no hint: there is no warning, and with `[REPORT] INPUT YES` the Link Summary only lists the conduit's slope as negative.

Flat culverts are where this matters, because surveyed inverts of a culvert laid level often differ by a few hundredths of a foot in either direction.

## Why it happens

```c
// src/legacy/engine/link.c, conduit_validate()
    // --- reverse orientation of conduit if using dynamic wave routing
    //     and slope is negative
    if ( RouteModel == DW &&
         slope < 0.0 &&
         Link[j].xsect.type != DUMMY )
    {
        conduit_reverse(j, k);    // swaps node1/node2, offsets, loss coeffs.;
    }                             // direction = -1; culvertCode unchanged
```

```c
// src/legacy/engine/dwflow.c, dwflow_findConduitFlow()
    if ( q > 0.0 )
    {
        // --- check for inlet controlled culvert flow
        if ( xsect->culvertCode > 0 && !isFull )
            q = culvert_getInflow(linkIndex, q, h1);
        ...
    }
```

```c
// src/legacy/engine/culvert.c, culvert_getInflow()
    // --- find head relative to culvert's upstream invert
    //     (can be greater than yFull when inlet is submerged)
    y = h - (Node[Link[j].node1].invertElev + Link[j].offset1);
```

After `conduit_reverse()`, `q > 0` is flow from the user's outlet to the user's inlet, and node1 is the user's outlet. `Link[j].direction` records the reversal, but neither function looks at it. The slope correction in `culvert_getInflow()` (`scf = 0.5 * Conduit[k].slope`) also gets the reversed, positive slope.

6.0.0 reverses the conduit the same way in `PostParseResolver` and its `applyFlowLimits()` only runs the culvert branch for `q > 0.0`, with `y_in = h1 - (inv1 + z1)`.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-07_positive.inp`](BND-07_positive.inp) | 60 cfs enters at J1 and flows through culvert C1 (inverts 100.00 → 99.98) to J2, which drains through a 10 ft drop to a free outfall; 2 hours |
| [`BND-07_adverse.inp`](BND-07_adverse.inp) | The same with J2 at 100.02 |
| [`BND-07_reverse-positive.inp`](BND-07_reverse-positive.inp) | 60 cfs enters at J2 and flows back through C1 (J2 at 99.98) to J1, which drains freely |
| [`BND-07_reverse-adverse.inp`](BND-07_reverse-adverse.inp) | The same with J2 at 100.02 |
| [`BND-07_test.c`](BND-07_test.c) | Legacy toolkit (5.2.4, 5.3.0): runs the four decks and reads the steady depth at the node where the flow enters C1 |
| [`BND-07_test6.c`](BND-07_test6.c) | The same through the 6.0.0 C API |

The test requires the forward headwater to be at least the HDS-5 inlet-control headwater less 0.15 ft on both slopes, and the reverse headwater to be more than 1 ft below it (flow entering the barrel at its outlet is not inlet controlled).

```sh
tools/run-test.sh BND-07            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh BND-07 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 5.2.4 and 6.0.0 print the same):

```
HDS-5 inlet-control headwater for 60 cfs: 4.878 ft

Flow      Culvert slope    C1 flow (cfs)   depth where flow enters C1 (ft)
forward   +0.033%             60.00          J1  4.877
forward   -0.033%             60.00          J1  2.371
reverse   +0.033%             60.00          J2  2.371
reverse   -0.033%             60.00          J2  4.877
FAIL: forward flow is not inlet controlled (J1 4.877 / 2.371 ft on the +/- slope, inlet-control headwater 4.878 ft); reverse flow is inlet controlled at the culvert's outlet (J2 2.371 / 4.877 ft on the +/- slope)
BND-07 5.3.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Flow      Culvert slope    C1 flow (cfs)   depth where flow enters C1 (ft)
forward   +0.033%             60.00          J1  4.877
forward   -0.033%             60.00          J1  4.878
reverse   +0.033%             60.00          J2  2.371
reverse   -0.033%             60.00          J2  2.023
PASS: inlet control applies at the culvert's inlet on both slopes, and not to reverse flow
BND-07 5.3.0 patched: PASS
```

The adverse forward case is now 0.001 ft above the positive one, which is the HDS-5 slope correction (`-0.5 S`) with the sign of the user's slope. The two reverse cases still differ (2.371 and 2.023 ft): the conduit's internal orientation differs between them, and `dwflow_findConduitFlow()` treats positive and negative flow differently in other places too (upstream area weighting is applied only to positive flow). That is not part of this issue.

## The fix

`Link[j].direction` already says whether the conduit was reversed. The 5.3.0 patch adds the reversed case to `dwflow_findConduitFlow()`:

```diff
             q = checkNormalFlow(linkIndex, q, y1, y2, a1, r1);
     }
+    // --- check for inlet control of a culvert reversed for an adverse slope
+    //     (its inlet is now at node2 and its forward flow is negative)
+    else if ( q < 0.0 && xsect->culvertCode > 0 && !isFull )
+        q = culvert_getInflow(linkIndex, q, h2);
```

and makes `culvert_getInflow()` take the inlet end from the direction:

```diff
+    // --- only flow entering the inlet is inlet controlled (a conduit reversed
+    //     for an adverse slope has direction -1 and its inlet at node2)
+    if ( q0 * Link[j].direction <= 0.0 ) return q0;
 ...
-    y = h - (Node[Link[j].node1].invertElev + Link[j].offset1);
+    if ( Link[j].direction > 0 )
+        y = h - (Node[Link[j].node1].invertElev + Link[j].offset1);
+    else
+    {
+        y = h - (Node[Link[j].node2].invertElev + Link[j].offset2);
+        culvert.scf = -culvert.scf;
+    }
 ...
-    if ( q < q0 )
+    if ( q < fabs(q0) )
 ...
-        return q;
+        return q * Link[j].direction;
```

For a conduit that was not reversed (`direction = 1`) every line computes what it did before. The 6.0.0 patch makes the same change in `applyFlowLimits()`: it skips the culvert call for positive flow in a reversed conduit and adds a branch for negative flow that passes the head and invert at node2 and the negated slope to `culvert::getInflow()`.

Effect on other models: only conduits that have a culvert code and an adverse slope under dynamic wave change. None of the 73 decks of the SWMM regression test suite uses a culvert code. On the four decks here, the positive-slope runs give identical reports with and without the patch.

Not examined: 6.0.0's finite-volume solver (`FLOW_ROUTING FV`) has its own culvert implementation and also runs on the reversed network.
