# NUM-12: A storage curve that starts above zero depth loses volume once the water passes its first point

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | For a TABULAR storage unit whose curve's first depth x<sub>1</sub> is above 0, the stored volume drops by a<sub>1</sub>·x<sub>1</sub>/2 the moment the depth passes x<sub>1</sub>, and stays that much too low. Reported stored, average and maximum volumes are too low, and the routing continuity error shows the missing water (7.04% in the test, where 500 ft³ of 7200 ft³ disappears). Depths are not affected in dynamic wave routing. |
| **Reached from** | `[STORAGE]` units of shape `TABULAR` whose storage curve has its first point at a depth greater than 0, e.g. a stage-area table that starts at the first surveyed contour |
| **5.3.0** | `table_getStorageVolume()` in [`src/legacy/engine/table.c:604`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/table.c#L604); the inverse `table_getStorageDepth()` at [`:673`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/table.c#L673) includes the volume |
| **5.2.4** | Same code, [`src/solver/table.c:599`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/table.c#L599) and [`:668`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/table.c#L668) |
| **6.0.0** | Reproduces in `table_getStorageVolume()`, [`src/engine/data/TableData.hpp:388`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/data/TableData.hpp#L388), with the same numbers |
| **Since** | 5.2.0, which replaced 5.1's `table_getArea()` (that one started its sum at `y1*x1/2.0`) with `table_getStorageVolume()` |
| **Fix** | Start the end-area sum from a<sub>1</sub>·x<sub>1</sub>/2: [`NUM-12_swmm530.patch`](NUM-12_swmm530.patch), [`NUM-12_swmm600.patch`](NUM-12_swmm600.patch) |

## The problem

A storage curve gives surface area against depth. Below its first point (x<sub>1</sub>, a<sub>1</sub>) SWMM takes the area to grow linearly from 0 at depth 0, so the basin holds a<sub>1</sub>·x<sub>1</sub>/2 below x<sub>1</sub>. `table_getStorageVolume()` uses that triangle up to x<sub>1</sub>, and `table_getStorageDepth()` (the inverse) includes it at every depth. Above x<sub>1</sub>, `table_getStorageVolume()` forgets it. The volume of a filling basin therefore jumps down by a<sub>1</sub>·x<sub>1</sub>/2 as the water passes x<sub>1</sub>, and the two functions are no longer inverses of each other.

With the curve (1 ft, 1000 ft²) (6 ft, 1000 ft²), the volume at 1.00 ft is 500 ft³, at 1.01 ft 10 ft³, and at 2 ft 1000 ft³ instead of 1500 ft³. The same curve with (0, 0) written as its first point, which describes the same basin under SWMM's own rule, gives 1500 ft³.

In the test, two such units each receive 3600 ft³ and cannot drain. The unit with the explicit (0, 0) point holds 3596.5 ft³; the other reports 3096.5 ft³ at the same depth, and the run reports a flow routing continuity error of 7.04% with no other warning. In the Storage Volume Summary SU1's maximum volume is 3.097 against SU2's 3.597 (1000 ft³), and its average 1.367 against 1.797; with the fix both units report 1.797 and 3.597.

## Why it happens

```c
// src/legacy/engine/table.c, table_getStorageVolume()
    v = 0.0;
    ...
    x1 = entry->x;
    a1 = entry->y;

    // --- target depth is below first tabulated depth
    if (x <= x1)
    {
        if (x1 < 1.e-6) return 0.0;
        return (a1/x1) * x * x / 2.0;          // includes the triangle
    }

    // --- otherwise traverse table entries until target depth is bracketed
    while (entry->next)
    {
        ...
            return v + (a1 + a) / 2.0 * (x - x1);   // v started at 0
```

```c
// src/legacy/engine/table.c, table_getStorageDepth()
    d1 = entry->x;
    a1 = entry->y;
    v1 = a1 * d1 / 2.0;                         // includes the triangle
```

`node_getVolume()` uses `table_getStorageVolume()` (node.c:939) and `storage_getDepth()` uses `table_getStorageDepth()` (node.c:815). In dynamic wave routing the depth comes from the head update, so the depth is right and the volume derived from it is low. The continuity balance compares that volume with the inflow and outflow totals.

5.1.013's `table_getArea()`, which did the same job, set `a = y1*x1/2.0;` before its loop. 6.0.0 ports the 5.2 function with `double v = 0.0;`.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-12_storage-fill.inp`](NUM-12_storage-fill.inp) | Two storage units, each filled with 0.5 cfs for 2 hours (3600 ft³), outlet offset 5.5 ft so nothing leaves. SU1 uses the curve (1, 1000)(6, 1000), SU2 the same curve with (0, 0) first. Dynamic wave. |
| [`NUM-12_test.c`](NUM-12_test.c) | Steps the run through the legacy toolkit, records each unit's volume and depth, and checks that the final volume equals the 3600 ft³ that entered (within 1%), that the depth is 1 + (3600 − 500)/1000 = 4.1 ft (within 0.01 ft), and that the volume never drops while the unit fills |
| [`NUM-12_test6.c`](NUM-12_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-12            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-12 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, all three engines lose 495 ft³ in one routing step at 1.002 ft (5.2.4 and 5.3.0 shown; 6.0.0 prints the same volumes and a continuity error of 7.041%):

```
Inflow volume: 3600 ft3; expected depth 4.100 ft
Unit  Final volume (ft3)  Final depth (ft)  Largest volume drop (ft3)
SU1               3096.5             4.097       495.0 at depth 1.002 ft
SU2               3596.5             4.097         0.0 at depth 0.000 ft
Flow routing continuity error: 7.038 %
FAIL: 1 storage unit(s) do not hold the volume that entered (SU1 3096.5 ft3, SU2 3596.5 ft3 of 3600)
NUM-12 5.3.0 base: FAIL
```

**With the fix**, both units hold the same volume:

```
Inflow volume: 3600 ft3; expected depth 4.100 ft
Unit  Final volume (ft3)  Final depth (ft)  Largest volume drop (ft3)
SU1               3596.5             4.097         0.0 at depth 0.000 ft
SU2               3596.5             4.097         0.0 at depth 0.000 ft
Flow routing continuity error: 0.093 %
PASS: both units hold the 3600 ft3 that entered at 4.1 ft, and the volume never drops
NUM-12 5.3.0 patched: PASS
```

6.0.0 patched prints the same volumes and depths, with a continuity error of 0.097%. (The 6.0.0 API's continuity value differs from legacy's in the third decimal before and after the fix, 7.041% vs 7.038%; both reports print 7.038%.)

## The fix

```diff
         if (x1 < 1.e-6) return 0.0;
         return (a1/x1) * x * x / 2.0;
     }
 
+    // --- start from the volume below the first tabulated depth
+    //     (as in table_getStorageDepth)
+    v = a1 * x1 / 2.0;
+
     // --- otherwise traverse table entries until target depth is bracketed
```

The 6.0.0 patch initialises `v` the same way in `TableData.hpp`. Volumes are now continuous at x<sub>1</sub>, `table_getStorageVolume()` and `table_getStorageDepth()` invert each other, and a curve with or without an explicit (0, 0) point gives the same results. Full volumes (`Node[j].fullVolume`) and the report's storage volumes of affected units go up by a<sub>1</sub>·x<sub>1</sub>/2.

Effect on other models: curves whose first depth is 0 compute `v = a1 * 0 / 2.0 = 0`, exactly as before. Every TABULAR storage curve in the regression decks starts at depth 0; `user2.inp` (28 tabular units), `Storage_Shape_Test.inp` and `extran9.inp` give byte-identical binary output files with the patch, in both 5.3.0 and 6.0.0.
