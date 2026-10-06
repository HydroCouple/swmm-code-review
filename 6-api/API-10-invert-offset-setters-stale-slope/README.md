# API-10: Setting a node invert or a conduit offset before the run leaves the conduit's slope and capacity unchanged

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | The setters return 0 and the new invert or offset reads back, but the conduit keeps the slope, full-flow capacity and kinematic-wave conveyance computed at `swmm_open()`. In the test, raising J1 by 1 ft should double C1's slope (0.0025 to 0.0050) and stop all flooding; through the API, C1 keeps slope 0.0025 and J1 floods 862 ft³ instead of 0. Lowering the outlet offset the other way gives 862 ft³ instead of 4,300 ft³. No warning. |
| **Reached from** | Toolkit API before the run starts. 5.3.0: `swmm_setValueExpanded()` with `swmm_NODE_ELEV`, `swmm_LINK_OFFSET1` or `swmm_LINK_OFFSET2` between `swmm_open()` and `swmm_start()`. 6.0.0: `swmm_node_set_invert_elev()`, `swmm_link_set_offset_up()` or `swmm_link_set_offset_dn()` between `swmm_engine_open()` and `swmm_engine_initialize()` |
| **5.3.0** | `setNodeValue()` and `setLinkValue()` in [`src/legacy/engine/swmm5.c:2293`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2293) and [`:2409-2414`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2409-L2414) only store the value; slope, beta and qFull are set in `conduit_validate()`, [`src/legacy/engine/link.c:1080-1138`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1080-L1138), during `swmm_open()` |
| **5.2.4** | Not affected: `swmm_setValue()` has no node invert or link offset property ([`src/solver/swmm5.c:867`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L867)) |
| **6.0.0** | Reproduces: [`swmm_node_set_invert_elev()`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_nodes_impl.cpp#L146-L153) and [`swmm_link_set_offset_up/_dn()`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_links_impl.cpp#L272-L290) only store the value; the slope is computed during open in `resolve_cross_references()` ([`src/engine/input/PostParseResolver.cpp:3473`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/PostParseResolver.cpp#L3473)) |
| **Since** | 5.3.0, when the setters were added (fork commit 5b87a2b5, December 2024, #204); 6.0.0 setters from 4e29c886 (March 2026) |
| **Fix** | Recompute the slope and the quantities that depend on it in the setters: [`API-10_swmm530.patch`](API-10_swmm530.patch), [`API-10_swmm600.patch`](API-10_swmm600.patch) Apply after API-01 (its `Requires:` line). |

## The problem

Both toolkit APIs let a program change a node's invert elevation or a conduit's end offsets after the input file is read and before the run starts. A script that adjusts inverts (for a design alternative, or to correct survey data) gets return code 0, and the getters return the new values, but the conduit's slope is the one from the input file. Everything that depends on the slope uses the old value: the kinematic-wave conveyance, the full-flow capacity, normal-flow limiting under dynamic wave, conduit lengthening and the report's capacity figures.

In the test, C1 (400 ft of 1.5 ft pipe, n = 0.013) runs from J1 (invert 100 ft) to J2 (99 ft) and carries a triangular inflow peaking at 6.5 cfs, under kinematic wave. At slope 0.0025 its full-flow capacity is 5.25 cfs, so J1 floods 862 ft³. Raising J1 to 101 ft, or C1's inlet offset to 1 ft, doubles the slope; the capacity becomes 7.43 cfs and nothing floods. Through the API, `LINK_SLOPE` still reads 0.0025, `LINK_FULLFLOW` 5.25 cfs, and J1 floods the same 862 ft³. Raising C1's outlet offset by 0.5 ft halves the slope and should flood 4,300 ft³; through the API it is again 862 ft³.

## Why it happens

`conduit_validate()` runs once, from `project_validate()` inside `swmm_open()`, and derives everything from the slope at that moment:

```c
// src/legacy/engine/link.c, conduit_validate()
    // --- compute conduit slope
    slope = conduit_getSlope(j);
    Conduit[k].slope = slope;
    ...
    // --- compute full flow through cross section
    if ( Link[j].xsect.type == DUMMY ) Conduit[k].beta = 0.0;
    else Conduit[k].beta = PHI * sqrt(fabs(slope)) / roughness;
    Link[j].qFull = Link[j].xsect.sFull * Conduit[k].beta;
    Conduit[k].qMax = Link[j].xsect.sMax * Conduit[k].beta;
```

`conduit_getSlope()` uses `Node[].invertElev` and `Link[].offset1/offset2`. The setters change those and nothing else:

```c
// src/legacy/engine/swmm5.c, setNodeValue(), simulation not started
case swmm_NODE_ELEV:
    Node[index].invertElev = value / UCF(LENGTH);
    return 0;
...
// setLinkValue()
case swmm_LINK_OFFSET1:
    Link[index].offset1 = value / UCF(LENGTH);
    return 0;
```

`swmm_start()` does not re-run `conduit_validate()`. Under dynamic wave, the momentum equation reads node inverts and offsets directly, so the water surface profile follows the edit, but beta, qFull and the lengthened length do not. Under kinematic wave, beta is the whole flow law.

6.0.0 has the same split: the resolver computes the conduit slope during `swmm_engine_open()` and `recompute_conduit_flow_properties()` derives beta and `q_full` from it. The invert and offset setters store the value only. (The roughness setter does call `recompute_conduit_flow_properties()`, so the pattern for a fix is already there.)

## How to reproduce

| File | What it is |
|---|---|
| [`API-10_base.inp`](API-10_base.inp) | J1 (100 ft) to J2 (99 ft) to outfall O1 (97 ft) through C1 and C2, kinematic wave; inflow at J1 peaks at 6.5 cfs at 1:00 |
| [`API-10_j1-101.inp`](API-10_j1-101.inp), [`API-10_offset1-1.inp`](API-10_offset1-1.inp), [`API-10_offset2-0.5.inp`](API-10_offset2-0.5.inp) | The base deck with J1's invert at 101 ft, C1's inlet offset at 1 ft, or C1's outlet offset at 0.5 ft |
| [`API-10_test.c`](API-10_test.c) | Legacy API: for each setter, opens the base deck, sets the value before `swmm_start()`, reads C1's `LINK_SLOPE` and `LINK_FULLFLOW`, runs, and compares them, C1's peak flow and J1's flooded volume with a run of the edited deck (5.3.0 only, `#ifdef`; 5.2.4 prints PASS because the setters do not exist) |
| [`API-10_test6.c`](API-10_test6.c) | The same with the 6.0.0 setters before `swmm_engine_initialize()` (slope, peak flow and flooded volume; 6.0.0 has no full-flow getter) |
| [`API-10_swmm530.patch`](API-10_swmm530.patch), [`API-10_swmm600.patch`](API-10_swmm600.patch) | The fixes |

```sh
tools/run-test.sh API-10            # 5.2.4 PASS, 5.3.0 FAIL, 6.0.0 FAIL
tools/run-test.sh API-10 --patched  # 5.3.0 PASS, 6.0.0 PASS
```

**Without the fix**, all three setters leave C1 as it was:

```
API-10_base.inp unchanged: C1 slope 0.002500, full flow 5.252 cfs, peak 5.680 cfs, J1 flooded 862 ft3

Set before swmm_start   rc | C1 slope          | C1 full flow    | C1 peak flow    | J1 flooded (ft3)
                           |      API     .inp |     API    .inp |     API    .inp |      API     .inp
NODE_ELEV J1 = 101       0 | 0.002500 0.005000 |   5.252   7.428 |   5.680   6.450 |      862        0
LINK_OFFSET1 C1 = 1      0 | 0.002500 0.005000 |   5.252   7.428 |   5.680   6.450 |      862        0
LINK_OFFSET2 C1 = 0.5    0 | 0.002500 0.001250 |   5.252   3.714 |   5.680   4.018 |      862     4300
FAIL: 3 of 3 invert/offset setters returned 0 but C1 kept the slope and capacity computed at swmm_open (after raising J1: slope 0.002500 instead of 0.005000, J1 flooded 862 ft3 instead of 0)
API-10 5.3.0 base: FAIL
```

```
Set before initialize   rc | C1 slope          | C1 peak flow    | J1 flooded (ft3)
                           |      API     .inp |     API    .inp |      API     .inp
invert J1 = 101          0 | 0.002500 0.005000 |   5.680   6.450 |      862        0
offset_up C1 = 1         0 | 0.002500 0.005000 |   5.680   6.450 |      862        0
offset_dn C1 = 0.5       0 | 0.002500 0.001250 |   5.680   4.018 |      862     4300
FAIL: 3 of 3 invert/offset setters returned 0 but C1 kept the slope computed at open (after raising J1: slope 0.002500 instead of 0.005000, J1 flooded 862 ft3 instead of 0)
API-10 6.0.0 base: FAIL
```

**With the fix**, both engines give the edited-deck results, with the same numbers:

```
NODE_ELEV J1 = 101       0 | 0.005000 0.005000 |   7.428   7.428 |   6.450   6.450 |        0        0
LINK_OFFSET1 C1 = 1      0 | 0.005000 0.005000 |   7.428   7.428 |   6.450   6.450 |        0        0
LINK_OFFSET2 C1 = 0.5    0 | 0.001250 0.001250 |   3.714   3.714 |   4.018   4.018 |     4300     4300
PASS: NODE_ELEV, LINK_OFFSET1 and LINK_OFFSET2 set before swmm_start() give the same slope, capacity and flows as the same values in the input file
API-10 5.3.0 patched: PASS
```

```
invert J1 = 101          0 | 0.005000 0.005000 |   6.450   6.450 |        0        0
offset_up C1 = 1         0 | 0.005000 0.005000 |   6.450   6.450 |        0        0
offset_dn C1 = 0.5       0 | 0.001250 0.001250 |   4.018   4.018 |     4300     4300
PASS: invert and offsets set before initialize give the same slope and flows as the same values in the input file
API-10 6.0.0 patched: PASS
```

## The fix

**5.3.0.** The second half of `conduit_validate()`, from "compute conduit slope" to the end, becomes its own function `link_updateSlope(j)`, unchanged. `conduit_validate()` calls it where the code used to be, and the setters call it for the conduits they affect:

```diff
     if ( Link[j].xsect.type == FILLED_CIRCULAR )
     {
         Link[j].offset1 += Link[j].xsect.yBot;
         Link[j].offset2 += Link[j].xsect.yBot;
     }
+    link_updateSlope(j);
+}
+
+void  link_updateSlope(int j)
 ...
+{
+    int    k = Link[j].subIndex;
+    double aa;
+    double lengthFactor, roughness, slope;
 
     // --- compute conduit slope
     slope = conduit_getSlope(j);
```

```diff
         case swmm_NODE_ELEV:
+        {
+            int j;
             Node[index].invertElev = value / UCF(LENGTH);
+
+            // --- update the slopes of conduits connected to the node
+            for (j = 0; j < Nobjects[LINK]; j++)
+            {
+                if (Link[j].type == CONDUIT &&
+                    (Link[j].node1 == index || Link[j].node2 == index))
+                    link_updateSlope(j);
+            }
             return 0;
+        }
 ...
         case swmm_LINK_OFFSET1:
             Link[index].offset1 = value / UCF(LENGTH);
+            if (Link[index].type == CONDUIT) link_updateSlope(index);
             return 0;
```

The split-off code can run twice for the same conduit, so it now resets `modLength` to the conduit's length before the lengthening step; on the first call that is already its value. Everything else in it gives the same result when repeated (a conduit reversed at open has a positive slope on the second call). An edit that makes a conduit adverse under dynamic wave reverses it, as `swmm_open()` would for the same input file.

**6.0.0.** A new `recompute_conduit_slope(ctx, j)` in `PostParseResolver.cpp` repeats the resolver's slope steps for one conduit (minimum drop, minimum slope, sign, the dynamic-wave reversal, without the warnings) and calls the existing `recompute_conduit_flow_properties()`. The three setters call it when the engine is in the OPENED state; in the BUILDING state the resolver has not run yet and computes the slope itself. Lengthening is applied later, in `Router::init()`, from the updated slope.

**Effect on other models.** Only runs that call these setters change. The 5.3.0 split moves no statement out of the order it ran in at open; `CoS-Reduced-Outlets.inp` (dynamic wave with lengthening), `extran1.inp` (force mains), `user5.inp` (irregular channels) and `swc25.inp` (kinematic wave) from the regression suite give byte-identical reports and binary output files with both patched engines.

**Not covered.** `swmm_open()` also raises a node's full depth to the crown of each connected conduit (`link_validate()`); the setters still do not. In 6.0.0, an edit that reverses a conduit happens after the virtual-junction and inlet references were resolved at open, and those are not rebuilt. 6.0.0's `swmm_link_set_length()` leaves the slope stale in the same way and is not changed here. In 5.3.0, `swmm_LINK_OFFSET1/2` on a `FILLED_CIRCULAR` conduit stores the value without the sediment depth that `conduit_validate()` adds at open; 6.0.0's setters add it.
