# API-08: A pollutant mass flux set through the API delivers 28.3 times the requested mass

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | A load of 10 mg/s set on a node or a link through the toolkit enters the network as 283 mg/s. Concentrations, loads and the quality continuity ledger are all 28.317 times too large: 10 mg/L instead of 0.353 mg/L in a 1 cfs flow, 4.491 lb of External Inflow instead of 0.159 lb over 2 h. The same load entered as an `[INFLOWS]` MASS inflow is correct, and nothing warns. |
| **Reached from** | 5.3.0: `swmm_setValueExpanded` with `swmm_NODE_POLLUTANT_LATMASS_FLUX` or `swmm_LINK_POLLUTANT_LATMASS_FLUX` (Python: `set_pollutant_lateral_mass_flux`). 6.0.0: `swmm_node_set_quality_mass_flux()` (Python: `set_quality_mass_flux`) |
| **5.3.0** | Running branches of `setNodeValue()` and `setLinkValue()`, [`src/legacy/engine/swmm5.c:2276`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2276) and [`:2390`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2390); used in [`routing.c:600`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L600) and `findLinkQual()`; compare [`inflow.c:129`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inflow.c#L129) |
| **5.2.4** | Not affected: no pollutant mass flux API |
| **6.0.0** | Reproduces: `swmm_node_set_quality_mass_flux()` in [`src/engine/core/openswmm_nodes_impl.cpp:400`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_nodes_impl.cpp#L400) stores the rate unconverted, while its `[INFLOWS]` parser converts ([`Inflow.cpp:273`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Inflow.cpp#L273)) |
| **Since** | 5.3.0, fork commit 5b87a2b5 ("WIP API bindings for pollutants", December 2024); 6.0.0's setter from c982efdc (April 2026) |
| **Fix** | Divide by `LperFT3` when the value is stored (and multiply in the 5.3.0 getters): [`API-08_swmm530.patch`](API-08_swmm530.patch), [`API-08_swmm600.patch`](API-08_swmm600.patch) |

## The problem

The API mass flux is documented as a mass rate: "pollutant mass flux ... (mass/sec)" in 5.3.0's `objects.h`, "Mass flux in mass/sec (project mass units)" in 6.0.0's `openswmm_nodes.h`, and "Mass flux in project mass/time units" in the Python wrapper. The input manual defines the project's mass units for `[INFLOWS]` MASS as "those specified for the pollutant in the [POLLUTANTS] section", i.e. mg/s for a pollutant in MG/L.

In the test deck, 1 cfs of clean dry-weather flow passes J1 -> C1 -> O1. A source of 10 mg/s of P1 must give 10 / (1 x 28.317) = 0.3531 mg/L downstream:

| Source of 10 mg/s | C1 concentration, 5.3.0 | 6.0.0 | Expected |
|---|---|---|---|
| `[INFLOWS] J1 P1 "" MASS 1.0 1.0 10` | 0.3531 | 0.3531 | 0.3531 mg/L |
| API node flux on J1 | 10.0000 | 10.0000 | 0.3531 mg/L |
| API link flux on C1 (5.3.0 only) | 10.0000 | - | 0.3531 mg/L |

The quality continuity of the node run books the same excess: External Inflow 4.491 lb in 5.3.0 (4.492 lb in 6.0.0) instead of 0.159 lb (72,000 mg over 2 h). A user who checks the ledger against the load they asked for sees a factor of 28.25, which is the litres in a cubic foot.

## Why it happens

Internally SWMM carries a pollutant mass rate as concentration x flow, i.e. (mg/L) x (ft3/s), and converts with `LperFT3` (28.317 L/ft3) only when it reports a mass. A mass inflow read from the input file is therefore divided by `LperFT3` when it is loaded:

```c
// src/legacy/engine/inflow.c, inflow_readExtInflow()
if ( type == MASS_INFLOW ) cf /= LperFT3;
```

The API setters store the user's mass rate as it comes:

```c
// src/legacy/engine/swmm5.c, setNodeValue() (running branch)
Node[index].apiExtQualMassFlux[pollutantIndex] = value;
// src/legacy/engine/swmm5.c, setLinkValue() (running branch)
link->apiExtQualMassFlux[pollutantIndex] = value;
```

and the routing step adds it to the node's inflow mass rate, in the internal unit, with no conversion:

```c
// src/legacy/engine/routing.c, addExternalInflows()
w = Node[j].apiExtQualMassFlux[p];
if (w > 0.0)
{
    Node[j].newQual[p] += w;
    massbal_addInflowQual(EXTERNAL_INFLOW, p, w);
}
```

The link flux is turned into a concentration increment `flux * tStep / (v1 + qIn * tStep)` in `findLinkQual()`, again in the internal unit. 6.0.0 follows the same path: `swmm_node_set_quality_mass_flux()` stores `mass_rate`, and `QualitySolver::addExtInflowLoads()` adds it to `qual_mass_in` like legacy's `addExternalInflows()`, while 6.0.0's own `[INFLOWS]` parser divides MASS inflows by `L_PER_FT3`.

## How to reproduce

| File | What it is |
|---|---|
| [`API-08_inflows-mass.inp`](API-08_inflows-mass.inp) | J1 -> C1 -> O1, 1 cfs DWF at J1, P1 (MG/L) with a 10 mg/s `[INFLOWS]` MASS baseline at J1; DYNWAVE, 2 h |
| [`API-08_api-flux.inp`](API-08_api-flux.inp) | The same without the `[INFLOWS]` line |
| [`API-08_test.c`](API-08_test.c) | Runs the first deck as a reference, then the second with a flux of 10 set on J1 (node property) and on C1 (link property) right after `swmm_start`; reads C1's concentration at the end and the flux back through the getter. Expected 0.3531 mg/L within 3% and a read-back of 10. 5.2.4 has no such API and prints PASS |
| [`API-08_test6.c`](API-08_test6.c) | The same for 6.0.0 with `swmm_node_set_quality_mass_flux()` (no link flux in 6.0.0) |

```sh
tools/run-test.sh API-08            # 5.3.0 and 6.0.0: FAIL; 5.2.4: PASS
tools/run-test.sh API-08 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0:

```
10 mg/s of P1 into 1 cfs; expected C1 concentration 10 / 28.3168 = 0.3531 mg/L
source                                        read back   C1 conc (mg/L)
[INFLOWS] J1 P1 MASS baseline 10                      -           0.3531
API NODE_POLLUTANT_LATMASS_FLUX(J1) = 10        10.0000          10.0000
API LINK_POLLUTANT_LATMASS_FLUX(C1) = 10        10.0000          10.0000
FAIL: 10 mg/s gives 10.0000 mg/L through the node API and 10.0000 mg/L through the link API, expected 0.3531 mg/L ([INFLOWS] MASS gives 0.3531)
API-08 5.3.0 base: FAIL
```

6.0.0:

```
source                                           C1 conc (mg/L)
[INFLOWS] J1 P1 MASS baseline 10                         0.3531
swmm_node_set_quality_mass_flux(J1, P1, 10)             10.0000
FAIL: 10 mg/s gives 10.0000 mg/L through the API, expected 0.3531 mg/L ([INFLOWS] MASS gives 0.3531)
API-08 6.0.0 base: FAIL
```

**With the fix**, both engines give the `[INFLOWS]` result, and the External Inflow of the API runs is 0.159 lb, as in the reference run:

```
API NODE_POLLUTANT_LATMASS_FLUX(J1) = 10        10.0000           0.3531
API LINK_POLLUTANT_LATMASS_FLUX(C1) = 10        10.0000           0.3531
PASS: an API mass flux of 10 mg/s gives 0.3531 mg/L in 1 cfs, as [INFLOWS] MASS does, and reads back as 10
API-08 5.3.0 patched: PASS
...
swmm_node_set_quality_mass_flux(J1, P1, 10)              0.3531
PASS: an API mass flux of 10 mg/s gives 0.3531 mg/L in 1 cfs, as [INFLOWS] MASS does
API-08 6.0.0 patched: PASS
```

## The fix

Convert at the API boundary with the constant the input path uses, in both directions in 5.3.0:

```diff
-            Node[index].apiExtQualMassFlux[pollutantIndex] = value;
+            // --- mass/sec to internal (concen. x cfs), as for [INFLOWS] MASS
+            Node[index].apiExtQualMassFlux[pollutantIndex] = value / LperFT3;
 ...
-            link->apiExtQualMassFlux[pollutantIndex] = value;
+            // --- mass/sec to internal (concen. x cfs), as for [INFLOWS] MASS
+            link->apiExtQualMassFlux[pollutantIndex] = value / LperFT3;
 ...
-            return node->apiExtQualMassFlux[pollutantIndex];
+            return node->apiExtQualMassFlux[pollutantIndex] * LperFT3;
 ...
-        return link->apiExtQualMassFlux[pollutantIndex];
+        return link->apiExtQualMassFlux[pollutantIndex] * LperFT3;
```

and in 6.0.0:

```diff
-    ctx.nodes.user_conc_mass_flux[flat] = mass_rate;
+    // mass/sec to internal (concen. x cfs), as Inflow.cpp does for [INFLOWS] MASS
+    constexpr double L_PER_FT3 = 28.317;   // legacy consts.h LperFT3
+    ctx.nodes.user_conc_mass_flux[flat] = mass_rate / L_PER_FT3;
```

Both engines use 28.317, so the patched engines inject the same mass. The pre-start branches of the 5.3.0 setters are not touched: their values are discarded by `swmm_start`, and [CRASH-13](../../5-crashes/CRASH-13-pollutant-flux-before-start/) makes them return an error. 6.0.0's `swmm_forcing_node_quality()` is documented in "concentration units x CFS" and is left as it is.

Clients that compensated by dividing their loads by 28.317 must stop doing so. Input-file runs are unchanged. 6.0.0's unit test `ArdTransportTest.ForcedQualityMassFluxRoutesUnderBothEngines` (`tests/unit/engine/test_ard_transport.cpp`) sets a flux of 12.5 on a 1 cfs, 12.5 mg/L inflow and expects the link concentration to rise by more than 50%; it encodes the old unit, and the 6.0.0 patch multiplies its flux by 28.317 so it passes again.
