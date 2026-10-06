# NUM-14: A nearly empty storage unit loses its evaporation rate in ft/s as if it were a flow in cfs

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Small. While a storage unit holds 1e-4 ft³ or less, `storage_getLosses()` returns the evaporation rate in ft/s as a loss in cfs: the unit loses (rate × time step) ft³ per step whatever its surface area, and the report books it as evaporation. In the test a 1000 ft² unit holding 5e-5 ft³ loses 9.645e-6 ft³ per 10 s step under 1 in/day of evaporation. The amounts are tiny, but the formula is dimensionally wrong, and in 6.0.0 the same value had to be copied on purpose because removing it made a regression deck diverge. |
| **Reached from** | Any storage unit with a non-zero evaporation factor (Fevap) and a non-zero evaporation rate, during steps when it holds 1e-4 ft³ or less (an emptying or dry unit) |
| **5.3.0** | `storage_getLosses()` in [`src/legacy/engine/node.c:1069`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L1069) |
| **5.2.4** | Same code, [`src/solver/node.c:1083`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L1083) |
| **6.0.0** | Reproduces on purpose in the router's storage-loss step, [`src/engine/hydraulics/Routing.cpp:720`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Routing.cpp#L720) ("Quirk reproduced verbatim") |
| **Since** | 5.1.014 ("Fixed bug in storage_losses() that affected storage exfiltration"), which moved the `newVolume > FUDGE` test from the whole loss block to the evaporation line only. In 5.1.013 a unit holding FUDGE or less had no losses at all. |
| **Fix** | Set the evaporation loss to 0 when the unit holds FUDGE or less: [`NUM-14_swmm530.patch`](NUM-14_swmm530.patch), [`NUM-14_swmm600.patch`](NUM-14_swmm600.patch) |

## The problem

The Reference Manual (Vol. II, Eq. 7-14) defines the evaporation loss from a storage unit as Q<sub>EN</sub> = e<sub>t</sub> f<sub>E</sub> A<sub>SN</sub>(Y), the evaporation rate times the surface area, capped so that a step cannot remove more than the unit holds. The code skips the multiplication by the area when the unit holds 1e-4 ft³ (FUDGE) or less, meaning "no evaporation from an empty unit", but forgets to zero the rate. The ft/s value is then added to the seepage rate, capped against the stored volume, returned as the node's loss in cfs and saved as the step's evaporation loss.

The test unit has a constant 1000 ft² surface, holds 5e-5 ft³, gets no inflow and evaporates at 1 in/day (9.645e-7 ft/s). Physically it would be dry within the first step (1000 ft² × 9.645e-7 ft/s = 9.6e-4 ft³/s); under the rule the code intends it keeps its 5e-5 ft³. Instead it loses exactly 9.645e-7 ft³ every second, a loss that does not depend on its area, until it is empty after about a minute.

## Why it happens

```c
// src/legacy/engine/node.c, storage_getLosses()
        // --- get node's evap. rate (ft/s) &  exfiltration object
        k = Node[j].subIndex;
        evapRate = Evap.rate * Storage[k].fEvap;
        ...
            // --- compute evap rate over this area (cfs)
            if (Node[j].newVolume > FUDGE)
                evapRate = area * evapRate;
            ...
            totalLoss = (evapRate + exfilRate) * tStep;
            ...
    Storage[Node[j].subIndex].evapLoss = evapRate * tStep;
    ...
    return evapRate + exfilRate;
```

Up to 5.1.013 the whole block sat inside `if ( Node[j].newVolume > FUDGE )`, so `evapRate` kept its initial value 0 for a nearly empty unit. 5.1.014 narrowed the test to the evaporation line so that seepage is computed for a unit that is momentarily empty, and the ft/s value leaked through.

6.0.0 first zeroed this case, then copied the legacy value back because the different loss in one routing step grew into a visible difference on a regression deck:

```cpp
// src/engine/hydraulics/Routing.cpp
                // ... v6 zeroed that case; an almost-empty storage
                // therefore lost nothing where legacy books ~4e-8 cfs, and the
                // 1-ULP gap in the node's outflow grew into a deck-wide
                // divergence (usgs-runoff's P005). Quirk reproduced verbatim.
                double evap_cfs = stor_evap_rate;
                if (nodes.volume[ui] > constants::FUDGE)
                    evap_cfs = area * stor_evap_rate;
```

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-14_nearly-empty.inp`](NUM-14_nearly-empty.inp) | One storage unit, constant 1000 ft² (FUNCTIONAL 0 0 1000), initial depth 5e-8 ft (5e-5 ft³), Fevap 1, constant evaporation 1 in/day, no inflow, outlet 5 ft up. Dynamic wave, 10 s steps. |
| [`NUM-14_test.c`](NUM-14_test.c) | Steps the first three routing steps through the legacy toolkit and checks that each step either removes nothing (the unit is treated as empty) or empties it (rate × area, capped); removing rate × dt is the defect |
| [`NUM-14_test6.c`](NUM-14_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-14            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-14 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, all three engines print (the first dynamic-wave step is 0.5 s and averages the loss with the zero loss at time 0):

```
Evaporation rate 9.645e-7 ft/s; rate x area = 9.645e-4 ft3/s
Step  dt (s)  Volume before  Volume after   Loss (ft3)  Loss/dt (ft3/s)
   1    0.50     5.0000e-05    4.9759e-05   2.4113e-07       4.8225e-07
   2   10.00     4.9759e-05    4.0114e-05   9.6450e-06       9.6450e-07
   3   10.00     4.0114e-05    3.0469e-05   9.6450e-06       9.6450e-07
FAIL: in 3 of 3 steps the nearly empty unit lost the evaporation rate in ft/s as if it were a flow in ft3/s
NUM-14 5.3.0 base: FAIL
```

**With the fix**, 5.3.0 and 6.0.0 print:

```
Evaporation rate 9.645e-7 ft/s; rate x area = 9.645e-4 ft3/s
Step  dt (s)  Volume before  Volume after   Loss (ft3)  Loss/dt (ft3/s)
   1    0.50     5.0000e-05    5.0000e-05   0.0000e+00       0.0000e+00
   2   10.00     5.0000e-05    5.0000e-05   0.0000e+00       0.0000e+00
   3   10.00     5.0000e-05    5.0000e-05   0.0000e+00       0.0000e+00
PASS: the evaporation loss of a nearly empty unit is either a flow over its area or zero
NUM-14 5.3.0 patched: PASS
NUM-14 6.0.0 patched: PASS
```

## The fix

```diff
             if (Node[j].newVolume > FUDGE)
                 evapRate = area * evapRate;
+            else evapRate = 0.0;
```

This restores 5.1.013's treatment of evaporation (none from a unit holding FUDGE or less) and keeps 5.1.014's seepage change. The 6.0.0 patch starts `evap_cfs` at 0 and replaces the comment. The other reading of the manual, always multiplying by the area and letting the cap empty the unit, would also pass the test; zero is the smaller change and what the guard was written for.

What changes for users: results of models with storage evaporation change by at most 1e-4 ft³ per storage unit per routing step while a unit is nearly empty. Such differences can still move later results by more than that, as 6.0.0's note about usgs-runoff P005 shows; both engines change the same way.

Effect on other models: none of the 73 regression decks has a storage unit with an evaporation factor above 0, so none changes.
