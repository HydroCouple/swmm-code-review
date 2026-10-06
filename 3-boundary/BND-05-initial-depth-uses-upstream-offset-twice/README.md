# BND-05: A conduit's inlet offset is added to the initial depth of its downstream node

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | Under dynamic wave, a junction downstream of a conduit that has an initial flow and an inlet offset starts too deep by half (or 1/n) of the offset difference. The false head drains out in the first minutes as flow that is in no storage term: in the test, J2 starts at 2.59 ft instead of 0.59 ft, the outfall peaks at 1.41 ft instead of 0.60 ft, and the routing continuity error is -19.5% instead of 1.1%. No warning. |
| **Reached from** | `FLOW_ROUTING DYNWAVE`, no hot start file, a conduit with a non-zero `InitFlow` in [CONDUITS] and `InOffset` ≠ `OutOffset`, whose downstream node is a junction or divider without an `InitDepth` |
| **5.3.0** | `initNodeDepths()` in [`src/legacy/engine/flowrout.c:366`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/flowrout.c#L366) |
| **5.2.4** | Same code, [`src/solver/flowrout.c:360`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/flowrout.c#L360) |
| **6.0.0** | Reproduces: the port in `SWMMEngine::initialize()`, [`src/engine/core/SWMMEngine.cpp:1025`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L1025), copies the same expression and gives the same numbers. |
| **Since** | Every release in the repository (present in 5.0.022, the oldest tag) |
| **Fix** | Use the outlet offset at the downstream node: [`BND-05_swmm530.patch`](BND-05_swmm530.patch), [`BND-05_swmm600.patch`](BND-05_swmm600.patch) |

## The problem

When a junction has no initial depth of its own, dynamic wave routing seeds it with the average water level of the links connected to it. A conduit with an initial flow starts at normal depth, so a junction between two such conduits should start near that depth.

The level a conduit contributes is measured from the conduit's invert at that end, so it is `depth + InOffset` at the upstream node and `depth + OutOffset` at the downstream node. SWMM uses `depth + InOffset` at both ends. A conduit that drops into a manhole from a height therefore lifts the manhole downstream of it by that height (divided by the number of links averaged).

The test network is J1 → C1 → J2 → C2 → free outfall O1, both conduits 2 ft circular, 500 ft, n = 0.013, with an initial flow of 3 cfs and the same 3 cfs entering at J1. C1 enters J1 4 ft above its invert and has no outlet offset. Neither junction has an initial depth.

| | C1 depth | C2 depth | J2 initial depth | Outfall max depth | Outflow / inflow (30 min) | Continuity error |
|---|---|---|---|---|---|---|
| unpatched | 0.424 ft | 0.746 ft | 2.585 ft | 1.41 ft at 0:00 | 0.149 / 0.124 ac-ft | -19.537% |
| with the fix | 0.424 ft | 0.746 ft | 0.585 ft | 0.60 ft at 0:30 | 0.120 / 0.124 ac-ft | 1.149% |

J2 starts 2 ft (half of C1's 4 ft offset) above the water in both of its conduits. That head pushes water through C2 that no inflow or initial storage term accounts for: about 0.027 ac-ft (1,200 ft³), so the run reports 20% more outflow than inflow. A modeller looking at the first report periods sees a start-up surge at the outfall that the inflows cannot explain. The same error then spreads one link further: `initLinkDepths()` runs next and fills conduits without an initial flow from the node depths just set.

Initial depths entered by the user are not affected, nor are hot-started runs.

## Why it happens

```c
// src/legacy/engine/flowrout.c, initNodeDepths()
    // --- total up flow depths in all connecting links into nodes
    for (i = 0; i < Nobjects[LINK]; i++)
    {
        if ( Link[i].newDepth > FUDGE ) y = Link[i].newDepth + Link[i].offset1;
        else y = 0.0;
        n = Link[i].node1;
        Node[n].inflow += y;
        Node[n].outflow += 1.0;
        n = Link[i].node2;
        Node[n].inflow += y;          // y still includes offset1
        Node[n].outflow += 1.0;
    }
```

`Link[i].newDepth` is non-zero at this point only for conduits with an initial flow (`conduit_initState()` sets it to normal depth). Each node's depth is then `Node[n].inflow / Node[n].outflow`. For J2 that is `((0.424 + 4) + 0.746) / 2 = 2.585` ft instead of `((0.424 + 0) + 0.746) / 2 = 0.585` ft.

6.0.0 ported the routine with the same expression:

```cpp
// src/engine/core/SWMMEngine.cpp, SWMMEngine::initialize()
double y  = (ld > constants::FUDGE) ? ld + ctx_.links.offset1[uj] : 0.0;
...
if (n2 >= 0) { acc[static_cast<std::size_t>(n2)] += y; ++cnt[static_cast<std::size_t>(n2)]; }
```

## How to reproduce

| File | What it is |
|---|---|
| [`BND-05_offset-inlet.inp`](BND-05_offset-inlet.inp) | J1 → C1 (initial flow 3 cfs, inlet offset 4 ft) → J2 → C2 (3 cfs) → free outfall, 30 min of dynamic wave routing |
| [`BND-05_test.c`](BND-05_test.c) | Legacy toolkit (5.2.4, 5.3.0): reads the conduit and junction depths after `swmm_start()`, then runs the model and reads the routing continuity error |
| [`BND-05_test6.c`](BND-05_test6.c) | The same through the 6.0.0 C API |

The tests require J2's initial depth to lie between the water levels of the two conduit ends that meet there (within 0.05 ft), and the routing continuity error to stay within 5%.

```sh
tools/run-test.sh BND-05            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh BND-05 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 5.2.4 prints the same, 6.0.0 the same except -19.533%):

```
Initial state (ft)
  C1 depth (normal depth, 3 cfs)         0.424
  C2 depth (normal depth, 3 cfs)         0.746
  J1 depth                               4.424
  J2 depth                               2.585
  water levels of C1/C2 ends at J2       0.424 ..  0.746
Routing continuity error (%)          -19.537
FAIL: J2 starts at 2.585 ft (water levels of its conduits: 0.424-0.746 ft), outside that range; routing continuity error -19.537%, beyond 5%
BND-05 5.3.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Initial state (ft)
  C1 depth (normal depth, 3 cfs)         0.424
  C2 depth (normal depth, 3 cfs)         0.746
  J1 depth                               4.424
  J2 depth                               0.585
  water levels of C1/C2 ends at J2       0.424 ..  0.746
Routing continuity error (%)            1.149
PASS: J2 starts between the water levels of its conduits and routing continuity is 1.149%
BND-05 5.3.0 patched: PASS
```

The remaining 1.1% is the rest of the start-up transient in this short run and is not part of this issue.

## The fix

Take the downstream node's level from the outlet offset:

```diff
         n = Link[i].node1;
         Node[n].inflow += y;
         Node[n].outflow += 1.0;
+        if ( Link[i].newDepth > FUDGE ) y = Link[i].newDepth + Link[i].offset2;
         n = Link[i].node2;
         Node[n].inflow += y;
```

The 6.0.0 patch computes `y2 = ld + offset2` and adds it at `n2`. Links with zero depth still count as zero in the average, as before.

Effect on other models: results change only for conduits that have an initial flow and different inlet and outlet offsets. None of the 73 decks of the SWMM regression test suite has one: the only dynamic wave deck with initial flows, extran8a, has equal offsets. extran8a, extran1 and Example2 give identical reports with the patched 5.3.0 and 6.0.0 CLIs.
