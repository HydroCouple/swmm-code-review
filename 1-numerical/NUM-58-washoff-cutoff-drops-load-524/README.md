# NUM-58: 5.2.4 sends runoff below 0.001 in/hr to the network with no pollutant, and drops its load

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | In 5.2.4, whenever a subcatchment's outflow is below 0.001 in/hr, its runoff concentration is set to 0 after the load has already been taken off the surface. The water is still routed, so the network receives clean water and the load is booked nowhere. A drizzle of 0.0005 in/hr at 10 mg/L for 10 h on 1,000 ac sends 0.005 ac-ft to the junction at 0 mg/L instead of 10 mg/L; 0.126 lb is missing and the runoff quality continuity error is 11.1 %. Recession tails, drizzle and LID-dominated subcatchments (where capture drives the outflow below the cutoff) lose pollutant this way. |
| **Reached from** | Any run with pollutants in which a subcatchment's surface + LID drain outflow is between 0 and 0.001 in/hr over its area |
| **5.3.0** | Not affected: the vendored 5.3.0 forms the concentration whenever there is pre-LID outflow ([`src/legacy/engine/surfqual.c:334`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/surfqual.c#L334)) |
| **5.2.4** | `surfqual_getWashoff()` in [`src/solver/surfqual.c:253`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/surfqual.c#L253) and [`:260`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/surfqual.c#L260). The same code is on EPA's `develop` branch (last commit 23 February 2025). |
| **6.0.0** | Not affected: the concentration is formed over the pre-LID outflow with no cutoff ([`src/engine/core/SWMMEngine.cpp:4234`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L4234)) |
| **Since** | The cutoff has zeroed the routed concentration since at least 5.1.010; 5.2.0 moved it ahead of the ledger bookings, so the load also left the runoff mass balance |
| **Fix** | None needed for 5.3.0 and 6.0.0: fixed in the fork by commit e8a8d107 (11 July 2026, "fix(surfqual): conserve pollutant mass when runoff falls below cutoff (#90)"). EPA's code still needs that change. |

## The problem

A 1,000 ac impervious subcatchment with no depression storage receives a drizzle of 0.0005 in/hr for 10 hours, with a rain concentration of 10 mg/L. There is no buildup and no evaporation, so the only pollutant is the one in the rain, and the runoff, about 0.0025 cfs, should carry it at 10 mg/L. The runoff stays below SWMM's `MIN_RUNOFF` of 0.001 in/hr the whole time.

5.2.4 routes the water to junction J1 at 0 mg/L for all 24 reporting periods. Its runoff quality balance:

```
  Wet Deposition ...........         1.132
  Surface Runoff ...........         0.000
  Remaining Buildup ........         1.007
  Continuity Error (%) .....        11.093
```

and the routing balance shows a Wet Weather Inflow of 0.000 lb. 5.3.0 and 6.0.0 report Surface Runoff 0.126 lb, a Wet Weather Inflow of 0.125 lb, and J1 at 10 mg/L. (5.3.0's runoff quality error is 0.000 %. 6.0.0 reports 88.9 %, because it leaves the 1.007 lb still ponded at the end out of Remaining Buildup, see [CON-09](../../2-conceptual/CON-09-ponded-evaporation-destroys-pollutant/).)

The amounts are small in this deck because the flow is small, but the error is systematic: every step of every recession tail below 0.001 in/hr delivers clean water, and in a subcatchment with LID units that capture almost all inflow, the load of the small remaining outflow is lost at every step.

## Why it happens

`surfqual_getWashoff()` first takes the washoff off the buildup (`findWashoffLoads()`) and the ponded runoff load off the ponded mass (`findPondedLoads()`), summing them in `OutflowLoad[p]`. It then converts that load to a concentration only if the post-LID outflow exceeds the cutoff:

```c
// src/solver/surfqual.c (5.2.4), surfqual_getWashoff()
    // --- determine if subcatchment outflow is below a small cutoff
    hasOutflow = (vOut2 > MIN_RUNOFF * area * tStep);

    // --- for each pollutant
    for (p = 0; p < Nobjects[POLLUT]; p++)
    {
        // --- convert washoff load to a concentration
        cOut = 0.0;
        if ( vOut1 > 0.0 && hasOutflow ) cOut = OutflowLoad[p] / vOut1;
        ...  // BMP removal, totalLoad, RUNOFF_LOAD and newQual all use cOut
```

With `cOut = 0` the BMP removal, the subcatchment's total load, the `RUNOFF_LOAD` ledger entry and the routed concentration `Subcatch[j].newQual` are all 0. The load already removed from the surface is in none of them. The runoff flow itself is not cut, so the water still reaches the network, clean.

Up to 5.1.015 the cutoff was applied after the ledger bookings (`if ( !hasOutflow ) cOut = 0.0;` just before `newQual` was set), so the load was at least counted as Surface Runoff, though it still did not reach the network. 5.2.0 moved the test into the concentration formula, ahead of the bookings.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-58_drizzle.inp`](NUM-58_drizzle.inp) | 1,000 ac impervious, no depression storage, 0.0005 in/hr for 10 h at 10 mg/L, no buildup, no evaporation, KINWAVE, 24 h |
| [`NUM-58_test.c`](NUM-58_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0) and reads J1's inflow and P1 for every reporting period of the `.out` file |
| [`NUM-58_test6.c`](NUM-58_test6.c) | The same through the 6.0.0 C API, after every routing step |

The only water reaching J1 is rain at 10 mg/L and nothing adds or removes pollutant, so J1 must read 10 mg/L whenever it has inflow. The test allows 1 %.

```sh
tools/run-test.sh NUM-58            # 5.2.4: FAIL, 5.3.0: PASS, 6.0.0: PASS
```

5.2.4:

```
Reporting periods with inflow at J1 ...... 24 of 24
Largest inflow at J1 (cfs) ............... 0.00261
P1 at J1 while it has inflow (mg/L) ...... 0.000 to 0.000 (rain: 10.000)
FAIL: runoff below the MIN_RUNOFF cutoff reaches J1 without its pollutant (0.000 to 0.000 mg/L instead of 10)
NUM-58 5.2.4 base: FAIL
```

5.3.0 and 6.0.0:

```
Reporting periods with inflow at J1 ...... 24 of 24
Largest inflow at J1 (cfs) ............... 0.00260
P1 at J1 while it has inflow (mg/L) ...... 10.000 to 10.000 (rain: 10.000)
PASS: runoff below the cutoff carries its 10 mg/L load to the network
NUM-58 5.3.0 base: PASS
...
Routing steps with inflow at J1 .......... 1439 of 1440
Largest inflow at J1 (cfs) ............... 0.00262
P1 at J1 while it has inflow (mg/L) ...... 10.000 to 10.000 (rain: 10.000)
PASS: runoff below the cutoff carries its 10 mg/L load to the network
NUM-58 6.0.0 base: PASS
```

## The fix

There are no patches: 5.3.0 and 6.0.0 are fixed. The fork's commit e8a8d107 forms the concentration whenever there is pre-LID outflow:

```diff
-    // --- determine if subcatchment outflow is below a small cutoff
-    hasOutflow = (vOut2 > MIN_RUNOFF * area * tStep);
 ...
         cOut = 0.0;
-        if ( vOut1 > 0.0 && hasOutflow ) cOut = OutflowLoad[p] / vOut1;
+        if (vOut1 > 0.0)
+            cOut = OutflowLoad[p] / vOut1;
```

(the same commit also splits the pre/post-LID volume difference between native-soil infiltration and BMP removal). EPA's 5.2.4 and its `develop` branch need the first part of that change.
