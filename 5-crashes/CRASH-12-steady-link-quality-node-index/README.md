# CRASH-12: Steady-flow link quality indexes the Link array with a node index

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Under STEADY routing with any pollutant, 5.3.0 reads `Link[j]` with `j` = the conduit's upstream **node** index at every routing step, API or not. When a node index is >= the number of links, it reads past the end of the `Link` array and writes through the pointer it finds there (heap corruption or a segfault in a release build; a sanitizer build stops at the first routing step). When the index is in range, a mass flux set on one conduit through the API is applied to a different conduit, and the first conduit's concentration stays 0. No warning. |
| **Reached from** | `FLOW_ROUTING STEADY` + `[POLLUTANTS]`, with a node listed so that its index is >= the number of links (for example `[OUTFALLS]` before `[JUNCTIONS]`); the wrong-link part needs `swmm_setValueExpanded(swmm_LINK, swmm_LINK_POLLUTANT_LATMASS_FLUX, ...)` |
| **5.3.0** | `findSFLinkQual()` in [`src/legacy/engine/qualrout.c:417`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L417) and [`:451`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L451), [`:460`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L460), [`:471`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L471) |
| **5.2.4** | Not affected: its [`findSFLinkQual()`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/qualrout.c#L357) has no API flux block |
| **6.0.0** | Not applicable: there is no link pollutant flux API, and the steady-flow branch of [`QualityRouting.cpp:1346`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1346) reads only the conduit's own upstream node |
| **Since** | 5.3.0, fork commit 5b87a2b5 (10 Dec 2024, "WIP API bindings for pollutants #204, #162, #163, #184, #180") |
| **Fix** | Use `Link[i]`, the conduit being processed: [`CRASH-12_swmm530.patch`](CRASH-12_swmm530.patch) Apply after NUM-01 (its `Requires:` line). |

## The problem

5.3.0 lets a toolkit client add a pollutant mass flux to a conduit (`swmm_LINK_POLLUTANT_LATMASS_FLUX`). For STEADY routing the flux is applied in `findSFLinkQual()`, which runs for every conduit at every routing step of any model with a pollutant. The block reads the flux and books the load on the wrong array element, so two things go wrong:

- **Out-of-bounds read and write.** With the outfall listed before the junctions, the two-conduit test deck has nodes O1 = 0, J1 = 1, J2 = 2 and links C1 = 0, C2 = 1. For C2 the code reads `Link[2]`, one element past the end of the `Link` array, takes the `apiExtQualMassFlux` pointer it finds there, reads through it and then writes `totalLoad[p]` through a second stray pointer. Nobody needs to call the API for this to happen: 1 cfs of 10 mg/L dry-weather flow is enough. A sanitizer build stops at the first routing step; a release build reads garbage and may corrupt the heap.
- **Flux on the wrong link.** With the indices in range (J1 = 0, J2 = 1; C2 = 0, C1 = 1), a flux set on C1 is read by C2 (whose upstream node J2 has index 1) and C1 reads C2's flux, which is 0. At the end of the run C1 carries 0 mg/L and C2 3.78 mg/L. The loads are booked to the wrong links' `totalLoad` as well.

## Why it happens

`j` holds the upstream node index, which the steady-flow rule needs for the conduit's concentration. The block added in 5.3.0 reuses it as a link index:

```c
// src/legacy/engine/qualrout.c, findSFLinkQual()
int j = Link[i].node1;                       // upstream NODE
...
    c1 = Node[j].newQual[p];                 // correct use of j
...
    cOut = Link[j].apiExtQualMassFlux[p];    // j used as a LINK index
    ...
        Link[j].totalLoad[p] -= cOut;
    ...
        Link[j].totalLoad[p] += cOut;
```

`Link` is allocated with exactly `Nobjects[LINK]` elements ([`project.c:1050`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L1050)). A tree network has one link fewer than nodes for each outfall, so its highest node indices are out of range for `Link`. Whether one of them is the upstream node of a conduit depends on the order of the node sections in the file: outfalls listed last avoid it, storage units or junctions listed after the outfalls do not. The same block in `findLinkQual()` (used by kinematic and dynamic wave) uses `Link[i]`.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-12_two-conduits.inp`](CRASH-12_two-conduits.inp) | Scenario B: J1 -> C1 -> J2 -> C2 -> O1, STEADY, 1 cfs of clean water; C2 is listed first so the indices are in range but differ |
| [`CRASH-12_outfall-first.inp`](CRASH-12_outfall-first.inp) | Scenario A: the same network with the outfall listed first (J2 = node 2, two links) and 10 mg/L in the inflow |
| [`CRASH-12_test.c`](CRASH-12_test.c) | Scenario B (5.3.0 only, `#ifdef` on the 5.3.0 header): sets a flux of 10 on C1 and reads both conduits' concentrations. Scenario A (both versions): runs the deck and checks the quality continuity error |
| [`CRASH-12_test6.c`](CRASH-12_test6.c) | Scenario A on 6.0.0: both conduits must carry 10 mg/L |

```sh
tools/run-test.sh CRASH-12            # 5.2.4 PASS, 5.3.0 CRASH, 6.0.0 PASS
tools/run-test.sh CRASH-12 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0 puts C1's flux into C2, then reads past the `Link` array in scenario A:

```
Scenario B: flux set on C1 (link index 1), C2 is link index 0
  final concentration  C1 = 0.00000 mg/L   C2 = 3.78186 mg/L   (error code 0)
  -> the flux set on C1 did not reach C1
=================================================================
==22305==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x517000000be0 ...
READ of size 8 at 0x517000000be0 thread T0
    #0 0x7f8ee716f8f0 in findSFLinkQual .../src/legacy/engine/qualrout.c:451:24
    #1 0x7f8ee716f8f0 in findLinkQual .../src/legacy/engine/qualrout.c:335:9
    #2 0x7f8ee716f8f0 in qualrout_execute .../src/legacy/engine/qualrout.c:146:9
CRASH-12 5.3.0 base: CRASH
```

5.2.4 and 6.0.0 run scenario A cleanly:

```
Scenario B: skipped (5.2.4 has no link pollutant flux API)
Scenario A: run finished with error code 0, quality continuity error 1.674 %
CRASH-12 5.2.4 base: PASS

Scenario B: not applicable (6.0.0 has no link pollutant flux API)
Scenario A: error code 0, final concentration C1 = 10.00000 mg/L  C2 = 10.00000 mg/L, quality continuity error -1.074 %
CRASH-12 6.0.0 base: PASS
```

**With the fix**, the flux set on C1 is in C1 (and carried on into C2), and scenario A gives the same result as 5.2.4:

```
Scenario B: flux set on C1 (link index 1), C2 is link index 0
  final concentration  C1 = 3.78186 mg/L   C2 = 3.78186 mg/L   (error code 0)
Scenario A: run finished with error code 0, quality continuity error 1.674 %
CRASH-12 5.3.0 patched: PASS
CRASH-12 6.0.0 patched: PASS
```

The 3.78 mg/L is not the concentration a flux of 10 should give at 1 cfs: most of the flux is lost by the steady-flow dilution in [NUM-50](../../1-numerical/NUM-50-steady-link-api-flux-mass-lost/README.md), and the value is in the internal mass unit described in [API-08](../../6-api/API-08-mass-flux-litres-conversion/README.md). The 1.7% continuity error of scenario A is the same in 5.2.4 and is not part of this defect.

## The fix

```diff
-        cOut = Link[j].apiExtQualMassFlux[p];
+        cOut = Link[i].apiExtQualMassFlux[p];
 ...
-            Link[j].totalLoad[p] -= cOut;
+            Link[i].totalLoad[p] -= cOut;
 ...
-            Link[j].totalLoad[p] += cOut;
+            Link[i].totalLoad[p] += cOut;
```

Models that do not use the link flux API get bit-identical results wherever the old code stayed in bounds (every link's flux is 0, so the block added nothing). None of the 73 regression decks combines STEADY routing with pollutants, so none of them is affected.
