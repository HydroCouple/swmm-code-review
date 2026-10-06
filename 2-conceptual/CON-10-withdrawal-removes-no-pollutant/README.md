# CON-10: A lateral withdrawal removes water from a node but no pollutant, so the node's concentration is inflated

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | A node that receives a withdrawal (negative lateral inflow) together with other lateral inflow reports concentrations above every source's concentration: 200 mg/L downstream of a 10 cfs, 100 mg/L inflow less a 5 cfs withdrawal. Loads leaving the outfall are doubled. The quality continuity table does not flag it, because the mass is conserved. Under KINWAVE, a node whose net lateral flow is negative also creates mass (continuity error -50 % in the test). |
| **Reached from** | Negative `[INFLOWS]` baselines or time series, negative `swmm_NODE_LATFLOW` from the API, two-way groundwater exchange (aquifer-ward flow at a node that also gets runoff), negative DWF, RDII or interface-file flows, in any model with pollutants |
| **5.3.0** | Withdrawals are subtracted from the lateral flow in `addExternalInflows()` [`src/legacy/engine/routing.c:572`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L572) (and the other `add...Inflows()` functions) without taking mass; `qualrout_execute()` mixes with that net flow, [`qualrout.c:119`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L119); `removeOutflows()` books mass only for a net negative flow, [`routing.c:1030`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L1030) |
| **5.2.4** | Same code: [`routing.c:469`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L469), [`qualrout.c:118`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/qualrout.c#L118), [`routing.c:915`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L915) |
| **6.0.0** | Reproduces on purpose: a parity block in `QualitySolver::assembleExternalLoads()` cuts the mixing volume back to legacy's net lateral flow, [`QualityRouting.cpp:276`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L276); its comment calls the result "legitimate". The KINWAVE net-withdrawal case does not create mass in 6.0.0, but still gives 110 mg/L there |
| **Since** | Every 5.x release |
| **Fix** | Mix inflow mass with all the water that carried it and take each withdrawal's mass at the node's concentration: [`CON-10_swmm530.patch`](CON-10_swmm530.patch), [`CON-10_swmm600.patch`](CON-10_swmm600.patch) |

## The problem

Junction J1 receives a 10 cfs dry weather flow at 100 mg/L TSS and an `[INFLOWS]` baseline of -5 cfs, a withdrawal. Every source is at 100 mg/L and nothing reacts, so whatever leaves J1 must be at 100 mg/L. All three versions report 200 mg/L at J1 and at the outfall, and the outfall load is 897 lb instead of 448 lb. Without the withdrawal the result is 100 mg/L.

The same happens for any negative lateral component at a node that has other lateral inflow. The common case in practice is two-way groundwater exchange: the aquifer draws water out of a node that also receives runoff, and the node's concentration rises above the runoff's. 6.0.0's own comment on its `usgs-runoff` deck describes it: nodes whose only source is 18 mg/L rain "climb to 47 mg/L" in legacy, and 6.0.0 was changed to match.

When the withdrawal is larger than the other lateral inflow (the node's net lateral flow is negative), KINWAVE also creates mass. In the third test deck, J1 gets 10 cfs at 100 mg/L through a conduit plus 1 cfs of its own DWF, and from 1:00 a 5 cfs withdrawal: 5.2.4 and 5.3.0 report 183 mg/L, 1,476 lb leaving for 988 lb entering, and a quality continuity error of -50 %. DYNWAVE does not create mass in that case but still reports 110 mg/L, as does 6.0.0 under either routing method.

## Why it happens

Each `add...Inflows()` function adds its flow, of either sign, to the node's lateral flow, and only positive flows bring mass:

```c
// src/legacy/engine/routing.c, addExternalInflows()
Node[j].newLatFlow += q;
if (q >= 0.0)
    massbal_addInflowFlow(EXTERNAL_INFLOW, q);
else
{
    massbal_addOutflowFlow(-q, FALSE);
    continue;                              // no mass leaves with it
}
```

Quality routing then divides the mass of the positive sources by the node's hydraulic inflow, which contains only the net lateral flow:

```c
// src/legacy/engine/qualrout.c, qualrout_execute() and findNodeQual()
Node[j].qualInflow = Node[j].inflow;       // links + NET lateral flow
...
cIn = Node[j].newQual[p] / qNode;          // mass of all positive sources / net flow
```

With 10 cfs in and 5 cfs withdrawn this is 1,000 / 5 = 200 mg/L, and the 5 cfs that leave through the conduit carry the whole 1,000 mass units. The withdrawn water carries none. Only when the net lateral flow is negative does `removeOutflows()` book mass leaving with it:

```c
// src/legacy/engine/routing.c, removeOutflows()
q = Node[i].newLatFlow;
if ( q < 0.0 )
    ... w = -q * Node[i].newQual[p]; massbal_addOutflowQual(p, w, FALSE);
```

Under KINWAVE `Node.inflow` includes a negative net lateral flow (links − withdrawal), so the concentration is inflated and the withdrawal is then also booked at that inflated concentration, which creates mass. DYNWAVE moves a negative net lateral flow to `Node.outflow`, so the link inflow alone dilutes the mass and any positive lateral water at the node is ignored.

6.0.0's loaders add each positive source's own water to the mixing volume, which is right, but the parity block then replaces that lateral volume with `max(0, net lateral flow)` whenever a negative component arrived, and the mass leaving with a withdrawal is booked only for a net negative lateral flow (`SWMMEngine.cpp:5735`).

## How to reproduce

| File | What it is |
|---|---|
| [`CON-10_withdrawal-dw.inp`](CON-10_withdrawal-dw.inp) | J1 (10 cfs DWF at 100 mg/L, -5 cfs `[INFLOWS]` baseline) - C1 - O1, DYNWAVE |
| [`CON-10_withdrawal-kw.inp`](CON-10_withdrawal-kw.inp) | The same with KINWAVE |
| [`CON-10_net-withdrawal-kw.inp`](CON-10_net-withdrawal-kw.inp) | J0 (10 cfs at 100 mg/L) - C0 - J1 (1 cfs DWF at 100 mg/L, -5 cfs from 1:00) - C1 - O1, KINWAVE |
| [`CON-10_test.c`](CON-10_test.c) | Legacy toolkit (5.2.4 and 5.3.0): runs each deck, reads TSS at J1 and O1 in the last period from the `.out` file and the quality continuity error |
| [`CON-10_test6.c`](CON-10_test6.c) | The same for 6.0.0 |

The test requires both concentrations within 1 mg/L of 100 mg/L and a quality continuity error below 1 %.

```sh
tools/run-test.sh CON-10            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-10 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**:

```
---- 5.3.0 (5.2.4 prints the same, with -50.186 % in the last row) ----
Deck                            Routing     J1 TSS   O1 TSS  Quality continuity
                                            (mg/L)   (mg/L)  error (%)
CON-10_withdrawal-dw.inp        DYNWAVE    200.00   200.00              -0.154  <-- wrong
CON-10_withdrawal-kw.inp        KINWAVE    200.00   200.00              -0.093  <-- wrong
CON-10_net-withdrawal-kw.inp    KINWAVE    183.33   183.33             -50.187  <-- wrong
FAIL: in 3 of 3 decks the withdrawal leaves its pollutant behind (concentration above every source's 100 mg/L, or no mass balance)

---- 6.0.0 ----
CON-10_withdrawal-dw.inp        DYNWAVE    200.00   200.00              -0.187  <-- wrong
CON-10_withdrawal-kw.inp        KINWAVE    200.00   200.00              -0.094  <-- wrong
CON-10_net-withdrawal-kw.inp    KINWAVE    110.00   110.00              -0.160  <-- wrong
FAIL: in 3 of 3 decks the withdrawal leaves its pollutant behind (concentration above every source's 100 mg/L, or no mass balance)
```

**With the fix**:

```
---- 5.3.0 ----
CON-10_withdrawal-dw.inp        DYNWAVE    100.00   100.00              -0.077
CON-10_withdrawal-kw.inp        KINWAVE    100.00   100.00              -0.046
CON-10_net-withdrawal-kw.inp    KINWAVE    100.00   100.00              -0.164
PASS: withdrawals leave at the node's concentration; all nodes stay at 100 mg/L

---- 6.0.0 ----
CON-10_withdrawal-dw.inp        DYNWAVE    100.00   100.00              -0.094
CON-10_withdrawal-kw.inp        KINWAVE    100.00   100.00              -0.047
CON-10_net-withdrawal-kw.inp    KINWAVE    100.00   100.00              -0.165
PASS: withdrawals leave at the node's concentration; all nodes stay at 100 mg/L
```

In the reports of the patched engines the outfall load of the first deck is 448.438 lb in both (896.876 lb unpatched); the rest of the 898 lb is booked as external outflow with the withdrawal. The small differences between the engines' continuity errors are present before the patch as well (6.0.0's quality API and its DWF inflow total differ slightly from legacy's).

## The fix

5.3.0: record the withdrawals at each node (a new `TNode.latWithdrawal`, reset with the lateral flow every step and incremented wherever a negative lateral component is added), add them back to the flow the inflow mass is mixed with, and book the mass leaving with them at the node's new concentration:

```diff
         Node[j].qualInflow = Node[j].inflow;
+
+        // --- mix the inflow mass with all the lateral water that carried it
+        //     (withdrawals leave at the mixed concen. in removeOutflows;
+        //     dynwave leaves a negative net lateral flow out of Node.inflow)
+        Node[j].qualInflow += Node[j].latWithdrawal;
+        if ( RouteModel == DW ) Node[j].qualInflow += MIN(0.0, Node[j].newLatFlow);
```

```diff
-        q = Node[i].newLatFlow;
-        if ( q < 0.0 )
+        q = Node[i].latWithdrawal;
+        if ( q > 0.0 )
         {
             for ( p = 0; p < Nobjects[POLLUT]; p++ )
             {
-                w = -q * Node[i].newQual[p];
+                w = q * Node[i].newQual[p];
                 massbal_addOutflowQual(p, w, FALSE);
```

The mixing flow is then the link inflow plus the positive lateral inflows, for every routing method. Where a node has no positive lateral inflow the result is unchanged under DYNWAVE; under KINWAVE it no longer creates mass.

6.0.0: remove the parity block (and the per-node lateral volume it used), so the mixing volume stays the sum of the sources' own water, and book the withdrawn mass from the per-node withdrawal volume the loaders already track, through a new `QualitySolver::lateralWithdrawal()`, instead of from a net negative lateral flow. With both patches the two engines give the same concentrations and outfall loads (448.438 lb in the first deck).

**Effect on other models.** Only models with pollutants and a negative lateral inflow change. The two regression decks with pollutants (Example1, events_example) have none, and their `.out` files from the patched 5.3.0 and 6.0.0 are byte-identical to the unpatched ones. A DYNWAVE variant of the third deck (net withdrawal plus a positive DWF at J1) goes from 110 mg/L to 100 mg/L in both engines.
