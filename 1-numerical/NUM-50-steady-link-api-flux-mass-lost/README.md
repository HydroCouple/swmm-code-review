# NUM-50: A link API mass flux under STEADY routing is diluted by the conduit volume, and most of it disappears

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Under STEADY routing, a pollutant mass flux added to a conduit through the 5.3.0 API raises the conduit's concentration by only a fraction q·Δt / (V + q·Δt) of what it should, while the full flux is booked as External Inflow. In the test 37.5% of the booked mass reaches the outfall, and the quality continuity error is 62.5%. The continuity error is the only sign. |
| **Reached from** | `FLOW_ROUTING STEADY` + `swmm_setValueExpanded(swmm_LINK, swmm_LINK_POLLUTANT_LATMASS_FLUX, ...)` |
| **5.3.0** | `findSFLinkQual()` in [`src/legacy/engine/qualrout.c:455`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L455) (removal) and [`:467`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L467) (addition), booked at [`:474`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L474) |
| **5.2.4** | Not affected: there is no link pollutant flux API ([`findSFLinkQual()`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/qualrout.c#L357)) |
| **6.0.0** | Not applicable: no link pollutant flux API. The node flux (`swmm_node_set_quality_mass_flux`) on the same deck is conserved (-0.54% continuity error) |
| **Since** | 5.3.0, fork commit 5b87a2b5 (10 Dec 2024, "WIP API bindings for pollutants #204, #162, #163, #184, #180") |
| **Fix** | Apply the flux as flux / flow: [`NUM-50_swmm530.patch`](NUM-50_swmm530.patch) (requires [CRASH-12](../../5-crashes/CRASH-12-steady-link-quality-node-index/README.md)) Apply after NUM-01, NUM-45, NUM-48 (their `Requires:` lines). |

## The problem

Steady Flow routing has no storage in a conduit. Volume III, eq. 5-9: "the inflow to the conduit completely replaces the previous contents over the time step. So there is no mixing of the previous contents with new inflow from the upstream node", and the conduit's concentration is c_L = f_evap · c_N · exp(-K₁Δt). Whatever concentration the conduit has, it passes on as c_L · q per second. A mass flux F added to the conduit should therefore raise c_L by F / q, so that exactly F per second leaves.

The 5.3.0 code uses the dynamic-wave mixing formula instead, which spreads the flux over the conduit's stored volume V plus the step's inflow volume:

    Δc = F · Δt / (V + q · Δt)

and books the full F as External Inflow. Only the part q · Δt / (V + q · Δt) leaves the conduit; the rest is neither carried on nor counted as stored. For the 400 ft, 1.5 ft pipe in the test (V ≈ 100 ft³ at 1 cfs) and a 60 s step that part is 37.5%. With a longer pipe or a shorter routing step the loss is larger.

## Why it happens

```c
// src/legacy/engine/qualrout.c, findSFLinkQual()
        qIn = fabs(Conduit[k].q1) * barrels;     // under STEADY: the conduit's outflow
        v1 = Link[i].oldVolume;

        cOut = Link[j].apiExtQualMassFlux[p];    // (index fixed by CRASH-12)
        ...
        else
        {
            cOut = cOut * tStep / (v1 + qIn * tStep);   // dc diluted by v1
            c2 += cOut;

            cOut = cOut * (v1 + qIn * tStep);           // = F * dt, booked in full
            Link[j].totalLoad[p] += cOut;

            cOut = cOut/ tStep;
            massbal_addInflowQual(EXTERNAL_INFLOW, p, cOut);
        }
```

The block is a copy of the one in `findLinkQual()`, where `v1` is the volume the link mixes over. Under STEADY routing the next step's `findLinkMassFlow()` moves `qLink * Link[i].oldQual[p]` ([`qualrout.c:216`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L216)) to the downstream node, and the conduit's concentration is reset from the upstream node at every step, so the mass spread over `v1` is lost. The removal branch (negative flux) has the same mismatch.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-50_steady-conduit.inp`](NUM-50_steady-conduit.inp) | J1 -> C1 -> O1 under STEADY routing, 1 cfs of clean water for 2 h, 60 s steps. J1 is node 0 and C1 link 0, so CRASH-12 has no effect |
| [`NUM-50_test.c`](NUM-50_test.c) | Sets a flux of 10 on C1 after the first step, then compares External Inflow and External Outflow in the report's quality ledger. 5.3.0 only (`#ifdef`); on 5.2.4 it prints PASS because the API does not exist |
| [`NUM-50_test6.c`](NUM-50_test6.c) | 6.0.0: the same deck with the flux set on node J1, the only API flux 6.0.0 has |

The check is mass conservation and uses only the report's own ledger, so it does not depend on the unit the flux is given in ([API-08](../../6-api/API-08-mass-flux-litres-conversion/README.md)).

```sh
tools/run-test.sh NUM-50            # 5.2.4 PASS, 5.3.0 FAIL, 6.0.0 PASS
tools/run-test.sh NUM-50 --patched  # 5.3.0 (with CRASH-12) and 6.0.0: PASS
```

**Without the fix:**

```
Final step: C1 flow 1.0000 cfs, C1 conc 3.78186, O1 conc 3.78186 (mg/L)
Quality ledger: External Inflow 4.435 lb, External Outflow 1.663 lb, delivered 37.5 %, continuity error 62.501 %
FAIL: only 37.5 % of the API mass flux booked on C1 leaves the conduit (continuity error 62.501 %)
NUM-50 5.3.0 base: FAIL
```

6.0.0, node flux on J1:

```
Final step: C1 conc 10.00000, O1 conc 10.00000 (mg/L), error code 0, quality continuity error -0.541 %
PASS: an API mass flux under STEADY routing leaves through the outfall (continuity error -0.541 %)
NUM-50 6.0.0 base: PASS
```

**With the fix**, C1 carries F / q = 10 / 1 (in the internal unit of API-08) and the booked mass leaves:

```
Final step: C1 flow 1.0000 cfs, C1 conc 10.00000, O1 conc 10.00000 (mg/L)
Quality ledger: External Inflow 4.435 lb, External Outflow 4.398 lb, delivered 99.2 %, continuity error 0.844 %
PASS: the mass flux added to C1 under STEADY routing leaves the conduit (continuity error 0.844 %)
NUM-50 5.3.0 patched: PASS
```

The remaining 0.84% is one step in 119: the mass added in the last step reaches the outfall one step later, after the run has ended, and the steady-flow ledger does not count it as stored.

## The fix

```diff
-        v1 = Link[i].oldVolume;
 
+        // --- under Steady Flow a conduit stores no pollutant: it passes on
+        //     c2 * qIn, so the flux changes c2 by flux / qIn (no flow, no flux)
         cOut = Link[i].apiExtQualMassFlux[p];
 
-        if (cOut < 0.0)
+        if (cOut < 0.0 && qIn > ZERO)
         {
-            cOut = -cOut * tStep / (v1 + qIn * tStep);
+            cOut = -cOut / qIn;
 ...
-            cOut = cOut * (v1 + qIn * tStep) ;
+            cOut = cOut * qIn * tStep;
 ...
-        else
+        else if (cOut > 0.0 && qIn > ZERO)
         {
-            cOut = cOut * tStep / (v1 + qIn * tStep);
+            cOut = cOut / qIn;
```

The booked mass is now exactly what the concentration change carries. When no water flows through the conduit the flux is neither applied nor booked. A removal is still capped at the conduit's concentration. The patch is written on top of CRASH-12's (`Requires: CRASH-12`), which changes the same lines from `Link[j]` to `Link[i]`. Models that do not call the link flux API are not affected; the regression suite has no deck that does.
