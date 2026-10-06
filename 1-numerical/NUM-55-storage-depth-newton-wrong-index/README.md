# NUM-55: 5.2.4 computes the depth of FUNCTIONAL, CONICAL and PYRAMIDAL storage units with another node's geometry and mixed units

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | 5.2.4 only. Wherever SWMM turns a storage volume into a depth (kinematic wave routing does this every step), units whose depth needs a Newton solve get a wrong depth: about 10 times too small in metric models, and in US models the depth of a different storage unit whenever node and storage indices differ (any storage unit listed after a junction, outfall or divider, or after another storage unit with a different index). In the test a pyramidal basin holding 359.4 ft³ reports 3.73 ft instead of 1.26 ft. No warning. |
| **Reached from** | Storage units of shape CONICAL, PYRAMIDAL, or FUNCTIONAL with A0 > 0 and an exponent other than 0 or 1, in kinematic wave routing (and any other path that calls `storage_getDepth()`) |
| **5.3.0** | Fixed: `storage_getDepth()` passes the node index, [`src/legacy/engine/node.c:807`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L807), and `storage_getVolDiff()` converts units, [`:875`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L875) |
| **5.2.4** | `storage_getDepth()` sets `storageVol.k = k` (the storage index), [`src/solver/node.c:838`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L838), and `storage_getVolDiff()` passes it to node-indexed functions with a depth in user units, [`:906`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L906) |
| **6.0.0** | Fixed in the same commit, [`src/engine/hydraulics/Node.cpp:271`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Node.cpp#L271) |
| **Since** | 5.2.0, when `storage_getVolDiff()` started calling `storage_getVolume()` and `storage_getSurfArea()` (5.1.013 evaluated the FUNCTIONAL formula directly from `Storage[k]`). Fixed in the OpenSWMM fork by [3d09a10a](https://github.com/HydroCouple/Stormwater-Management-Model/commit/3d09a10a3a0b48d27a5f9dcc5c32f8d77a65fb6f) (12 July 2026); EPA's `develop` branch still has the 5.2.4 code. |
| **Fix** | None needed for 5.3.0 and 6.0.0. For 5.2.4, commit 3d09a10a is the fix. |

## The problem

For CONICAL, PYRAMIDAL and general FUNCTIONAL shapes there is no closed-form depth for a given volume, so `storage_getDepth()` solves V(d) = v with Newton's method. In 5.2.4 the callback that evaluates V(d) and A(d) for the solver has two defects:

- **Wrong node.** It calls `storage_getVolume()` and `storage_getSurfArea()` with the storage unit's index into `Storage[]`, but both functions take a node index and look up `Node[j].subIndex` themselves. Unless the two indices happen to coincide, the solve uses the shape, full depth and full volume of whichever node sits at that position.
- **Wrong units.** The trial depth and target volume are in user units, but `storage_getVolume()` and `storage_getSurfArea()` take a depth in feet and return ft³ and ft². In US units the factors are 1; in metric units the solve converges to a depth about 10 times too small.

The test fills two storage units for one hour with no outflow, under kinematic wave routing, and compares each reported depth with the depth at which the unit's own shape holds the reported volume:

| Deck | Unit | Volume | 5.2.4 depth | V(5.2.4 depth) | 5.3.0 / 6.0.0 depth |
|---|---|---|---|---|---|
| metric | SF, FUNCTIONAL 50 + 10 d² | 35.94 m³ | 0.0668 m | 3.340 m³ | 0.6965 m |
| metric | SP, PYRAMIDAL 20 × 10 m, slope 2 | 35.94 m³ | 0.0167 m | 3.351 m³ | 0.1709 m |
| US, junction first | SF | 359.4 ft³ | 3.7296 ft | 359.400 ft³ | 3.7301 ft |
| US, junction first | SP | 359.4 ft³ | 3.7296 ft | 1857.163 ft³ | 1.2642 ft |

In the US deck SF is node 1 and storage 0, so its solve uses node 0, the junction, whose `subIndex` happens to be 0 too, and gets SF's own shape. SP is node 2 and storage 1, so its solve uses node 1, which is SF, and returns SF's depth.

## Why it happens

```c
// src/solver/node.c (5.2.4), storage_getDepth()
    int    k = Node[j].subIndex;
    ...
    // --- convert volume to user's units
    v *= UCF(VOLUME);
    storageVol.k = k;                 // storage index, not node index
    storageVol.v = v;                 // user units
    ...
        case CONICAL:
        case PYRAMIDAL:
        d = v / a0;
        findroot_Newton(0.0, Node[j].fullDepth*UCF(LENGTH), &d,
            0.001, storage_getVolDiff, &storageVol);
```

```c
// src/solver/node.c (5.2.4), storage_getVolDiff()
    k = storageVol->k;

    // --- compute volume & surface area at depth y
    *f = storage_getVolume(k, y) - storageVol->v;   // k used as node index;
    *df = storage_getSurfArea(k, y);                // y in user units, results in ft
```

5.3.0 (vendored in OpenSWMM) stores the node index (`storageVol.j = j`) and converts y to feet and the results back to user units inside `storage_getVolDiff()`. 6.0.0's `storage_getDepth` has the same correction, with a comment describing the legacy defect.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-55_cms.inp`](NUM-55_cms.inp) | CMS: SF (FUNCTIONAL A1 10, A2 2, A0 50) and SP (PYRAMIDAL 20 × 10 m, side slope 2), each filled at 0.01 cms for 1 hour, outlets above the water; the storage units are the first nodes, so only the units defect acts. Kinematic wave. |
| [`NUM-55_cfs-junction-first.inp`](NUM-55_cfs-junction-first.inp) | The same two shapes in feet, filled at 0.1 cfs, with a junction listed first, so only the index defect acts |
| [`NUM-55_test.c`](NUM-55_test.c) | Runs both decks through the legacy toolkit and checks V(reported depth) = reported volume within 1% for each unit |
| [`NUM-55_test6.c`](NUM-55_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-55            # 5.2.4: FAIL; 5.3.0 and 6.0.0: PASS
```

**5.2.4**:

```
NUM-55_cms.inp
  SF  node 0  volume    35.940 m3  depth  0.0668 m  V(depth)     3.340   <-- wrong depth
  SP  node 1  volume    35.940 m3  depth  0.0167 m  V(depth)     3.351   <-- wrong depth
NUM-55_cfs-junction-first.inp
  SF  node 1  volume   359.400 ft3  depth  3.7296 ft  V(depth)   359.400
  SP  node 2  volume   359.400 ft3  depth  3.7296 ft  V(depth)  1857.163   <-- wrong depth
FAIL: 3 of 4 storage units report a depth at which their shape does not hold their volume
NUM-55 5.2.4 base: FAIL
```

**5.3.0 and 6.0.0** (identical output):

```
NUM-55_cms.inp
  SF  node 0  volume    35.950 m3  depth  0.6965 m  V(depth)    35.950
  SP  node 1  volume    35.950 m3  depth  0.1709 m  V(depth)    35.950
NUM-55_cfs-junction-first.inp
  SF  node 1  volume   359.500 ft3  depth  3.7301 ft  V(depth)   359.500
  SP  node 2  volume   359.500 ft3  depth  1.2642 ft  V(depth)   359.500
PASS: every storage unit's depth matches its volume
NUM-55 5.3.0 base: PASS
NUM-55 6.0.0 base: PASS
```

(5.2.4's volumes are 0.01 m³ and 0.1 ft³ lower than 5.3.0's; the check compares each engine's depth with that engine's own volume.)

## The fix

No patch: 5.3.0 and 6.0.0 at cb3e192b already contain the fix from commit 3d09a10a. Users of EPA SWMM 5.2.x get the wrong depths for these shapes in kinematic wave routing until EPA adopts an equivalent change.
