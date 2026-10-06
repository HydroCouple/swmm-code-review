# CON-09: Pollutant in water ponded on a subcatchment disappears when the water evaporates

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | Wet deposition and run-on load held in depression storage is destroyed in proportion to the water that evaporates. 0.1 in of rain at 10 mg/L on a 10 ac impervious lot that never runs off loses all 2.265 lb: Wet Deposition 2.265, Surface Runoff 0, Remaining Buildup 0, runoff quality continuity error 100 %. With a second, larger storm the error is 82 % (88 % in 6.0.0), and the second storm's runoff carries water whose concentration has not been raised by the evaporation. The runoff quantity balance closes, so only the quality table hints at it. |
| **Reached from** | Any pollutant with a rain concentration (`Crain`) or any subcatchment receiving run-on from another subcatchment, together with evaporation of ponded surface water (depression storage) |
| **5.3.0** | `findPondedLoads()` in [`src/legacy/engine/surfqual.c:465`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/surfqual.c#L465) |
| **5.2.4** | Same code: [`src/solver/surfqual.c:382`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/surfqual.c#L382) |
| **6.0.0** | Reproduces ([`src/engine/core/SWMMEngine.cpp:4160`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L4160)), and also drops the ponded mass when the surface dries ([`:4095`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L4095)) and leaves the mass still ponded at the end out of Remaining Buildup ([`:6001`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L6001)) |
| **Since** | 5.1.008, which introduced the ponded-water quality balance (`findPondedLoads()`) |
| **Fix** | Keep the mass that infiltration and runoff did not remove: [`CON-09_swmm530.patch`](CON-09_swmm530.patch), [`CON-09_swmm600.patch`](CON-09_swmm600.patch) |

## The problem

Since 5.1.008 SWMM tracks the pollutant carried by rain (wet deposition) and by run-on in the water ponded on a subcatchment's surface. The reference manual (Vol. III, section 4.3.2) writes its mass balance as

> d(V<sub>ponded</sub> C<sub>ponded</sub>)/dt = Q<sub>runon</sub>C<sub>runon</sub> + Q<sub>ppt</sub>C<sub>ppt</sub> − C<sub>ponded</sub>(Q<sub>infil</sub> + Q<sub>out</sub>)   (4-11)

with evaporation only in the volume balance (4-12), and states: "Evaporation removes volume but not mass causing C<sub>ponded</sub> to increase." The code does not do that.

A 10 ac fully impervious lot with 0.2 in of depression storage receives 0.1 in of rain at 10 mg/L, 2.265 lb of pollutant. Nothing runs off. Evaporation (0.5 in/day) dries the surface in about 5 hours. All three engines report:

```
  Wet Deposition ...........         2.265
  Surface Runoff ...........         0.000
  Remaining Buildup ........         0.000
  Continuity Error (%) .....       100.000
```

The water is accounted for (Evaporation Loss 0.083 ac-ft = the rainfall), the pollutant is not. With a second storm of 0.3 in at hour 14 that does run off, 5.3.0 reports Surface Runoff 1.083 lb, Remaining Buildup 0.570 lb and an 81.8 % error for 9.058 lb deposited. The mass in the water that evaporates during and after each storm is lost, and the runoff concentration is too low because the evaporation never concentrates the pond.

## Why it happens

`findPondedLoads()` mixes the ponded mass with the rain and run-on loads over the step's total water volume, removes the infiltrated and runoff mass, and then throws the remainder away and rebuilds the ponded mass from the end-of-step depth:

```c
// src/legacy/engine/surfqual.c, findPondedLoads()
            wPonded = Subcatch[j].pondedQual[p] + wRain + wRunon;
            cPonded = wPonded / Vinflow;      // Vinflow = ponded + rain + run-on volume

            // --- mass lost to infiltration
            wInfil = cPonded * Vinfil;
            ...
            wPonded -= wInfil;

            // --- mass lost to runoff
            wOutflow = cPonded * Voutflow;
            ...
            wPonded -= wOutflow;
            ...
            // --- update ponded mass (using newly computed ponded depth)
            Subcatch[j].pondedQual[p] = cPonded * subcatch_getDepth(j) * nonLidArea;
```

The volumes balance as V<sub>inflow</sub> = V<sub>infil</sub> + V<sub>outflow</sub> + V<sub>evap</sub> + V<sub>final</sub>. After infiltration and runoff, `wPonded` = c<sub>ponded</sub>(V<sub>evap</sub> + V<sub>final</sub>) is left, but only c<sub>ponded</sub>V<sub>final</sub> is kept. The pollutant in the evaporated water, c<sub>ponded</sub>V<sub>evap</sub>, disappears at every step, with no ledger entry. When the surface is dry the remaining ponded mass is booked as `FINAL_LOAD` (Remaining Buildup), but by then evaporation has taken all of it.

The code follows step 4 of the algorithm in the same manual section, m<sub>p</sub> = C<sub>ponded</sub> d<sub>2</sub> A, which the manual says accounts for evaporation "implicitly". It accounts for infiltration and runoff, but it removes the solute along with the evaporated water, which contradicts Eq. 4-11 and note 4 next to it. Conduits and storage units, where the manual says the same thing ("When water is evaporated, the pollutant mass stays behind", section 5.2), concentrate the remaining water with an evaporation factor instead.

Rebuilding from the depth also creates or destroys mass when the volumes do not close exactly, for instance with runoff routed between the impervious and pervious sub-areas, which enters the receiving sub-area one step later and is not part of V<sub>inflow</sub>. EPA's Example1 with a rain concentration has no evaporation and still shows a −0.028 % runoff quality error from this.

6.0.0 rebuilds the ponded mass the same way, on purpose. It also sets the ponded mass to 0 when the surface dries without booking it (the legacy `FINAL_LOAD` line was removed because the end-of-run total overwrote it), and its end-of-run Remaining Buildup does not include the mass still ponded. Both are noted in its comments as "a small parity gap". Together they give 88 % instead of 82 % on the two-storm deck.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-09_rain-evaporates.inp`](CON-09_rain-evaporates.inp) | 10 ac impervious, 0.2 in depression storage, 0.1 in of rain at 10 mg/L, evaporation 0.5 in/day, 24 h |
| [`CON-09_two-storms.inp`](CON-09_two-storms.inp) | The same lot: 0.1 in at hour 1 that evaporates, then 0.3 in at hour 14 that runs off |
| [`CON-09_test.c`](CON-09_test.c) | Runs both decks through the legacy toolkit (5.2.4 and 5.3.0) and reads the Runoff Quality Continuity table |
| [`CON-09_test6.c`](CON-09_test6.c) | The same through the 6.0.0 C API |

The test applies conservation of mass: every pound deposited by rain must end in Surface Runoff or Remaining Buildup. It requires the runoff quality continuity error, reported and recomputed from the table's own lines, within 1 %.

```sh
tools/run-test.sh CON-09            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-09 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix:**

```
Deck                         Wet         Surface   Remaining  Error from  Reported
                             deposition  runoff    buildup    the table   error
                             (lb)        (lb)      (lb)       (%)         (%)
CON-09_rain-evaporates.inp       2.265     0.000      0.000     100.000   100.000  <-- wrong
CON-09_two-storms.inp            9.058     1.083      0.570      81.751    81.754  <-- wrong
FAIL: in 2 of 2 decks pollutant deposited in ponded water vanishes as the water evaporates (runoff quality continuity error)
CON-09 5.3.0 base: FAIL
```

5.2.4 prints 1.082 / 0.569 / 81.778 % for the second deck. 6.0.0 loses the remaining ponded mass as well:

```
CON-09_rain-evaporates.inp       2.265     0.000      0.000     100.000   100.000  <-- wrong
CON-09_two-storms.inp            9.058     1.083      0.000      88.044    88.044  <-- wrong
CON-09 6.0.0 base: FAIL
```

**With the fix** both engines print the same:

```
CON-09_rain-evaporates.inp       2.265     0.000      2.265       0.000     0.000
CON-09_two-storms.inp            9.058     1.165      7.893       0.000     0.000
PASS: pollutant in evaporating ponded water stays on the surface; the runoff quality balance closes
CON-09 5.3.0 patched: PASS
CON-09 6.0.0 patched: PASS
```

The second storm's runoff now carries 1.165 lb instead of 1.083 lb: the ponded water it washes off has been concentrated by evaporation. Most of the deposit stays on the surface because most of the rain in this deck evaporates.

## The fix

5.3.0: keep what infiltration and runoff did not remove.

```diff
-            // --- update ponded mass (using newly computed ponded depth)
-            Subcatch[j].pondedQual[p] = cPonded * subcatch_getDepth(j) * nonLidArea;
+            // --- update ponded mass: what infiltration and runoff did not
+            //     remove stays (evaporation takes water, not pollutant)
+            Subcatch[j].pondedQual[p] = wPonded;
```

When the surface has dried and gets no rain or run-on, the existing `Vinflow == 0.0` branch books the residue to Remaining Buildup, as before. The concentration of the ponded water now rises as it evaporates, as Eq. 4-11 says, and the runoff quality balance closes by construction.

6.0.0: the same change in the ponded-quality block of the runoff step, plus the two pieces that 6.0.0 dropped: the ponded mass of a surface that dries is booked to a new running total, `qual_final_ponded_dry` (added to `MassBalance` in `SimulationContext.hpp` next to `qual_routing_final_dry`), and `computeFinalQualityMassBalance()` adds that total and the mass still ponded to Remaining Buildup, as legacy's `FINAL_LOAD` and `massbal_getBuildup()` do. A running total is needed because `computeFinalQualityMassBalance()` runs at every step and overwrites the term.

Effect on other models: with private patched builds of both engines, EPA's Example1, `events_example.inp`, the parity deck `steady_quality.inp` and the 6.0.0 `site_drainage_example.inp` give byte-identical `.out` files and reports: none has a rain concentration or run-on load. With a rain concentration of 10 mg/L added to TSS:

| Deck | Engine | Surface Runoff (lb) | Remaining Buildup (lb) | Runoff quality error | External Outflow (lb) |
|---|---|---|---|---|---|
| Example1 + Crain | 5.3.0 | 602.305 → 600.105 | 3316.486 → 3317.295 | −0.028 % → 0.000 % | 568.785 → 566.610 |
| Example1 + Crain | 6.0.0 | 602.305 → 600.105 | 3314.297 → 3317.295 | +0.025 % → 0.000 % | 569.287 → 567.124 |
| site_drainage_example + Crain | 5.3.0 | 1731.588 → 1730.557 | 90.449 → 90.569 | −0.037 % → 0.000 % | 1187.761 → 1191.913 |
| site_drainage_example + Crain | 6.0.0 | 1731.588 → 1730.557 | 88.759 → 90.569 | +0.054 % → 0.000 % | 1189.299 → 1193.445 |

Example1 has no evaporation; the change there is the mass the depth-based rebuild created when water moved between sub-areas. In the site-drainage deck the routed load also shifts between outflow and flooding (Flooding Loss 494.789 → 489.660 lb in 5.3.0).
