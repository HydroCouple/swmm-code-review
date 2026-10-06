# NUM-46: Pollutant left behind when a node or link dries out is counted twice in Final Stored Mass

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Every dry-out of a storage unit or conduit adds a second copy of the mass it held to Final Stored Mass. A pond that evaporates to dryness reports 2.490 lb stored at the end against 1.248 lb at the start, and a quality continuity error of −99.6 %, when nothing left it. In real decks the error is smaller but systematic: EPA's `events_example.inp` reports 0.052 lb instead of 0.026 lb. |
| **Reached from** | Any run with pollutants in which a storage unit or conduit dries out while holding pollutant, most visibly a pond drying by evaporation |
| **5.3.0** | `massbal_addToFinalStorage()` in [`src/legacy/engine/massbal.c:520`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L520), folded in by `massbal_updateRoutingTotals()` at [`massbal.c:578`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L578), which `routing_execute()` calls at [`routing.c:227`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L227) and [`routing.c:271`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L271) |
| **5.2.4** | Same code: [`src/solver/massbal.c:554`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/massbal.c#L554), [`massbal.c:616`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/massbal.c#L616), [`routing.c:220`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L220) and [`routing.c:264`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L264) |
| **6.0.0** | Not affected: the residue goes once into a run total, `qual_routing_final_dry` ([`src/engine/quality/QualityRouting.cpp:1223`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1223)) |
| **Since** | 5.1.010, which added the fold of the step's dry-out residue into the run total (5.1.008 had booked it into the step totals only) |
| **Fix** | Add the residue to the run total directly: [`NUM-46_swmm530.patch`](NUM-46_swmm530.patch) |

## The problem

A closed storage pond, 10,000 ft² with 0.02 ft of water (200 ft³) at 100 mg/L, holds 1.248 lb of pollutant. Its outlet is 6 ft above the bottom, and it loses its water to evaporation (1 in/day) in about 5.8 hours. Evaporation takes water, not pollutant, so at the end of the run all 1.248 lb should be in Final Stored Mass. 5.2.4 and 5.3.0 report:

```
  Initial Stored Mass ......         1.248
  Final Stored Mass ........         2.490
  Continuity Error (%) .....       -99.559
```

Final Stored Mass is 1.995 times the mass that exists. A user sees a −99.6 % quality continuity error for a model with no inflow, no outflow and no reactions, and nothing points to the cause. 6.0.0 reports 1.245 lb and +0.221 %.

The same happens, on a smaller scale, every time a conduit or storage unit dries with pollutant in it. In EPA's `events_example.inp` (regression suite) Final Stored Mass is 0.052 lb in 5.3.0 and 0.026 lb in 6.0.0 and in the patched 5.3.0: the whole of it is dry-out residue, counted twice. Because the phantom copy adds to the outflow side of the balance, it also hides real losses.

## Why it happens

When a conduit or a storage unit dries, `qualrout.c` sets its concentration to 0 and books the mass it still holds as stored mass. Under evaporation that mass is the pond's whole pollutant load, because the concentration has been scaled up by the evaporation factor at every step:

```c
// src/legacy/engine/qualrout.c, findStorageQual()
        // --- set concen. to zero if remaining volume & inflow is negligible
        if ((Node[j].newVolume <= ZeroVolume ||
             Node[j].newDepth <= ZeroDepth) &&
            qIn <= ZERO)
        {
            massbal_addToFinalStorage(p, c2 * Node[j].newVolume);
            c2 = 0.0;
        }
```

(`findLinkQual()` does the same with `c2 * v2` for a conduit.) The value is a mass, and it goes into the step totals:

```c
// src/legacy/engine/massbal.c
void massbal_addToFinalStorage(int pollutIndex, double w)
{
    if ( pollutIndex < 0 || pollutIndex >= Nobjects[POLLUT] ) return;
    StepQualTotals[pollutIndex].finalStorage += w;
}
```

The step totals are rates, which `massbal_updateRoutingTotals()` integrates over half a step at a time; the stored mass is added with no time factor:

```c
// src/legacy/engine/massbal.c, massbal_updateRoutingTotals()
        QualTotals[j].reacted  += StepQualTotals[j].reacted * tStep;
        QualTotals[j].seepLoss += StepQualTotals[j].seepLoss * tStep;
        QualTotals[j].finalStorage += StepQualTotals[j].finalStorage;
```

`routing_execute()` calls that function twice for each step's totals: at the end of the step, and again at the start of the next one, before `massbal_initTimeStepTotals()` clears them:

```c
// src/legacy/engine/routing.c, routing_execute()
    // --- update mass balance totals over previous half time step
    massbal_updateRoutingTotals(routingStep/2.);
    ...
    massbal_initTimeStepTotals();
    ...
    // --- update mass balance totals over the current half time step
    massbal_updateRoutingTotals(routingStep / 2.);
```

For a rate, the two halves add up to one step. For the mass booked by `massbal_addToFinalStorage()` they add up to two copies. Only a dry-out in the very last step is counted once.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-46_pond-dries-by-evaporation.inp`](NUM-46_pond-dries-by-evaporation.inp) | The closed pond above: storage SU1 (10,000 ft², 0.02 ft, P1 100 mg/L), outlet 6 ft up, evaporation 1 in/day, DYNWAVE with a fixed 10 s step, 12 h |
| [`NUM-46_test.c`](NUM-46_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0), checks that the pond dried, and reads the Quality Routing Continuity table |
| [`NUM-46_test6.c`](NUM-46_test6.c) | The same through the 6.0.0 C API |

The test applies conservation of mass: nothing enters or leaves the pond, so Final Stored Mass must equal Initial Stored Mass. It requires the ratio within 2 % of 1 and the reported continuity error within 2 % (a correct run gives 0.998 and +0.2 %; the bug gives 1.995 and −99.6 %).

```sh
tools/run-test.sh NUM-46            # 5.2.4: FAIL, 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh NUM-46 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print the same:

```
Pond volume, start / end (ft3) .....    200.000 / 0.000
Initial Stored Mass (lb) ...........      1.248
Final Stored Mass (lb) .............      2.490
Final / Initial ....................      1.995
Reported continuity error (%) ......    -99.559
FAIL: nothing left the pond, but Final Stored Mass is 2.490 lb for 1.248 lb initially (x1.995) and the continuity error is -99.559 %
NUM-46 5.2.4 base: FAIL
NUM-46 5.3.0 base: FAIL
```

6.0.0 passes unpatched:

```
Pond volume, start / end (ft3) .....    200.000 / 0.000
Initial Stored Mass (lb) ...........      1.248
Final Stored Mass (lb) .............      1.245
Final / Initial ....................      0.998
Reported continuity error (%) ......      0.221
PASS: the dried-out pollutant mass is booked once (Final Stored = Initial Stored)
NUM-46 6.0.0 base: PASS
```

**With the fix**, 5.3.0 gives the same numbers as 6.0.0:

```
Final Stored Mass (lb) .............      1.245
Final / Initial ....................      0.998
Reported continuity error (%) ......      0.221
PASS: the dried-out pollutant mass is booked once (Final Stored = Initial Stored)
NUM-46 5.3.0 patched: PASS
```

The remaining 0.003 lb (0.2 %) shortfall is the same in both engines and is not part of this issue.

## The fix

Add the residue to the run total, where it is needed, instead of to the step totals:

```diff
 void massbal_addToFinalStorage(int pollutIndex, double w)
 {
     if ( pollutIndex < 0 || pollutIndex >= Nobjects[POLLUT] ) return;
-    StepQualTotals[pollutIndex].finalStorage += w;
+    // --- w is a mass, not a rate: book it once in the run total
+    //     (massbal_updateRoutingTotals runs twice per step's totals)
+    QualTotals[pollutIndex].finalStorage += w;
 }
```

`StepQualTotals[].finalStorage` then stays 0, so the fold in `massbal_updateRoutingTotals()` adds nothing and is left as it is. 6.0.0 needs no change.

Effect on other models, with a private patched 5.3.0 build: the `.out` files of EPA's Example1, `events_example.inp`, the parity deck `steady_quality.inp` and the 6.0.0 `site_drainage_example.inp` are byte-identical; only the continuity table changes. In `events_example.inp` Final Stored Mass goes from 0.052 to 0.026 lb (continuity error 0.096 → 0.098 %), and in `site_drainage_example.inp` from 0.096 to 0.050 lb (4.059 → 4.064 %). Both new values equal 6.0.0's. Example1 and `steady_quality.inp` do not change.
