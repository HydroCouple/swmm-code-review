# CRASH-13: Setting a pollutant mass flux before swmm_start crashes (link) or writes out of bounds (node)

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | A toolkit client that sets a link pollutant mass flux between `swmm_open` and `swmm_start` crashes the host process (NULL pointer dereference). The same call for a node with a pollutant index outside 0..N-1 writes 8 bytes outside a heap block and returns 0, which corrupts memory silently in a release build. A valid node call also returns 0, but `swmm_start` zeroes the value, so the run has no load and nothing warns. |
| **Reached from** | `swmm_setValueExpanded(swmm_LINK, swmm_LINK_POLLUTANT_LATMASS_FLUX, ...)` or `swmm_setValueExpanded(swmm_NODE, swmm_NODE_POLLUTANT_LATMASS_FLUX, ...)` before `swmm_start`, e.g. from the Python bindings |
| **5.3.0** | Pre-start branches of `setNodeValue()` and `setLinkValue()` in [`src/legacy/engine/swmm5.c:2343`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2343) and [`:2441`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2441); the pointer is declared NULL at [`:2357`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2357); the values are zeroed in [`qualrout.c:82`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L82) and [`:96`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L96) |
| **5.2.4** | Not affected: it has no expanded setters and no pollutant mass flux |
| **6.0.0** | Not affected: `swmm_node_set_quality_mass_flux()` and `swmm_forcing_node_quality()` check the pollutant index and return `SWMM_ERR_LIFECYCLE` before the run starts ([`openswmm_nodes_impl.cpp:385`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_nodes_impl.cpp#L385)); there is no link flux |
| **Since** | 5.3.0, fork commit 5b87a2b5 ("WIP API bindings for pollutants", December 2024) |
| **Fix** | Return `ERR_API_NOT_STARTED` for both properties until the run has started: [`CRASH-13_swmm530.patch`](CRASH-13_swmm530.patch) |

## The problem

5.3.0 added a pollutant mass flux to the toolkit for nodes and links (`swmm_NODE_POLLUTANT_LATMASS_FLUX`, `swmm_LINK_POLLUTANT_LATMASS_FLUX`). Like the other expanded setters, `setNodeValue()` and `setLinkValue()` have a running branch and a branch "Set values that can only be configured before simulation starts", and both properties appear in both. The running branches work. The pre-start branches do three different wrong things:

- **Link:** the call dereferences a NULL pointer. Any client that configures a conduit load before starting the run crashes.
- **Node, pollutant index out of range:** the call writes past the end (or before the start) of the node's `apiExtQualMassFlux` array, which has one entry per pollutant, and returns 0.
- **Node, valid pollutant index:** the call returns 0 and the getter reads the value back (5.0), but `swmm_start` resets it to zero (the getter then returns 0.0), so the run carries no load.

In the test deck (J1 -> C1 -> O1, 1 cfs dry-weather flow, two pollutants with no other source), each call is made with a flux of 5.0 before `swmm_start`: the first node call is accepted and J1's concentration stays 0.0 for the whole run, the out-of-range node call is a heap-buffer-overflow, and the link call is a NULL dereference. The same node and link calls made after `swmm_start` (before the first `swmm_step`) return 0 and are used: J1 and C1 both reach 5.0 mg/L.

## Why it happens

`setLinkValue()` sets its `link` pointer only in the running branch; the pre-start branch uses it anyway, and checks and indexes by `subIndex` where every other path uses `pollutantIndex`:

```c
// src/legacy/engine/swmm5.c, setLinkValue()
TLink *link = NULL;
...
else if (IsStartedFlag == TRUE)
{
    link = &Link[index];
    ...
}
else
{
    // Set values that can only be configured before simulation starts
    switch (property)
    {
    ...
    case swmm_LINK_POLLUTANT_LATMASS_FLUX:
    {
        if (subIndex < 0 || subIndex >= Nobjects[POLLUT])
            return ERR_API_OBJECT_INDEX;

        link->apiExtQualMassFlux[subIndex] = value;     // link is NULL here
        return 0;
    }
```

`setNodeValue()`'s running branch checks `pollutantIndex` before writing; its pre-start branch does not:

```c
// src/legacy/engine/swmm5.c, setNodeValue(), pre-start branch
case swmm_NODE_POLLUTANT_LATMASS_FLUX:
    Node[index].apiExtQualMassFlux[pollutantIndex] = value;   // no bounds check
    return 0;
```

Even a correct pre-start write could not take effect. `swmm_start()` -> `routing_open()` -> `qualrout_init()` clears both arrays for every node and link, which is needed so that a flux from a previous `swmm_start`/`swmm_end` cycle in the same session does not carry over:

```c
// src/legacy/engine/qualrout.c, qualrout_init()
Node[i].oldQual[p] = c;
Node[i].newQual[p] = c;
Node[i].apiExtQualMassFlux[p] = 0.0;
...
Link[i].apiExtQualMassFlux[p] = 0.0;
```

This is the same pattern as [API-01](../../6-api/API-01-settings-before-start-reset/) (gage rainfall, node inflow and link setting set before `swmm_start` are reset by it); API-01 left these two properties to this issue because their pre-start branch also crashes.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-13_two-pollutants.inp`](CRASH-13_two-pollutants.inp) | J1 -> C1 -> O1, 1 cfs DWF at J1, pollutants P1 and P2 with zero concentration everywhere; DYNWAVE, 1 hour |
| [`CRASH-13_test.c`](CRASH-13_test.c) | Three pre-start calls, each in a child process (so one crash does not hide the next): node flux for P1, node flux for pollutant 2 (out of range), link flux for P2; each child then runs the deck and records the largest concentration at the object. 5.2.4 has no such API and prints PASS |
| [`CRASH-13_test6.c`](CRASH-13_test6.c) | The same through 6.0.0's `swmm_node_set_quality_mass_flux()` and `swmm_forcing_node_quality()` |

A call passes if it is refused with a non-zero code, or if the run uses the value (concentration > 0), and nothing crashes.

```sh
tools/run-test.sh CRASH-13            # 5.3.0: CRASH; 5.2.4 and 6.0.0: PASS
tools/run-test.sh CRASH-13 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0 (sanitizer reports shortened):

```
Flux 5.0 set with swmm_setValueExpanded before swmm_start (2 pollutants)
call                                                   rc       max conc   result
NODE_POLLUTANT_LATMASS_FLUX(J1, P1 = 0)                 0         0.0000   ACCEPTED BUT IGNORED
=================================================================
==25508==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x502000000320 at pc 0x7f83febcbda4 bp 0x7ffc248c16f0 sp 0x7ffc248c16e8
WRITE of size 8 at 0x502000000320 thread T0
    #0 0x7f83febcbda3 in setNodeValue .../src/legacy/engine/swmm5.c:2344:60
    #1 0x7f83febcbda3 in swmm_setValueExpanded .../src/legacy/engine/swmm5.c:1462:16
0x502000000320 is located 0 bytes after 16-byte region [0x502000000310,0x502000000320)
NODE_POLLUTANT_LATMASS_FLUX(J1, pollutant 2)   CRASHED (exit code 1)
../src/src/legacy/engine/swmm5.c:2446:19: runtime error: member access within null pointer of type 'TLink'
LINK_POLLUTANT_LATMASS_FLUX(C1, P2 = 1)        CRASHED (exit code 1)
FAIL: 3 of 3 pre-start flux calls are wrong (2 crashed, 1 accepted but ignored)
CRASH-13 5.3.0 base: CRASH
```

The 16-byte region is J1's two-pollutant array. 6.0.0 refuses every call with `SWMM_ERR_LIFECYCLE` (6):

```
swmm_node_set_quality_mass_flux(J1, P1 = 0)           6       0.0000   refused
swmm_node_set_quality_mass_flux(J1, pollutant 2)      6       0.0000   refused
swmm_forcing_node_quality(J1, P1 = 0)                 6       0.0000   refused
swmm_forcing_node_quality(J1, pollutant 2)            6       0.0000   refused
PASS: every pre-start pollutant flux call is refused with an error code or used by the run, without a crash
CRASH-13 6.0.0 base: PASS
```

**With the fix**, 5.3.0 refuses all three calls with `ERR_API_NOT_STARTED` (-999902):

```
NODE_POLLUTANT_LATMASS_FLUX(J1, P1 = 0)           -999902         0.0000   refused
NODE_POLLUTANT_LATMASS_FLUX(J1, pollutant 2)      -999902         0.0000   refused
LINK_POLLUTANT_LATMASS_FLUX(C1, P2 = 1)           -999902         0.0000   refused
PASS: every pre-start pollutant flux call is refused with an error code or used by the run, without a crash
CRASH-13 5.3.0 patched: PASS
```

## The fix

There are two ways to make the pre-start branch correct: refuse the call, or keep the value through `swmm_start`. Keeping it would mean moving the reset out of `qualrout_init()` (which `swmm_start` also skips when a hotstart file is read, [`routing.c:125`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L125)) into `swmm_end` and project creation. The patch refuses it instead, which is what API-01 does for the other forcing values and what 6.0.0 does for its node flux:

```diff
         case swmm_NODE_POLLUTANT_LATMASS_FLUX:
-            Node[index].apiExtQualMassFlux[pollutantIndex] = value;
-            return 0;
+            // --- swmm_start resets this (qualrout_init)
+            return ERR_API_NOT_STARTED;
 ...
         case swmm_LINK_POLLUTANT_LATMASS_FLUX:
-        {
-            if (subIndex < 0 || subIndex >= Nobjects[POLLUT])
-                return ERR_API_OBJECT_INDEX;
-
-            link->apiExtQualMassFlux[subIndex] = value;
-            return 0;
-        }
+            // --- swmm_start resets this (qualrout_init)
+            return ERR_API_NOT_STARTED;
```

No write happens before the run starts, so the NULL pointer and the unchecked index are never used. A flux set after `swmm_start`, including before the first `swmm_step`, goes through the running branches, which are unchanged and check `pollutantIndex`. Input-file runs never call these setters, so no model result changes.
