# CON-02: MIN_SURFAREA overrides the area curve of a small storage unit

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | A storage unit smaller than MIN_SURFAREA (12.566 ft² or 1.167 m² by default) fills as if it had that area, while its volume comes from its own curve. Its depth is too low by the ratio of the two areas, and the difference in volume disappears. In the test, a 4 ft² wet well holds 9.6 of the 30 ft³ that entered and shows 2.4 ft instead of 7.6 ft (continuity error +68%). Pump on/off levels in such wet wells are reached only after 3.1 times as much water has entered. |
| **Reached from** | Dynamic wave routing with a storage unit whose area (plus the area of its links) is below MIN_SURFAREA: small wet wells and catch basins modelled as storage units, or any curve near its zero-area bottom |
| **5.3.0** | `setNodeDepth()` in [`src/legacy/engine/dynwave.c:666`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L666) |
| **5.2.4** | Same, [`src/solver/dynwave.c:670`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dynwave.c#L670) |
| **6.0.0** | Reproduces in `DWSolver::setNodeDepth()`, [`src/engine/hydraulics/DynamicWave.cpp:3827`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/DynamicWave.cpp#L3827) |
| **Since** | Every release: the floor has applied to all nodes since 5.0 |
| **Fix** | Cap a storage unit's floor at its own area at full depth: [`CON-02_swmm530.patch`](CON-02_swmm530.patch), [`CON-02_swmm600.patch`](CON-02_swmm600.patch) |

## The problem

Wet well S1 is a 2 ft × 2 ft storage unit (constant area 4 ft², 10 ft deep) with no outlet. It receives 0.05 cfs for 10 minutes, 30.25 ft³ in all, so it should end at 30.25 / 4 = 7.56 ft holding 30.25 ft³.

5.2.4, 5.3.0 and 6.0.0 raise S1's depth as if it were 12.566 ft² and book its volume from the 4 ft² curve. S1 ends at 2.41 ft with 9.63 ft³; the other 20.6 ft³ is gone, and the routing continuity error is +68.2%. A control that switches a pump at a level in S1 sees that level only after 3.1 times as much water has entered.

The same applies wherever a storage unit's own area plus its links' areas is below MIN_SURFAREA. In SI decks the default is 1.167 m², but a deck that sets `MIN_SURFAREA 12.566` in SI units makes the floor 12.566 m². update_v52/CoS-Reduced-Outlets in the regression suite does exactly that. It models its catch basins as 0.64 m² storage units, so their depths were computed with an area up to 20 times too large.

## Why it happens

`setNodeDepth()` floors every node's surface area before the depth update, and books the volume from the node's own geometry:

```c
// src/legacy/engine/dynwave.c, setNodeDepth()
surfArea = Xnode[i].newSurfArea;           // storage curve area + link areas
surfArea = MAX(surfArea, MinSurfArea);     // 12.566 ft2 by default
...
dy = dV / surfArea;
...
else Node[i].newVolume = node_getVolume(i, yNew);   // storage curve volume
```

The Hydraulics Reference Manual (Vol. II, eq. 3-22) describes the floor as a guard "against the nodal head change formula 3-15 from becoming unbounded as surface area becomes vanishingly small", "strictly a computational device" that "does not add volume to a junction node". At a junction, whose volume is not booked, that holds. At a storage unit with a real area of 4 ft², the floor is not guarding a vanishing area. It replaces the unit's geometry in the depth equation but not in the volume, so the two disagree. 6.0.0 applies the same floor to every non-virtual node.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-02_small-wet-well.inp`](CON-02_small-wet-well.inp) | Storage S1 (FUNCTIONAL, constant 4 ft², 10 ft), no links, 0.05 cfs for 10 min, 20 min run, fixed 5 s step, default MIN_SURFAREA. J1-C1-O1 is only there because SWMM needs an outfall. |
| [`CON-02_test.c`](CON-02_test.c) | Legacy toolkit (5.2.4, 5.3.0). Integrates S1's inflow and checks S1's final volume against it (±0.5 ft³) and its depth against volume / 4 ft² (±0.05 ft). |
| [`CON-02_test6.c`](CON-02_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh CON-02            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-02 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (the same in all three; 5.3.0 shown):

```
  time   inflow vol   S1 depth  S1 volume
 (min)        (ft3)       (ft)      (ft3)
     2         5.87      0.468       1.87
     4        11.87      0.945       3.78
     6        17.87      1.422       5.69
     8        23.87      1.900       7.60
    10        29.87      2.377       9.51
    15        30.25      2.407       9.63
Expected at the end: volume 30.25 ft3, depth 7.562 ft (4 ft2)
Routing continuity error: 68.166 %
FAIL: S1 ends at 2.407 ft holding 9.63 ft3, but 30.25 ft3 entered (7.562 ft over its 4 ft2)
CON-02 5.3.0 base: FAIL
```

The depth is exactly the inflow volume over 12.566 ft² (30.25 / 12.566 = 2.407 ft).

**With the fix**, both engines print:

```
     2         5.87      1.469       5.88
    ...
    15        30.25      7.562      30.25
Expected at the end: volume 30.25 ft3, depth 7.562 ft (4 ft2)
Routing continuity error: 0.000 %
PASS: S1 holds the 30.25 ft3 that entered, at 7.562 ft
CON-02 5.3.0 patched: PASS
CON-02 6.0.0 patched: PASS
```

## The fix

For a storage unit, cap the floor at the unit's own area at full depth:

```diff
     surfArea = Xnode[i].newSurfArea;
-    surfArea = MAX(surfArea, MinSurfArea);
+    aMin = MinSurfArea;
+
+    // --- MinSurfArea does not override a smaller storage unit's own area
+    if ( Node[i].type == STORAGE )
+    {
+        aFull = node_getSurfArea(i, Node[i].fullDepth);
+        if ( aFull > 0.0 ) aMin = MIN(aMin, aFull);
+    }
+    surfArea = MAX(surfArea, aMin);
```

A storage unit smaller than MIN_SURFAREA now uses its real area. A curve that starts at zero area but is larger than MIN_SURFAREA when full keeps the floor near its bottom, where the floor does what it is meant to do: it stops `dV / surfArea` from blowing up. The volume error there is only the sliver of the curve below MIN_SURFAREA. Junctions and storage units at least MIN_SURFAREA in size are unchanged. The 6.0.0 patch makes the same change in `DWSolver::setNodeDepth()`.

The simplest alternative, no floor at all for storage units, would divide by a vanishing area at the bottom of every curve that starts at zero area. The fix keeps the floor there. Updating storage depths from volume would need a new root solve that includes the link areas.

This rule also covers the band of a closed storage unit above full depth after the [CON-01](../CON-01-surcharged-storage-volume-pinned/README.md) fix, where the unit's own area is set to zero. There the floor is MIN_SURFAREA, or the unit's area if it is smaller.

**Effect on other models.** update_v52/CoS-Reduced-Outlets (SI, `MIN_SURFAREA 12.566` m², 16 catch-basin storage units of 0.64 m²) changes in both patched engines identically. Its continuity error goes from 0.256% to 0.220%, outfall volume from 3.964 to 3.966 ML, and the catch basins' maximum depths rise, for example L4055 from 0.13 to 0.27 m, L3953 from 0.38 to 0.51 m and L4157A from 0.74 to 0.97 m. Storage_Shape_Test, extran5, user3, user5 and gate_control_2 keep their continuity errors and flooding.
