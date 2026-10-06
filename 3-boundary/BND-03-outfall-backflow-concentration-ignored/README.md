# BND-03: A CONCEN inflow at an outfall never sets the quality of its reverse flow

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | A documented boundary condition is ignored. A receiving-water concentration given as a CONCEN inflow at an outfall has no effect: the reverse flow enters the network with the outfall's last discharged concentration (0 mg/L in the test instead of 100). If the outfall also has a FLOW inflow, 5.2.4 and 5.3.0 instead multiply the concentration by reverse flow / FLOW inflow: 137,300 mg/L in the network for a 100 mg/L boundary. Nothing warns the user. |
| **Reached from** | `[INFLOWS]` CONCEN at an outfall that feeds water into the network (FIXED, TIDAL or TIMESERIES stage above the network's water level) |
| **5.3.0** | `addExternalInflows()` in [`src/legacy/engine/routing.c:577`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L577), defeated by `node_getSystemOutflow()` in [`src/legacy/engine/node.c:444`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L444) |
| **5.2.4** | Same code, [`src/solver/routing.c:474`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L474) |
| **6.0.0** | Reproduces for an outfall with only a CONCEN inflow: `InflowSolver::computeAll()` multiplies the concentration by the outfall's FLOW inflow and nothing else ([`src/engine/hydrology/Inflow.cpp:520`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Inflow.cpp#L520)). With a FLOW inflow as well, 6.0.0 gets the concentration right. |
| **Since** | The first commit of the repository (2014), so every release |
| **Fix** | Make the outfall hold the CONCEN value as its quality: [`BND-03_swmm530.patch`](BND-03_swmm530.patch), [`BND-03_swmm600.patch`](BND-03_swmm600.patch). Requires [BND-04](../BND-04-outfall-backflow-mass-unbooked/), which books the mass. |

## The problem

The input reference ([INFLOWS]) says: "If an external inflow of a pollutant concentration is specified for a node, then there must also be an external inflow of FLOW provided for the same node, unless the node is an Outfall. In that case a pollutant can enter the system during periods when the outfall is submerged and reverse flow occurs." This is how a user gives the quality of a receiving water that can flow back into a drainage system.

It does not work. In the test, a FIXED outfall OHI (stage 2 ft above its invert) supplies about 13.7 cfs through junction J1 to a free outfall, and has `OHI TSS "" CONCEN 1.0 1.0 100`:

- **CONCEN inflow only**: the network stays at 0 mg/L in 5.2.4, 5.3.0 and 6.0.0. The reverse flow carries the outfall's held concentration (here its initial 0) instead of 100 mg/L.
- **CONCEN plus a 0.01 cfs FLOW inflow at OHI**: 5.2.4 and 5.3.0 put 137,299.6 mg/L into the network. 6.0.0 gets 100 mg/L. All three report a flow continuity error of -55,905 % and a quality continuity error of -137,250 % (6.0.0: -137,219 %), from [NUM-19](../../1-numerical/NUM-19-outfall-two-way-flow-not-booked/) and [BND-04](../BND-04-outfall-backflow-mass-unbooked/).

## Why it happens

`addExternalInflows()` has code for exactly this case. It adds the reverse flow through an outfall to the flow that a CONCEN inflow is multiplied by, using the outfall's net inflow from the previous step:

```c
// src/legacy/engine/routing.c, addExternalInflows()
// --- add on any inflow (i.e., reverse flow) through an outfall
if ( Node[j].type == OUTFALL && Node[j].oldNetInflow < 0.0 )
{
    q = q - Node[j].oldNetInflow;
}
...
if ( inflow->type == CONCEN_INFLOW ) w *= q;
Node[j].newQual[p] += w;
```

But at the end of every step, `node_getSystemOutflow()` overwrites the inflow of an outfall that only sends flow into the network:

```c
// src/legacy/engine/node.c, node_getSystemOutflow()
if ( Node[j].inflow == 0.0 )
{
    outflow = -Node[j].outflow;
    Node[j].inflow = fabs(outflow);     // inflow := outflow
}
```

so `oldNetInflow = inflow - outflow` is 0 and the branch never runs: `w = c * 0`. Even if it ran, `findNodeQual()` would divide the mass by the outfall's inflow, which is 0, and keep the held concentration.

With a FLOW inflow `q` at the outfall, the inflow is not overwritten and `oldNetInflow = q - b` for a reverse flow `b`. The branch then sets the flow to `q - (q - b) = b`, so the mass is `c b`, while `findNodeQual()` mixes it into the outfall's actual inflow `q`: the outfall's concentration becomes `c b / q` = 100 x 13.73 / 0.01 = 137,300 mg/L, and the reverse flow carries that into the network.

6.0.0 has no reverse-flow term at all. A CONCEN row is multiplied by the outfall's FLOW inflow, so it does nothing without one and is right with one.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-03_concen-only.inp`](BND-03_concen-only.inp) | FIXED outfall OHI (invert 100, stage 102) -> C1 -> J1 -> C2 -> FREE outfall OLO; `OHI TSS CONCEN 100`; 12 h |
| [`BND-03_concen-and-flow.inp`](BND-03_concen-and-flow.inp) | The same plus a 0.01 cfs FLOW inflow at OHI |
| [`BND-03_test.c`](BND-03_test.c) | Runs both decks through the legacy toolkit (5.2.4, 5.3.0) and reads TSS at J1 and OLO from the output file at the last period |
| [`BND-03_test6.c`](BND-03_test6.c) | The same through the 6.0.0 C API (`swmm_node_get_quality()`) |

All the water reaching J1 comes from OHI, so after 12 h of steady flow J1 and OLO must be at 100 mg/L (tolerance 1 mg/L).

```sh
tools/run-test.sh BND-03            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-03 --patched  # 5.3.0 and 6.0.0 with BND-04 and this fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0:

```
Deck                               J1 TSS      OLO TSS   expected
                                   (mg/L)       (mg/L)     (mg/L)
BND-03_concen-only.inp              0.000        0.000      100.0
BND-03_concen-and-flow.inp     137299.578   137299.578      100.0
FAIL: the reverse flow from the outfall does not carry its 100 mg/L CONCEN inflow: J1 is at 0.000 mg/L (CONCEN only) and 137299.578 mg/L (CONCEN + FLOW)
```

6.0.0:

```
BND-03_concen-only.inp              0.000        0.000      100.0
BND-03_concen-and-flow.inp        100.000      100.000      100.0
FAIL: the reverse flow from the outfall does not carry its 100 mg/L CONCEN inflow: J1 is at 0.000 mg/L (CONCEN only) and 100.000 mg/L (CONCEN + FLOW)
```

**With the fix**, both engines:

```
BND-03_concen-only.inp            100.000      100.000      100.0
BND-03_concen-and-flow.inp        100.000      100.000      100.0
PASS: the reverse flow from the outfall carries the outfall's CONCEN inflow (100 mg/L)
BND-03 5.3.0 patched: PASS
BND-03 6.0.0 patched: PASS
```

The quality continuity error of the CONCEN-only deck goes from 0.000 % (no mass entered at all) to -0.054 %, against a flow error of -0.058 %. The deck with a FLOW inflow at the outfall still reports -55,905 % (flow) and -137,212 % (quality, 5.3.0) because the outfall both receives and sends flow. With the [NUM-19](../../1-numerical/NUM-19-outfall-two-way-flow-not-booked/) patch as well, that deck gives -0.058 % and -0.054 % in both engines.

## The fix

Remove the reverse-flow term, so a CONCEN inflow at an outfall is multiplied by its FLOW inflow as at any other node, and make the outfall hold the CONCEN value as its quality:

```diff
-        // --- add on any inflow (i.e., reverse flow) through an outfall
-        if ( Node[j].type == OUTFALL && Node[j].oldNetInflow < 0.0 )
-        {
-            q = q - Node[j].oldNetInflow;
-        }
-
...
-                if ( inflow->type == CONCEN_INFLOW ) w *= q;
+                if ( inflow->type == CONCEN_INFLOW )
+                {
+                    // --- an outfall holds this concentration, so that any
+                    //     reverse flow into the network carries it
+                    if ( Node[j].type == OUTFALL ) Node[j].oldQual[p] = w;
+                    w *= q;
+                }
```

An outfall that supplies the network receives no flow, so `findNodeQual()` keeps its held quality, which is now the CONCEN value, and the reversed link carries it. An outfall that receives flow mixes its inflows as before; the held value is not used. With a FLOW inflow at the outfall, the CONCEN mass is `c q` mixed into `q`, which gives `c`.

The mass the reverse flow carries in is booked by the [BND-04](../BND-04-outfall-backflow-mass-unbooked/) patch, at the outfall's concentration, so this patch requires it. Without BND-04 the concentrations are right but the CONCEN-only deck's quality continuity error is -100 %.

The 6.0.0 patch sets the same held value (`conc_old`, which has already been rolled for the step when the inflows are computed) in `InflowSolver::computeAll()`.

With 5.3.0's `OUTFALL_BACKFLOW_QUALITY ZERO` option, `findNodeQual()` still sets a supplying outfall to 0 before it looks at the held value, so that option overrides a CONCEN inflow at the outfall; the patch leaves that order alone.

**Effect on other models.** None of the regression decks has a CONCEN inflow at an outfall, so none changes.
