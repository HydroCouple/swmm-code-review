# NUM-47: Losses in the first (0.5 s) dynamic-wave step are booked 10 to 30 times over

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | The routing mass balance books treatment, decay, evaporation and seepage losses with the weight (dt<sub>n</sub> + dt<sub>n+1</sub>) / (2 dt<sub>n</sub>) instead of 1. Under DYNWAVE with a variable step the first step is 0.5 s and the next is the full routing step, so a loss in step 1 is booked 10.5 times (10 s routing step) or 30.5 times (30 s). A pond whose initial TP is removed by treatment reports Mass Reacted 26.201 lb for 2.495 lb present and a continuity error of −950 %. Fast decay of an initial store gives −24.6 %. Wherever the step size changes later, the same terms are skewed on a smaller scale. |
| **Reached from** | DYNWAVE with `VARIABLE_STEP` > 0 (the default) and a treatment expression, first-order decay, evaporation or seepage acting on water present at the start; more generally any change of routing step |
| **5.3.0** | `massbal_updateRoutingTotals()` in [`src/legacy/engine/massbal.c:576`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L576), called by `routing_execute()` at [`routing.c:227`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L227) and [`routing.c:271`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L271); the rates come from [`treatmnt.c:262`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/treatmnt.c#L262), [`qualrout.c:615`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L615) and [`routing.c:951`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L951) |
| **5.2.4** | Same code: [`src/solver/massbal.c:614`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/massbal.c#L614), [`routing.c:220`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L220), [`routing.c:264`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L264) |
| **6.0.0** | Not affected: it books each step's decay as a mass, `(c1 - c2) * v_old` ([`src/engine/quality/QualityRouting.cpp:1192`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1192)), and its flow terms as rate × the step's own dt ([`src/engine/core/SWMMEngine.cpp:1658`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L1658)). Its treatment ledger has a different defect (below). |
| **Since** | 5.0 (the half-step update and the decay rate are both in the repository's first commit, 2014) |
| **Fix** | Integrate the loss terms over the step that produced them: [`NUM-47_swmm530.patch`](NUM-47_swmm530.patch) |

## The problem

A closed storage pond holds 4,000 ft³ of water with 10 mg/L of TP, 2.495 lb. Its outlet is 6 ft above the bottom, so nothing leaves. A treatment expression `TP C = 0` removes all of the TP in the first routing step. The routing step is 10 s, DYNWAVE, with the default variable step. 5.2.4 and 5.3.0 report:

```
  Mass Reacted .............        26.201
  Initial Stored Mass ......         2.495
  Final Stored Mass ........         0.000
  Continuity Error (%) .....      -950.000
```

26.201 / 2.495 = 10.50 = (0.5 + 10) / (2 × 0.5). The same deck with `VARIABLE_STEP 0` (every step 10 s) gives Mass Reacted 2.495 and 0.000 %. Nothing in the physics changed; only the length of the first step did.

Treatment is not needed. With first-order decay (K1 = 1440/day) acting on the same pond and a 30 s routing step, 5.2.4 and 5.3.0 report Mass Reacted 3.109 lb for 2.495 lb and −24.6 %; 6.0.0 reports 2.495 lb and 0.000 %.

Users who set an initial concentration in a pond or wetland and a treatment function (a common way to model a BMP) see a large negative quality continuity error and have no way to tell that the model's concentrations are right and only the ledger is wrong.

## Why it happens

`routing_execute()` adds the step totals (rates, in ft³/s or mass/s) to the run totals twice, each time over half a step: at the end of the step, and again at the start of the next one, before `massbal_initTimeStepTotals()` clears them. The start-of-step call passes the length of the new step:

```c
// src/legacy/engine/routing.c, routing_execute(routingModel, routingStep)
    // --- update mass balance totals over previous half time step
    massbal_updateRoutingTotals(routingStep/2.);     // step n's rates x dt(n+1)/2
    ...
    massbal_initTimeStepTotals();
    ...
    // --- update mass balance totals over the current half time step
    massbal_updateRoutingTotals(routingStep / 2.);   // step n+1's rates x dt(n+1)/2
```

So the rates of step n get the weight (dt<sub>n</sub> + dt<sub>n+1</sub>)/2. For a flow sampled at the end of step n (inflows, outflow, flooding) that is the trapezoidal rule, which is presumably the intent. But the loss terms are the loss of step n divided by dt<sub>n</sub>:

```c
// src/legacy/engine/treatmnt.c, treatmnt_treat()
        massLost = (Cin[p]*q*tStep + Node[j].oldQual[p]*Node[j].oldVolume -
                   cOut*(q*tStep + Node[j].oldVolume)) / tStep;
        ...
        massbal_addReactedMass(p, massLost);

// src/legacy/engine/qualrout.c, getReactedQual()
    lossRate = (c - c2) * v1 / tStep;
    massbal_addReactedMass(p, lossRate);

// src/legacy/engine/routing.c, removeStorageLosses()
    massbal_addNodeLosses(evapLoss/tStep, exfilLoss/tStep);
```

The mass booked for step n is therefore (loss in step n) × (dt<sub>n</sub> + dt<sub>n+1</sub>) / (2 dt<sub>n</sub>). With a fixed step the factor is 1. Under DYNWAVE with a variable step, `dynwave_getRoutingStep()` starts with `VariableStep = MinRouteStep` (0.5 s, [`dynwave.c:203`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dynwave.c#L203)) and then jumps to the Courant step, capped at the routing step. In the treatment deck the whole store is removed in step 1, so it is booked 10.5 times. In the decay deck the 0.5 s of decay in step 1 is booked as 30.25 s of decay. Later in a run the factor is above 1 when the step grows and below 1 when it shrinks, so the error partly cancels, but it never vanishes.

The quality seepage term (`qExfil * c1`, `qSeep * c1`) and the flow ledger's evaporation and seepage terms are built the same way and carry the same weighting.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-47_treat-initial-store.inp`](NUM-47_treat-initial-store.inp) | Closed pond SU1 (1000 ft², 4 ft, TP 10 mg/L), treatment `TP C = 0`, DYNWAVE, 10 s routing step, default variable step, 2 h |
| [`NUM-47_fast-decay.inp`](NUM-47_fast-decay.inp) | The same pond with pollutant P1, no treatment, K1 = 1440/day, 30 s routing step |
| [`NUM-47_test.c`](NUM-47_test.c) | Runs both decks through the legacy toolkit (5.2.4 and 5.3.0) and reads the Quality Routing Continuity table |
| [`NUM-47_test6.c`](NUM-47_test6.c) | Runs the decay deck through the 6.0.0 C API. The treatment deck is left out: 6.0.0 adds the treatment removal rate (mass/s) to its reacted-mass total, a separate 6.0.0 defect that would fail the check for another reason (see below) |

The test applies conservation of mass: nothing enters or leaves the pond, so Mass Reacted + Final Stored Mass must equal Initial Stored Mass. It requires that ratio within 2 % of 1 and the reported continuity error within 2 %.

```sh
tools/run-test.sh NUM-47            # 5.2.4: FAIL, 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh NUM-47 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print the same:

```
Deck                              Initial   Mass      Final     (Reacted+Final)  Reported
                                  stored    reacted   stored    / Initial        error (%)
NUM-47_treat-initial-store.inp      2.495    26.201     0.000         10.501      -950.000  <-- wrong
NUM-47_fast-decay.inp               2.495     3.109     0.000          1.246       -24.583  <-- wrong
FAIL: in 2 of 2 decks Mass Reacted exceeds the mass that was there (the first 0.5 s step is booked with the next step's length)
NUM-47 5.2.4 base: FAIL
NUM-47 5.3.0 base: FAIL
```

6.0.0 passes unpatched:

```
NUM-47_fast-decay.inp               2.495     2.495     0.000          1.000         0.000
PASS: Mass Reacted + Final Stored Mass = Initial Stored Mass
NUM-47 6.0.0 base: PASS
```

**With the fix**, 5.3.0 books exactly the mass that was removed, the same as 6.0.0 for the decay deck:

```
NUM-47_treat-initial-store.inp      2.495     2.495     0.000          1.000         0.000
NUM-47_fast-decay.inp               2.495     2.495     0.000          1.000         0.000
PASS: Mass Reacted + Final Stored Mass = Initial Stored Mass in both decks
NUM-47 5.3.0 patched: PASS
```

## The fix

Remember the half length of the step whose losses are in the step totals, and use it for the four loss terms in both updates. The first update after `massbal_initTimeStepTotals()` is the one at the end of that step, so it sets the value; the update at the start of the next step reuses it. Flows sampled at the end of a step keep the trapezoidal weights.

```diff
+static double    LossHalfStep;    // half length of the step whose loss rates
+                                  // are in StepFlowTotals & StepQualTotals
 ...
 void massbal_initTimeStepTotals()
     OldStepFlowTotals = StepFlowTotals;
+    LossHalfStep = 0.0;
 ...
 void massbal_updateRoutingTotals(double tStep)
+    if ( LossHalfStep == 0.0 ) LossHalfStep = tStep;
 ...
-    FlowTotals.evapLoss += StepFlowTotals.evapLoss * tStep;
-    FlowTotals.seepLoss += StepFlowTotals.seepLoss * tStep;
+    FlowTotals.evapLoss += StepFlowTotals.evapLoss * LossHalfStep;
+    FlowTotals.seepLoss += StepFlowTotals.seepLoss * LossHalfStep;
 ...
-        QualTotals[j].reacted  += StepQualTotals[j].reacted * tStep;
-        QualTotals[j].seepLoss += StepQualTotals[j].seepLoss * tStep;
+        QualTotals[j].reacted  += StepQualTotals[j].reacted * LossHalfStep;
+        QualTotals[j].seepLoss += StepQualTotals[j].seepLoss * LossHalfStep;
```

With a fixed step nothing changes. 6.0.0 needs no change for this issue.

Effect on other models: all 73 decks of the regression suite, run with a private patched 5.3.0 build, give byte-identical `.out` files and reports identical apart from the run-time stamps: where their routing step varies, the loss terms are small and the change is below the printed precision. The change shows where a large loss falls in a step whose neighbour has a different length, as in the decks here.

6.0.0 has a related but separate defect in its treatment ledger: `QualityRouting.cpp` ports the legacy `massLost` (a rate, mass/s) and adds it to `qual_routing_reacted`, which everywhere else accumulates mass ([`QualityRouting.cpp:1703`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1703)). On the treatment deck 6.0.0 reports Mass Reacted 4.991 lb (twice the store, since the first step is 0.5 s) and −100 %; with a fixed 10 s step it reports 0.250 lb and +90 %. It is not fixed here.
