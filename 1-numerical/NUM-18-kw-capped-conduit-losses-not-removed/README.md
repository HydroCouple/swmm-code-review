# NUM-18: Under STEADY and KINWAVE, a full conduit's seepage and evaporation are booked but never taken from the water

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | When the inflow to a lossy conduit exceeds its full flow, the conduit's seepage (and evaporation) loss is reported as Exfiltration (Evaporation) Loss although no water was removed: the same water also leaves as outflow or flooding. In the test the 2.29 ac-ft of exfiltration is counted twice, flooding is overstated by the same 2.29 ac-ft, and the continuity error is -11.5 % (STEADY) and -12.1 % (KINWAVE) instead of 0.0 % and -0.5 %. Nothing warns the user except the continuity error. |
| **Reached from** | FLOW_ROUTING STEADY or KINWAVE, a conduit with seepage ([LOSSES]) or evaporation (open section, [EVAPORATION]) whose inflow exceeds its full flow |
| **5.3.0** | `steadyflow_execute()` in [`src/legacy/engine/flowrout.c:778`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/flowrout.c#L778)-[`785`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/flowrout.c#L785); `kinwave_execute()` in [`src/legacy/engine/kinwave.c:151`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/kinwave.c#L151) |
| **5.2.4** | Same code, [`src/solver/flowrout.c:772`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/flowrout.c#L772)-[`779`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/flowrout.c#L779), [`src/solver/kinwave.c:153`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/kinwave.c#L153) |
| **6.0.0** | Reproduces in the steady-flow solve ([`src/engine/hydraulics/Routing.cpp:1050`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Routing.cpp#L1050)) and the kinematic-wave solve ([`src/engine/hydraulics/KinematicWave.cpp:362`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/KinematicWave.cpp#L362)) |
| **Since** | The first commit of the repository (2014), so every release |
| **Fix** | Cap the conduit's inflow at its full flow plus its loss: [`NUM-18_swmm530.patch`](NUM-18_swmm530.patch), [`NUM-18_swmm600.patch`](NUM-18_swmm600.patch) |

## The problem

Under steady flow and kinematic wave routing a conduit cannot carry more than its full flow. When more arrives, the conduit takes its full flow from the upstream node, which floods the rest, and delivers its full flow at the outlet. The conduit's loss rate is computed and stored for the mass balance before that cap, and is not taken from either flow. So the loss is booked, but the water it describes is still delivered at the outlet or flooded at the node.

In the test, junction J1 receives 40 cfs and drains through a 2-ft pipe, 2000 ft at 0.5 % (about 16 cfs full), with 50 in/hr of seepage (about 4.6 cfs). Over 6 h the report shows 19.806 ac-ft of inflow, 7.921 ac-ft of outflow, 11.886 ac-ft of flooding and 2.286 ac-ft of exfiltration: 2.29 ac-ft more out than in, a continuity error of -11.54 % under STEADY (-12.05 % under KINWAVE). The same deck with 5 cfs, which does not fill the pipe, gives 0.000 % under STEADY.

## Why it happens

```c
// src/legacy/engine/flowrout.c, steadyflow_execute()
q = (*qin) / Conduit[k].barrels;
...
// --- adjust flow for evap and infil losses
q -= link_getLossRate(j, SF, q, tStep);    // stores Conduit.evapLossRate / seepLossRate

// --- flow can't exceed full flow
if ( q > Link[j].qFull )
{
    q = Link[j].qFull;                     // outflow: full flow
    Conduit[k].a1 = Link[j].xsect.aFull;
    (*qin) = q * Conduit[k].barrels;       // inflow taken from the node: full flow too
}
```

Inflow and outflow are both the full flow, so the loss subtracted from `q` is lost when `q` is reset. `removeConduitLosses()` still books `Conduit[k].seepLossRate * barrels` as exfiltration ([`routing.c:979`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L979)).

Kinematic wave routing has the same gap. With a normalized inflow `qin >= 1` the inlet area is full, the continuity solve ends in its "full flow" branch (`*aout = ain`, [`kinwave.c:242`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/kinwave.c#L242)), so the outflow is the full flow, and the inflow is then capped at the full flow:

```c
// src/legacy/engine/kinwave.c, kinwave_execute()
qout = Beta1 * xsect_getSofA(pXsect, aout*Afull);
if ( qin > 1.0 ) qin = 1.0;                // loss rate q3 is booked but taken from neither
```

6.0.0 ports both solves with the same caps.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-18_steady.inp`](NUM-18_steady.inp) | Junction J1 (40 cfs) -> 2-ft circular conduit C1 (2000 ft, 0.5 %, seepage 50 in/hr) -> FREE outfall; STEADY; 6 h |
| [`NUM-18_kinwave.inp`](NUM-18_kinwave.inp) | The same with KINWAVE |
| [`NUM-18_test.c`](NUM-18_test.c) | Runs both decks through the legacy toolkit (5.2.4, 5.3.0) and checks the flow continuity error from `swmm_getMassBalErr()`, printing the report's continuity terms |
| [`NUM-18_test6.c`](NUM-18_test6.c) | The same through the 6.0.0 C API |

Water is conserved, so the test requires a continuity error within 1 %. With 5 cfs (pipe not full) the decks give 0.000 % (STEADY) and -0.69 % (KINWAVE); KINWAVE loses about 0.1 ac-ft while the empty pipe fills, with or without seepage (-0.54 % without it at 40 cfs).

```sh
tools/run-test.sh NUM-18            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-18 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0; 6.0.0 prints -12.269 % and -12.049 %):

```
Deck                    Inflow   Outflow  Flooding    Exfil.     Error
                       (ac-ft)   (ac-ft)   (ac-ft)   (ac-ft)       (%)
NUM-18_steady.inp       19.806     7.921    11.886     2.286   -11.542
NUM-18_kinwave.inp      19.806     7.877    11.886     2.286   -12.050
FAIL: the seepage of the full conduit is booked but not removed: flow continuity error -11.542 % (STEADY) and -12.050 % (KINWAVE)
```

**With the fix** the conduit takes its seepage from J1's water, so J1 floods 2.286 ac-ft less:

```
---- NUM-18 on 5.3.0 (patched) ----
NUM-18_steady.inp       19.806     7.921     9.600     2.286     0.000
NUM-18_kinwave.inp      19.806     7.877     9.600     2.286    -0.509
PASS: the full conduit's seepage is taken from the water it receives (continuity errors 0.000 % and -0.509 %)
---- NUM-18 on 6.0.0 (patched) ----
NUM-18_steady.inp       19.806     7.921     9.600     2.286    -0.727
NUM-18_kinwave.inp      19.806     7.877     9.600     2.286    -0.509
PASS: the full conduit's seepage is taken from the water it receives (continuity errors -0.727 % and -0.509 %)
```

Every flow and volume is the same in both engines. The remaining -0.727 % in 6.0.0's STEADY run is not this bug: 6.0.0 reports the full pipe's 0.144 ac-ft as Final Stored Volume under STEADY routing, where 5.3.0 reports 0, and it does so with or without this patch.

## The fix

Cap the inflow at the full flow plus the loss, so the upstream node supplies the water that seeps or evaporates:

```diff
             // --- adjust flow for evap and infil losses
-            q -= link_getLossRate(j, SF, q, tStep);
+            loss = link_getLossRate(j, SF, q, tStep);
+            q -= loss;
          
             // --- flow can't exceed full flow 
+            //     (the conduit then takes in its full flow plus its losses)
             if ( q > Link[j].qFull )
             {
                 q = Link[j].qFull;
                 Conduit[k].a1 = Link[j].xsect.aFull;
-                (*qin) = q * Conduit[k].barrels;
+                (*qin) = (q + loss) * Conduit[k].barrels;
             }
```

```diff
         qout = Beta1 * xsect_getSofA(pXsect, aout*Afull);
-        if ( qin > 1.0 ) qin = 1.0;
+
+        // --- a full conduit takes in its full flow plus its losses
+        if ( qin > 1.0 + q3 ) qin = 1.0 + q3;
```

The outflow of the full conduit stays its full flow; only the flow it draws from the node grows by the loss. A conduit with no loss (`loss = 0`, `q3 = 0`) or one that is not full gives bit-identical results. The 6.0.0 patch makes the same two changes.

**Effect on other models.** No regression deck routed with KINWAVE or STEADY has seepage. `Example4`, `swc1`, `swc8`, `swc19` and `swc23` (KINWAVE, with evaporation) give identical reports in both engines, so none of them has a lossy conduit running full.
