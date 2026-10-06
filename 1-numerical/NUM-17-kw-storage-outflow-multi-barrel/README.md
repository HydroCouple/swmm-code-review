# NUM-17: Under KINWAVE and STEADY, a storage unit drains into a multi-barrel conduit through one barrel only

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A storage unit whose outlet conduit has N barrels drains as if it had one: each barrel carries 1/N of the flow it should. In the test, a 2-barrel outlet that should pass 30 cfs at 1.19 ft passes 22.6 cfs, the storage fills to its 6 ft maximum and floods 6.72 ac-ft (2.19 Mgal), the same as with a single barrel. The report's Max/Full Flow for the conduit (0.54) suggests spare capacity. Nothing warns the user. |
| **Reached from** | FLOW_ROUTING KINWAVE or STEADY, a storage unit drained by a conduit with Barrels > 1 |
| **5.3.0** | `storage_getOutflow()` in [`src/legacy/engine/node.c:1028`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L1028), used by `kinwave_execute()` ([`kinwave.c:100`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/kinwave.c#L100)) and `steadyflow_execute()` ([`flowrout.c:773`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/flowrout.c#L773)) |
| **5.2.4** | Same code, [`src/solver/node.c:1042`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L1042) |
| **6.0.0** | Reproduces: `storageConduitOutflow()` ports the same function ([`src/engine/hydraulics/KinematicWave.cpp:68`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/KinematicWave.cpp#L68)) |
| **Since** | The first commit of the repository (2014), so every release |
| **Fix** | Multiply by the barrel count: [`NUM-17_swmm530.patch`](NUM-17_swmm530.patch), [`NUM-17_swmm600.patch`](NUM-17_swmm600.patch) |

## The problem

Under kinematic wave and steady flow routing, the flow from a storage unit into a conduit is the conduit's normal flow at the storage depth, or its full flow once the storage is deeper than the conduit. That is computed for one barrel, and then used as the flow into the whole conduit, so a multi-barrel outlet conveys the flow of a single barrel.

In the test, a 5000 ft2 storage unit (6 ft deep) receives a steady 30 cfs and drains through two 2-ft circular barrels, 400 ft long at 1 % slope, each able to carry about 22.6 cfs. Two barrels should carry 15 cfs each, at a storage depth equal to the normal depth of 15 cfs: 1.19 ft by Manning's equation (dynamic wave routing gives the same). Under KINWAVE and STEADY, the conduit's peak flow is 24.36 cfs (Max/Full Flow 0.54) and from then on it carries 22.62 cfs, the full flow of one barrel, while the storage sits at 6 ft and floods 6.72 ac-ft over 12 h. With a single barrel the flooding is 6.73 ac-ft.

## Why it happens

```c
// src/legacy/engine/node.c, storage_getOutflow()
// --- return 0 if conduit empty or full flow if full
if ( y <= 0.0 ) return 0.0;
if ( y >= Link[i].xsect.yFull ) return Link[i].qFull;          // per barrel

// --- if partially full, return normal flow
k = Link[i].subIndex;
a = xsect_getAofY(&Link[i].xsect, y);
return Conduit[k].beta * xsect_getSofA(&Link[i].xsect, a);     // per barrel
```

`Link.qFull` and `Conduit.beta` describe one barrel (`Link[j].qFull = Link[j].xsect.sFull * Conduit[k].beta`, [`link.c:1138`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1138)). `conduit_getInflow()` returns this value as the conduit's inflow, and the routing functions treat it as the total over all barrels:

```c
// src/legacy/engine/kinwave.c, kinwave_execute()
qin = (*qinflow) / Conduit[k].barrels / Qfull;

// src/legacy/engine/flowrout.c, steadyflow_execute()
q = (*qin) / Conduit[k].barrels;
```

So each barrel receives 1/N of one barrel's flow. Inflows to conduits from other node types are totals, and are right. 6.0.0's `storageConduitOutflow()` returns the same per-barrel values to a caller that divides by the barrel count.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-17_kinwave.inp`](NUM-17_kinwave.inp) | Storage S1 (5000 ft2, 6 ft) with 30 cfs inflow -> 2-barrel 2-ft circular conduit C1 (400 ft, 1 %) -> FREE outfall; KINWAVE; 12 h |
| [`NUM-17_steady.inp`](NUM-17_steady.inp) | The same with STEADY routing |
| [`NUM-17_test.c`](NUM-17_test.c) | Runs both decks through the legacy toolkit (5.2.4, 5.3.0); at the end checks the conduit flow, the storage depth against the Manning normal depth of 15 cfs per barrel, and that the storage never floods |
| [`NUM-17_test6.c`](NUM-17_test6.c) | The same through the 6.0.0 C API |

Tolerances: flow 30 +/- 0.3 cfs, depth within 0.05 ft of the normal depth (SWMM's tabulated circular geometry differs from the exact circle by about 0.001 ft here), no flooding.

```sh
tools/run-test.sh NUM-17            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-17 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, all three engines print:

```
Expected: conduit flow 30 cfs, storage depth 1.190 ft (normal depth of 15 cfs per barrel), no flooding
Deck                    C1 flow   S1 depth  S1 flooding
                          (cfs)       (ft)    max (cfs)
NUM-17_kinwave.inp       22.623      6.000        7.377
NUM-17_steady.inp        22.623      6.000        7.377
FAIL: the 2-barrel outlet passes 22.623 cfs (KINWAVE) and 22.623 cfs (STEADY) instead of 30; the storage is 6.000 / 6.000 ft deep instead of 1.190 and floods at up to 7.377 / 7.377 cfs
```

**With the fix**, 5.3.0 and 6.0.0:

```
NUM-17_kinwave.inp       30.000      1.191        0.000
NUM-17_steady.inp        30.000      1.191        0.000
PASS: both barrels carry the storage outflow (30 cfs at the normal depth)
NUM-17 5.3.0 patched: PASS
NUM-17 6.0.0 patched: PASS
```

The KINWAVE report's flooding loss goes from 6.724 ac-ft to 0, and the conduit's Max/Full Flow from 0.54 to 0.66.

## The fix

```diff
     // --- return 0 if conduit empty or full flow if full
+    //     (qFull and beta are per barrel)
+    k = Link[i].subIndex;
     if ( y <= 0.0 ) return 0.0;
-    if ( y >= Link[i].xsect.yFull ) return Link[i].qFull;
+    if ( y >= Link[i].xsect.yFull ) return Link[i].qFull * Conduit[k].barrels;
 
     // --- if partially full, return normal flow
-    k = Link[i].subIndex;
     a = xsect_getAofY(&Link[i].xsect, y);
-    return Conduit[k].beta * xsect_getSofA(&Link[i].xsect, a);
+    return Conduit[k].beta * xsect_getSofA(&Link[i].xsect, a) * Conduit[k].barrels;
```

A single-barrel conduit multiplies by exactly 1, so its results are bit-identical. The 6.0.0 patch makes the same change in `storageConduitOutflow()`.

**Effect on other models.** No regression deck routes a storage unit into a multi-barrel conduit under KINWAVE or STEADY. `swc11` and `swc17` (KINWAVE, a storage unit with a single-barrel outlet, 3 years) and `Example1` (KINWAVE) give identical reports in both engines.
