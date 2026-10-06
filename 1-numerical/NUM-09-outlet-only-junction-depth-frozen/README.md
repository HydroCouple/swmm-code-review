# NUM-09: A junction drained only by outlets never changes depth under dynamic wave routing

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | The junction's depth stays wherever the first step put it. The outlet discharges at that fixed head whatever the inflow does, and the difference is lost: a 22.4% routing continuity error in the test. The report's continuity error is the only sign. |
| **Reached from** | Dynamic wave routing with EXTRAN surcharge (the default), and a junction whose only links are outlets (rating curve or functional outlets). |
| **5.3.0** | Crown from the outlet's DUMMY section in `dynwave_init()`, [`src/legacy/engine/dynwave.c:146`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L146) (`yFull = TINY`, [`xsect.c:238`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L238)). Surcharge test and `dy = 0` in `setNodeDepth()`, [`dynwave.c:686`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L686) and [`:731`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L731) |
| **5.2.4** | Same, [`src/solver/dynwave.c:690`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dynwave.c#L690) and [`:733`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dynwave.c#L733) |
| **6.0.0** | Reproduces, on purpose: `SWMMEngine::initGeometry()` copies the TINY crown for parity and its comment describes the frozen depth, [`src/engine/core/SWMMEngine.cpp:9480`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L9480). `DWSolver::setNodeDepth()` then sets `dy = 0`, [`DynamicWave.cpp:4087`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/DynamicWave.cpp#L4087) |
| **Since** | Every release: 5.0.022 already gives DUMMY sections `yFull = TINY` and treats any node with `yLast > yCrown` as surcharged |
| **Fix** | Update a surcharged node whose links have no `dqdh` from its surface area, as 5.3.0 already does for storage units: [`NUM-09_swmm530.patch`](NUM-09_swmm530.patch), [`NUM-09_swmm600.patch`](NUM-09_swmm600.patch) |

## The problem

Junction J1 starts dry, receives a constant 2 cfs and drains only through outlet OL1, a functional outlet with `q = 1.0·h^0.5` on J1's depth. J1 should fill until the outlet passes the inflow, `h^0.5 = 2`, so 4.0 ft. With SWMM's minimum junction area of 12.566 ft² and `dq/dh = 0.25 cfs/ft` at 4 ft, it takes about a minute.

In 5.2.4, 5.3.0 and 6.0.0, J1 rises to 2.387 ft on the first 30 s step (30 ft³ / 12.566 ft²) and stays there for the 3-hour run. The outlet passes 1.545 cfs, and the other 0.455 cfs disappears: the routing continuity error is 22.364%. With an initial depth the junction freezes at that depth instead. The review found this with a deck whose junction starts at 2 ft and holds exactly 2.00 ft for 3 h (28.7%).

## Why it happens

Three pieces combine. An outlet has no cross-section, so `link_setParams()` gives it a DUMMY section, and every dimension of a DUMMY section is `TINY` (1e-6 ft):

```c
// src/legacy/engine/xsect.c, xsect_setParams()
case DUMMY:
    xsect->yFull = TINY;
```

`dynwave_init()` raises each node's crown to the top of its highest link, outlets included, so J1's crown is 1e-6 ft above its invert:

```c
// src/legacy/engine/dynwave.c, dynwave_init()
z = Node[j].invertElev + Link[i].offset1 + Link[i].xsect.yFull;
Node[j].crownElev = MAX(Node[j].crownElev, z);
```

`setNodeDepth()` treats a junction above its crown as surcharged and updates its depth from the head derivatives of its links. Outlets never set `dqdh` (`findNonConduitFlow()` resets it to 0 and `outlet_getInflow()` leaves it there), so the denominator is 0 and the depth change is 0:

```c
// src/legacy/engine/dynwave.c, setNodeDepth()
else isSurcharged = (yCrown > 0.0 && yLast > yCrown);   // yCrown = 1e-6
...
if (!isSurcharged || (Node[i].type == STORAGE && Xnode[i].sumdqdh == 0.0))
{
    dy = dV / surfArea;            // free-surface update
    ...
}
else
{
    denom = Xnode[i].sumdqdh;      // 0: only outlets
    ...
    if ( denom == 0.0 ) dy = 0.0;  // depth frozen, dQ discarded
```

5.3.0 already exempts storage units with `sumdqdh == 0` from the surcharge branch (OpenSWMM #149, commit 059d3428). Junctions were left out. 6.0.0 keeps the TINY crown deliberately: the comment in `initGeometry()` says such a junction "is EXTRAN-'surcharged' from the first drop, with sumdqdh 0 — its depth freezes and its inflow is lost", and cites a 5.8% continuity error in a deck of its own test set.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-09_outlet-junction.inp`](NUM-09_outlet-junction.inp) | J1 (dry, 10 ft deep) with 2 cfs inflow, drained only by functional outlet OL1 (`q = 1.0·h^0.5`) into J2 → C1 → O1. 3 h, fixed 30 s step. |
| [`NUM-09_test.c`](NUM-09_test.c) | Legacy toolkit (5.2.4, 5.3.0). Prints J1's inflow and depth and OL1's flow, then checks the final depth against the analytic 4.0 ft and the routing continuity error. |
| [`NUM-09_test6.c`](NUM-09_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-09            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-09 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (the same in 5.2.4, 5.3.0 and 6.0.0; 5.3.0 shown):

```
  time  J1 inflow  J1 depth  OL1 flow
 (min)      (cfs)      (ft)     (cfs)
    15      2.000     2.387     1.545
    30      2.000     2.387     1.545
    45      2.000     2.387     1.545
    60      2.000     2.387     1.545
   120      2.000     2.387     1.545
Expected J1 depth: 4.000 ft (outlet passes 2 cfs)
Routing continuity error: 22.364 %
FAIL: J1 ends at 2.387 ft instead of 4.000 ft, the outlet passes 1.545 of 2 cfs, continuity error 22.364 %
NUM-09 5.3.0 base: FAIL
```

**With the fix**, both engines print:

```
    15      2.000     4.000     2.000
    ...
Expected J1 depth: 4.000 ft (outlet passes 2 cfs)
Routing continuity error: -0.231 %
PASS: J1 fills to 4.000 ft where the outlet passes the 2 cfs inflow; continuity error -0.231 %
NUM-09 5.3.0 patched: PASS
NUM-09 6.0.0 patched: PASS
```

The remaining -0.231% is SWMM's usual treatment of junction storage. Filling J1 to 4 ft over its 12.566 ft² minimum area takes 50 ft³, which a junction does not book as volume (50 / 21,600 ft³ = 0.23%).

## The fix

Extend the 5.3.0 storage-unit rule to every node: if no link at the node responds to head (`sumdqdh == 0`), the surcharge formula cannot move the depth, so use the free-surface update.

```diff
-    if (!isSurcharged || (Node[i].type == STORAGE && Xnode[i].sumdqdh == 0.0))
+    // The same holds for any node whose links all have dqdh = 0, such as a
+    // junction drained only by outlets (whose DUMMY section sets its crown).
+    if (!isSurcharged || Xnode[i].sumdqdh == 0.0)
```

The 6.0.0 patch makes the same change to the explicit branch of `DWSolver::setNodeDepth()`.

Conduits always add a positive `dqdh`, so any node with a conduit is unchanged. The new path is taken only where the old one set `dy = 0` or was near it: nodes whose links are all outlets, closed orifices or pumps without a head-dependent curve. Where the old code was close to `dy = 0`, depth was within 25% above a crown and no link responded to head. Two alternatives were considered and not used:

- Skipping DUMMY sections in the crown loop would also move the crown of every storage unit and junction drained by an outlet with an offset. That crown also limits the variable time step (`getNodeStep()`) and feeds the surcharge statistics, so many more models would change.
- Giving outlets a `dqdh` would keep J1 in the surcharge formula. A junction at the head of an outlet would then hold no water even though it has a free surface.

**Effect on other models.** Five DW regression decks give byte-identical reports with the patched 5.3.0 and 6.0.0 CLIs. They are update_v52/CoS-Reduced-Outlets, CoS-Reduced-Inlets, extran5, user2 and user3 (weirs, orifices and pumps). CoS-Reduced-Outlets has 16 rating-curve outlets from street junctions into catch-basin storage units, but each of those junctions also has conduits. None of the decks has a node whose links are all outlets.
