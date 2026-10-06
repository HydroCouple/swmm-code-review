# NUM-19: An outfall that both receives and sends flow books nothing to the mass balance

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Water that an outfall feeds into the network is missing from the flow mass balance: in the tests, a continuity error of -99 % with SKIP_STEADY_STATE and -1,255 % with a 1 cfs inflow at the outfall, where the same network otherwise gives -0.06 %. The External Inflow total is wrong by the same amount. The report shows the error but nothing points to its cause. |
| **Reached from** | An outfall that feeds the network (FIXED, TIDAL or TIMESERIES stage above the network's water level, or reverse flow through a regulator) and either (1) has its own lateral inflow ([INFLOWS], [DWF], [RDII], interface file), or (2) is in a run with SKIP_STEADY_STATE YES |
| **5.3.0** | `node_getSystemOutflow()` in [`src/legacy/engine/node.c:435`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L435)-[`444`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L444); skipped steps in `routing_execute()`, [`src/legacy/engine/routing.c:249`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L249) |
| **5.2.4** | Same code, [`src/solver/node.c:455`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L455)-[`464`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L464) |
| **6.0.0** | Reproduces: `updateRoutingMassBalance()` follows the legacy rule, including the skipped-step case it describes in a comment ([`src/engine/core/SWMMEngine.cpp:5657`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L5657)) |
| **Since** | The first commit of the repository (2014); since 5.1.013 the outfall's reverse flow is booked as external inflow in the cases the function does handle |
| **Fix** | Book the net flow, and undo the reporting write on skipped steps: [`NUM-19_swmm530.patch`](NUM-19_swmm530.patch), [`NUM-19_swmm600.patch`](NUM-19_swmm600.patch) |

## The problem

At the end of each routing step `removeOutflows()` asks `node_getSystemOutflow()` how much water leaves (or enters) the system at each outfall. It books a positive answer as outflow and a negative one, an outfall feeding the network, as external inflow. The function handles an outfall that only receives flow and one that only sends flow. When the outfall's inflow and outflow are both nonzero it returns 0, and neither is booked. Two ordinary situations produce that state:

1. **A lateral inflow at an outfall that feeds the network.** A pipe that discharges straight to the receiving water, a DWF or an RDII inflow assigned to the outfall node, or an interface file all give the outfall an inflow, while the stage drives flow back into the network.
2. **SKIP_STEADY_STATE.** For an outfall that only sends flow, the function also writes that flow into the outfall's `inflow`, so that reports show it. A skipped step does not re-route, so the next call sees the written inflow and the unchanged outflow: both nonzero, nothing booked. The step's flow error then exceeds SYS_FLOW_TOL, so the next step is routed, and the run alternates between routed and skipped steps. Half the water the outfall supplies is never booked.

In the tests, a FIXED outfall (stage 2 ft above its invert) supplies 13.7 cfs through two conduits to a free outfall for 12 h. Without skipping and without an inflow at the supplying outfall the continuity error is -0.058 %. With SKIP_STEADY_STATE the External Inflow is 6.830 ac-ft instead of 13.6 and the error is -99.2 %. With a 1 cfs inflow at the supplying outfall the External Inflow is just that inflow (0.992 ac-ft) and the error is -1,254.9 %. In a deck where a FIXED outfall fills a storage unit, a 1 cfs inflow at the outfall changes the error from +0.93 % to -64.8 %.

## Why it happens

```c
// src/legacy/engine/node.c, node_getSystemOutflow()
if ( Node[j].type == OUTFALL )
{
    // --- node receives inflow from outfall conduit
    if ( Node[j].outflow == 0.0 ) outflow = Node[j].inflow;

    // --- node sends flow into outfall conduit
    //     (therefore it has a negative outflow)
    else
    {
        if ( Node[j].inflow == 0.0 )
        {
            outflow = -Node[j].outflow;
            Node[j].inflow = fabs(outflow);   // reported as the outfall's inflow
        }
        // inflow and outflow both nonzero: outflow stays 0
    }
```

Case 1: `node_initFlows()` sets `Node.inflow = newLatFlow` at the start of each routed step ([`node.c:324`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L324)), so an outfall with a lateral inflow never has `inflow == 0`.

Case 2: `routing_execute()` only calls `routeFlow()`, which resets the node flows, on a step that is not skipped:

```c
// src/legacy/engine/routing.c, routing_execute()
inSteadyState = isInSteadyState(actionCount, stepFlowError);
if (inSteadyState == FALSE)
    trialsCount = routeFlow(routingModel, routingStep);
```

On a skipped step the outfall still has `inflow = outflow` from the line marked above. Quality routing on that step also sees the written inflow, as `findNodeQual()`'s flow.

6.0.0 copies the rule; its comment at the same place notes that a skipped step inherits the written inflow, and keeps the legacy result.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-19_skip-steady.inp`](NUM-19_skip-steady.inp) | FIXED outfall OHI (invert 100, stage 102) -> C1 -> J1 -> C2 -> FREE outfall OLO; SKIP_STEADY_STATE YES with SYS_FLOW_TOL 1 %; 12 h |
| [`NUM-19_outfall-inflow.inp`](NUM-19_outfall-inflow.inp) | The same network without skipping, plus a 1 cfs FLOW inflow at OHI |
| [`NUM-19_test.c`](NUM-19_test.c) | Runs both decks through the legacy toolkit (5.2.4, 5.3.0) and checks the flow continuity error from `swmm_getMassBalErr()`, printing the report's external inflow and outflow and the share of skipped steps |
| [`NUM-19_test6.c`](NUM-19_test6.c) | The same through the 6.0.0 C API (`swmm_get_routing_continuity_error()`) |

Water is conserved, so the test requires a continuity error within 1 %; the same network without skipping and without an inflow at OHI gives -0.058 %. The skip deck sets SYS_FLOW_TOL to 1 % because at the default 5 % skipping starts while the conduits are still filling, and the skipped steps then book the remaining imbalance (+3.6 % with the fix): that is the documented tolerance of SKIP_STEADY_STATE, not this bug.

```sh
tools/run-test.sh NUM-19            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-19 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0; 6.0.0 prints -99.157 % and -1254.982 %):

```
Deck                          Ext. in   Ext. out      Error   Steady
                              (ac-ft)    (ac-ft)        (%)      (%)
NUM-19_skip-steady.inp          6.830     13.593    -99.166    49.79
NUM-19_outfall-inflow.inp       0.992     13.593  -1254.911     0.00
FAIL: water supplied by the outfall is not booked: flow continuity error -99.166 % with SKIP_STEADY_STATE and -1254.911 % with an inflow at the outfall
```

**With the fix**, both engines print the same:

```
NUM-19_skip-steady.inp         13.669     13.632      0.084    99.66
NUM-19_outfall-inflow.inp      13.610     13.593     -0.058     0.00
PASS: the outfall's net flow is booked (continuity errors 0.084 % and -0.058 %)
NUM-19 5.3.0 patched: PASS
NUM-19 6.0.0 patched: PASS
```

With the fix the skipped steps no longer show a step flow error, so the run skips 99.7 % of its steps instead of alternating.

## The fix

Return the net flow, inflow minus outflow, which is what the two handled cases already return. Keep writing the inflow for reporting when the outfall only sends flow, but remember that it did (a new `Outfall.inflowIsBackflow` flag), and on a skipped step put that inflow back to 0 before quality routing and `removeOutflows()` read it:

```diff
     if ( Node[j].type == OUTFALL )
     {
+        Outfall[Node[j].subIndex].inflowIsBackflow = FALSE;
+
         // --- node receives inflow from outfall conduit
         if ( Node[j].outflow == 0.0 ) outflow = Node[j].inflow;
...
         else
         {
+            // --- net of any inflow the outfall also receives
+            outflow = Node[j].inflow - Node[j].outflow;
             if ( Node[j].inflow == 0.0 )
             {
-                outflow = -Node[j].outflow;
                 Node[j].inflow = fabs(outflow);
+                Outfall[Node[j].subIndex].inflowIsBackflow = TRUE;
             }
         }
```

```diff
         if (inSteadyState == FALSE)
             trialsCount = routeFlow(routingModel, routingStep);
+
+        // --- a skipped step reuses the last routed flows, so remove the
+        //     inflow that node_getSystemOutflow() gave a backflowing outfall
+        //     at the end of the previous step
+        else
+        {
+            for (j = 0; j < Nobjects[NODE]; j++)
+            {
+                if ( Node[j].type == OUTFALL &&
+                     Outfall[Node[j].subIndex].inflowIsBackflow )
+                    Node[j].inflow = 0.0;
+            }
+        }
```

The written inflow only ever replaces an exact 0, so putting back 0 restores the routed state exactly. Results change only for outfalls that both receive and send flow, and on skipped steps that follow one; the reported outfall inflow is unchanged. The 6.0.0 patch makes the same change in `updateRoutingMassBalance()`, with a per-node flag vector in `SWMMEngine` and `last_step_steady_` to recognise a skipped step; its FV branch is not touched.

With [BND-04](../../3-boundary/BND-04-outfall-backflow-mass-unbooked/) the pollutant mass of the net reverse flow is booked as well. With BND-04 and [BND-03](../../3-boundary/BND-03-outfall-backflow-concentration-ignored/) applied too, BND-03's deck with both a FLOW and a CONCEN inflow at the supplying outfall goes from continuity errors of -55,905 % (flow) and -137,250 % (quality) in unpatched 5.3.0 to -0.058 % and -0.054 % in both engines.

**Effect on other models.** The review deck in which a FIXED outfall with a 1 cfs inflow fills a storage unit goes from -64.80 % to +0.59 % (the same deck without the outfall inflow: +0.93 %, unchanged); its summary tables do not change. The regression decks with FIXED outfalls, `extran2` and `user2`, are unchanged in both engines, also when run with SKIP_STEADY_STATE YES (`extran2` then skips 22 % of its steps). No regression deck uses SKIP_STEADY_STATE.
