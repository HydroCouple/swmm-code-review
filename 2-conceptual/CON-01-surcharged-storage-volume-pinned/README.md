# CON-01: The surcharge band of a closed storage unit holds water its volume never books

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | A closed storage unit (surcharge depth > 0) behaves as if the band between its full depth and its maximum surcharged depth held A × SurDepth of water, but its reported volume stays at full volume. Flooding comes later and is smaller than it should be, the hidden water appears in no volume report, and the routing continuity error grows by up to A × SurDepth (+14% and +7% in the tests, -36% when such a tank drains). In 5.2.4 the no-link case freezes the depth instead and loses all later inflow (+72%). |
| **Reached from** | Dynamic wave routing and a storage unit with a surcharge depth. Under SURCHARGE_METHOD SLOT every closed storage unit is affected. Under EXTRAN (default) only units with no head-dependent link: no links at all, or only outlets and pumps. |
| **5.3.0** | Area above full depth from `initNodeStates()`, [`src/legacy/engine/dynwave.c:311`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L311), used by the free-surface update in `setNodeDepth()`, [`dynwave.c:693`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L693). The volume is capped in `storage_getVolume()`, [`node.c:930`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L930). |
| **5.2.4** | SLOT: the same. EXTRAN without head-dependent links: the surcharge branch has a zero denominator and the depth freezes, [`src/solver/dynwave.c:685`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dynwave.c#L685) and [`:733`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dynwave.c#L733) |
| **6.0.0** | Reproduces in `DWSolver::initNodeStates()`, [`src/engine/hydraulics/DynamicWave.cpp:1547`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/DynamicWave.cpp#L1547). 6.0.0 also books a tank that starts above full depth at the curve volume of its initial depth, because `SWMMEngine::initialize()` computes the initial volume before the full volume that caps it, [`src/engine/core/SWMMEngine.cpp:834`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L834) |
| **Since** | 5.1.013, which added closed storage units and SLOT. The EXTRAN no-link case went from a frozen depth to band storage with OpenSWMM #149 (commits ee1b5efe and 059d3428, December 2023). |
| **Fix** | Give a closed storage unit no free surface of its own above full depth: [`CON-01_swmm530.patch`](CON-01_swmm530.patch), [`CON-01_swmm600.patch`](CON-01_swmm600.patch) (6.0.0 also fixes the initial-volume order) |

## The problem

A storage unit with a surcharge depth is closed: it holds its full volume, and above full depth its head can rise by up to SurDepth as pressure. SWMM books the volume that way, but the depth update does not always follow it.

**Closed tank, no links, EXTRAN** (`CON-01_closed-tank-fill.inp`). A 1,000 ft² tank, 10 ft full, SurDepth 5 ft, takes 10 cfs for an hour. It is full at 1,000 s (16.7 min), so from then on the rest of the inflow, 26,000 ft³, should flood. In 5.3.0 and 6.0.0 the head keeps rising over the full 1,000 ft² and only reaches 15 ft at 25 min. The 5,000 ft³ held in the band is never booked, so flooding is about 5,000 ft³ short and the continuity error is +13.9%. In 5.2.4 the depth freezes at 10.03 ft: the tank never floods and the error is +72.2%.

**The same tank draining** (`CON-01_closed-tank-drain.inp`). It starts surcharged at 15 ft, full at 10,000 ft³, and 1 cfs is withdrawn for an hour. It should end at 6,400 ft³ (6.4 ft). In 5.3.0 the depth falls through the band, to 11.41 ft, while the volume stays at 10,000 ft³, so the 3,600 ft³ withdrawn came from nowhere (-36.0%). 6.0.0 shows +9.3% instead, because it books the initial volume at 15 ft as 15,000 ft³, which no later step keeps.

**Closed tank between pipes, SLOT** (`CON-01_slot-tank.inp`). A 2,000 ft² tank between a 2 ft inflow pipe and a 1 ft outlet pipe takes 20 cfs for 2 h. Under SLOT the band holds 10,000 ft³. The tank first floods at 40 min instead of 25 min, it floods 47,590 ft³ instead of 56,784 ft³, and the continuity error is +7.0%. A CLI run of the same deck under EXTRAN gives 0.381%.

A full fill-and-drain cycle gives the band water back, so the error nets out. A run that ends with the tank surcharged, or that starts that way, keeps it.

## Why it happens

`storage_getVolume()` gives a storage unit its full volume at or above full depth. That is the lid of a closed unit:

```c
// src/legacy/engine/node.c, storage_getVolume()
if ( d >= Node[j].fullDepth
&&   Node[j].fullVolume > 0.0 ) return Node[j].fullVolume;
```

Every iteration, `initNodeStates()` starts the node's surface area from its area curve at the current depth, above full depth included. The link end areas are then added:

```c
// src/legacy/engine/dynwave.c, initNodeStates()
Xnode[i].newSurfArea = node_getSurfArea(i, Node[i].newDepth);
```

`setNodeDepth()` treats a full closed storage unit as surcharged under EXTRAN, and uses the head-derivative update `dy = dQ / sumdqdh` there. That update has no area and stores nothing, which is right for a closed unit. Two cases use the free-surface update `dy = dV / surfArea` instead, with the curve area:

- SLOT never sets `isSurcharged`, so every node always takes the free-surface update.
- Under EXTRAN, OpenSWMM #149 sends a surcharged storage unit with `sumdqdh == 0` to the free-surface update: `if (!isSurcharged || (Node[i].type == STORAGE && Xnode[i].sumdqdh == 0.0))`. Before #149, and in 5.2.4, the surcharge branch ran with a zero denominator, `if ( denom == 0.0 ) dy = 0.0;`, so the depth froze and the inflow was discarded.

In both cases the depth moves through the band as if it had the curve area, while `node_getVolume()` keeps returning the full volume. The depth update and the volume book use two geometries for the same tank.

6.0.0 has the same area rule in `DWSolver::initNodeStates()`. In addition, `SWMMEngine::initialize()` computes each node's initial volume before its `full_volume`, so the cap in `node::getVolume()` is not yet active. A tank that starts above full depth is booked at its curve volume (15,000 ft³ here). Legacy `node_initState()` computes `fullVolume` first.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-01_closed-tank-fill.inp`](CON-01_closed-tank-fill.inp) | Closed tank S1 (1,000 ft², 10 ft + 5 ft surcharge), no links, 10 cfs for 1 h, EXTRAN, fixed 5 s step |
| [`CON-01_closed-tank-drain.inp`](CON-01_closed-tank-drain.inp) | S1 starting at 15 ft, 1 cfs withdrawn (negative inflow) for 1 h |
| [`CON-01_slot-tank.inp`](CON-01_slot-tank.inp) | Closed tank S2 (2,000 ft², 10 ft + 5 ft) between pipes C1 (2 ft) and C2 (1 ft), 20 cfs for 2 h, SURCHARGE_METHOD SLOT |
| [`CON-01_test.c`](CON-01_test.c) | Legacy toolkit (5.2.4, 5.3.0). Runs the three decks, prints the tank's final depth and volume, the volume it flooded and when it first floods, and the routing continuity error. Checks \|error\| < 1% and that the drained tank ends at 10,000 ft³ minus the water withdrawn (±50 ft³). |
| [`CON-01_test6.c`](CON-01_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh CON-01            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-01 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.3.0:

```
deck                              depth     volume    flooded  floods at  cont.err
                                   (ft)      (ft3)      (ft3)      (min)       (%)
CON-01_closed-tank-fill.inp       15.00      10000      20950       25.1    13.899
CON-01_closed-tank-drain.inp      11.41      10000          0          -   -35.975
CON-01_slot-tank.inp              15.00      20000      47590       40.1     7.030
Drain: withdrawn 3595 ft3, expected final volume 6405 ft3, booked 10000 ft3
FAIL: the surcharge band holds water the tank volume does not book, in the fill drain SLOT deck(s) (see table)
CON-01 5.3.0 base: FAIL
```

5.2.4 freezes the no-link tank instead:

```
CON-01_closed-tank-fill.inp       10.03      10000          0          -    72.195
CON-01_closed-tank-drain.inp      15.00      10000          0          -   -35.965
CON-01_slot-tank.inp              15.00      20000      42488       41.5     7.030
CON-01 5.2.4 base: FAIL
```

6.0.0 matches 5.3.0 except for the drain, where it books the initial 15,000 ft³:

```
CON-01_closed-tank-fill.inp       15.00      10000      21000       25.1    13.889
CON-01_closed-tank-drain.inp      11.40      10000          0          -     9.333
CON-01_slot-tank.inp              15.00      20000      47610       40.1     7.029
CON-01 6.0.0 base: FAIL
```

**With the fix**, the no-link tank floods as soon as it is full (16.8 min), the drained tank ends at 6.39 ft, and the SLOT tank floods at 24.6 min, as it does under EXTRAN:

```
CON-01_closed-tank-fill.inp       15.00      10000      25900       16.8     0.139
CON-01_closed-tank-drain.inp       6.39       6395          0          -     0.125
CON-01_slot-tank.inp              15.00      20000      56784       24.6     0.118
Drain: withdrawn 3595 ft3, expected final volume 6405 ft3, booked 6395 ft3
PASS: closed storage units conserve water through the surcharge band in all three decks
CON-01 5.3.0 patched: PASS
...
CON-01 6.0.0 patched: PASS
```

The two test programs differ by one step at the end. The legacy `swmm_step()` returns 0 on the last step, so the legacy test misses that step's 5 ft³ of withdrawal and 50 ft³ of flooding; the 6.0.0 test records it. The 6.0.0 report files give the same continuity errors as 5.3.0's (13.899%, and 0.125% patched for the drain). `swmm_get_routing_continuity_error()` differs slightly from its own report (13.889% vs 13.899%), because the API totals count the first step's inflow in full.

## The fix

A closed storage unit has no free surface above its full depth, so it contributes no area of its own there:

```diff
         else
         {
             Xnode[i].newSurfArea = node_getSurfArea(i, Node[i].newDepth);
         }
 
+        // --- a closed storage unit has no free surface above its full depth
+        if ( Node[i].type == STORAGE && Node[i].surDepth > 0.0 &&
+             Node[i].newDepth > Node[i].fullDepth )
+            Xnode[i].newSurfArea = 0.0;
+
```

In the band the unit then behaves like a junction in surcharge. The depth moves with the area of its links (the Preissmann slot widths under SLOT), floored at MIN_SURFAREA. At most MIN_SURFAREA × SurDepth (63 ft³ here) can then go unbooked, as at any junction. The 0.1% left in the tests is of that order. The 6.0.0 patch makes the same change in `DWSolver::initNodeStates()` (it skips a storage unit that can pond, which legacy input cannot create). It also computes `full_volume` before the initial volume in `SWMMEngine::initialize()`, as legacy does.

The review's first proposal was to drive the depth of a no-link tank from its volume balance and send any excess straight to flooding. That is exact for an isolated tank, but it does not cover SLOT, and a tank drained only by an outlet would jump between full depth and maximum depth from step to step. Removing the lid's area covers both surcharge methods with one rule.

**Effect on other models.** EXTRAN storage units with conduits use the dQ/dH update above full depth and are not affected. update_v52/CoS-Reduced-Outlets has 16 closed storage units (SurDepth 100), several of which surcharge, and gives byte-identical reports with both patched engines. So do CoS-Reduced-Inlets, extran5 and user3. None of the regression decks uses SLOT.
