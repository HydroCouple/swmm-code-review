# NUM-24: A transect's right end wall is left out of the wetted perimeter, the left one is not

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | When the two ends of a transect are at different heights, SWMM extends the lower end with a vertical wall. The wall's wetted perimeter counts on the left side and is dropped on the right side, so a transect and its mirror image differ once the water is above the lower end. In the test the full flow differs by 19 % (126.85 vs 150.61 cfs) and the depth at 60 cfs by 4 % (3.205 vs 3.079 ft). No warning. |
| **Reached from** | `[TRANSECTS]` whose last GR station is lower than the highest station, at depths above that last station; also the hydraulic radius and capacity at full depth |
| **5.3.0** | `getFlow()` in [`src/legacy/engine/transect.c:563`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L563); the walls are added by `transect_validate()` at [`transect.c:250`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L250)-[`253`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L253) |
| **5.2.4** | Same code, [`src/solver/transect.c:534`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/transect.c#L534) and [`transect.c:248`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/transect.c#L248) |
| **6.0.0** | Reproduces: `transect::buildTables()` is a port, with the same test in its `getFlow` lambda ([`Transect.cpp:110`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Transect.cpp#L110)) |
| **Since** | 5.0.022 or earlier (in the first commit of the EPA repository, 2014) |
| **Fix** | End a transect's last sub-section at the right wall, so that its perimeter counts as the left wall's does: [`NUM-24_swmm530.patch`](NUM-24_swmm530.patch), [`NUM-24_swmm600.patch`](NUM-24_swmm600.patch) |

## The problem

SWMM needs a transect to reach its full height at both ends. If one end is lower, `transect_validate()` adds a station directly above it at the highest elevation, i.e. a vertical wall. The reference manual (Vol. II, section 5.3) describes this, and its algorithm for the geometry tables treats the added segment like any other: its wetted perimeter goes into the compound segment it belongs to. HEC-RAS, whose HEC-2 transect format SWMM uses, also adds the wetted perimeter of such a wall.

In the code that only happens on the left. In the test, T1 has stations `GR 101 0 100 2 100 8 105 10` (left end 1 ft above the bottom, right end at the full 5 ft) and T2 is the same channel reflected, `GR 105 0 100 2 100 8 101 10`. With n = 0.03 throughout, no overbanks and the water at full depth, both have A = 44 ft², and with the 4 ft wall P = 17.62 ft, so the full flow is 126.85 cfs at slope 0.001. 5.2.4, 5.3.0 and 6.0.0 give:

```
Conduit  Transect                       Full flow (cfs)  Depth at 60 cfs (ft)
C1       T1 low end on the left               126.85           3.205
C2       T2 low end on the right              150.61           3.079
```

The Cross Section Summary in the test's report gives T2 a full hydraulic radius of 3.23 ft (A/P without the wall) against 2.50 ft for T1. Which result a user gets depends only on which bank was entered first.

## Why it happens

The walls become slices 1 and Nstations of the station loop. `getGeometry()` accumulates area and wetted perimeter slice by slice and asks `getFlow()` whether the current compound segment ends at this slice. `getFlow()` forces the end at the last real station, one slice before the right wall:

```c
// src/legacy/engine/transect.c, getFlow()
    if ( findFlow == FALSE)
    {
        // --- flow needs updating if we are at last station
        if ( k == Nstations - 1 ) findFlow = TRUE;
```

The right-wall slice that follows then starts a new segment of its own. It has no area, so its flow `PHI / n * a * pow(a/wp, 2./3.)` is zero, the `if ( q > 0.0 )` test in `getGeometry()` fails and its wetted perimeter is discarded. The left wall is slice 1: its perimeter is carried in `wpSum` into the first segment and counted.

For a transect the last segment should end at the wall slice, which is the real end of the section. Streets are built by `transect_createStreetTransect()` from their own station list, whose last slice is the street crown (half street) or the far sidewalk (full street), and they rely on the current split; for a half street, dropping the crown wall's perimeter is what keeps the crown frictionless.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-24_mirror.inp`](NUM-24_mirror.inp) | Two 1000 ft conduits at slope 0.001 with transect T1 and its mirror image T2, n = 0.03, 60 cfs each, dynamic wave |
| [`NUM-24_test.c`](NUM-24_test.c) | Legacy toolkit (5.2.4, 5.3.0): the full flows of C1 and C2 must agree to 0.5 % and lie within 1 % of the reference manual's value (both walls counted); the steady depths at 60 cfs must agree to 1 % |
| [`NUM-24_test6.c`](NUM-24_test6.c) | 6.0.0: the same checks; the full flows are read from the report's Cross Section Summary |

```sh
tools/run-test.sh NUM-24            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-24 --patched  # 5.3.0, 6.0.0: PASS
```

**Without the fix** (5.3.0 and 6.0.0 print the same; 5.2.4 gives 126.51 and 150.20 cfs, 0.3 % lower because of [NUM-57](../NUM-57-transect-radius-1-49-524/)):

```
Conduit  Transect                       Full flow (cfs)  Depth at 60 cfs (ft)
C1       T1 low end on the left               126.85           3.205
C2       T2 low end on the right              150.61           3.079
Full flow with both end walls in the wetted perimeter: 126.85 cfs
FAIL: the mirror image has +18.7 % full flow (150.61 vs 126.85 cfs) and -3.9 % depth at 60 cfs
NUM-24 5.2.4 base: FAIL
NUM-24 5.3.0 base: FAIL
NUM-24 6.0.0 base: FAIL
```

**With the fix** (both patched engines print the same):

```
Conduit  Transect                       Full flow (cfs)  Depth at 60 cfs (ft)
C1       T1 low end on the left               126.85           3.205
C2       T2 low end on the right              126.85           3.205
Full flow with both end walls in the wetted perimeter: 126.85 cfs
PASS: a transect and its mirror image have the same full flow and depth
NUM-24 5.3.0 patched: PASS
NUM-24 6.0.0 patched: PASS
```

## The fix

For transects, end the last compound segment at the right-wall slice instead of the slice before it. Streets keep the old index, so their results do not change:

```diff
+static int     LastSlice;              // slice that ends the last sub-section
 ...
     Station[Nstations] = Station[Nstations-1];
     Elev[Nstations] = Elev[0];
 
+    // --- the right end wall belongs to the last sub-section, as the
+    //     left end wall belongs to the first one
+    LastSlice = Nstations;
 ...
         // --- flow needs updating if we are at last station
-        if ( k == Nstations - 1 ) findFlow = TRUE;
+        if ( k == LastSlice ) findFlow = TRUE;
 ...
+    // --- the last slice (crown or right sidewalk) forms its own sub-section
+    LastSlice = Nstations - 1;
```

6.0.0's `buildTables()` already knows whether it builds a transect or a street (`add_end_walls`); the patch uses `k == (add_end_walls ? Nsta : Nsta - 1)` there.

This keeps the left-wall behaviour, which matches the reference manual and HEC-RAS, and brings the right wall into line with it: transects whose right end is lower than the left lose conveyance once the water is above the right end. The opposite convention (count neither wall) would also make the two orientations agree, but contradicts the manual's algorithm and would raise the conveyance of every transect whose left end is the lower one.

**Effect on other models.** Of the six regression decks in `regsuite` with transects, five (`extran8a`, `user2`, `user5`, `Example7-Inlets`, `Example7-Final`) give byte-identical output files with the patched 5.3.0 and 6.0.0 command-line programs. In `CoS-Reduced-Outlets.inp` the transect `Roadway` (`GR .61884 0 .2 7.4 0 7.4 .1925 15.1`) is a half road with its crown, 0.43 ft below the sidewalk, as the right end. Its 100 conduits now count the wall above the crown: full flow 16.01 instead of 16.52 m³/s for R3750 (all 100 drop by 3.1 %), full hydraulic radius 0.35 instead of 0.37 m. Peak flows change by at most 0.8 % (R3954E, 0.250 to 0.248 m³/s), peak depths by less than 0.1 %, and the routing continuity error from 0.256 % to 0.255 %. The patched 5.3.0 and 6.0.0 agree. A user who wants the crown of a half road to be frictionless has no way to say so for a transect, whichever side the crown is on; the STREETS section handles that case.
