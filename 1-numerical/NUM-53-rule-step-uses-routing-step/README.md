# NUM-53: With RULE_STEP > 0 the rules use the routing step as their time step

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | With a rule step longer than the routing step, a premise such as `SIMULATION TIME = 1.05` or `CLOCKTIME = 01:03:00` never holds unless the time falls within half a routing step of a rule time, so the rule silently never acts (it does act with `RULE_STEP 0`). A PID action integrates over one routing step per rule evaluation instead of one rule step: in the test the setting moves 19 times less than it should with 10 s routing steps and 4.5 times less with 60 s, so the controller depends on the routing step. No warning. |
| **Reached from** | `RULE_STEP` > 0 together with `=` / `<>` premises on `SIMULATION TIME`, `SIMULATION CLOCKTIME`, `TIMEOPEN` or `TIMECLOSED`, or with a `PID` action |
| **5.3.0** | `evaluateControlRules()` in [`src/legacy/engine/routing.c:383`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L383); the step is used by `evaluatePremise()` at [`controls.c:1815`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1815) and `getPIDSetting()` at [`controls.c:1666`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1666) |
| **5.2.4** | Same code, [`src/solver/routing.c:281`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L281) |
| **6.0.0** | Reproduces: `SWMMEngine::stepRouting()` passes `dt_routing`, [`src/engine/core/SWMMEngine.cpp:4696`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L4696), used for the half-step window and the PID at [`Controls.cpp:224`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/controls/Controls.cpp#L224) |
| **Since** | 5.1.013, when `RULE_STEP` was added |
| **Fix** | Pass the rule step when `RULE_STEP` > 0: [`NUM-53_swmm530.patch`](NUM-53_swmm530.patch), [`NUM-53_swmm600.patch`](NUM-53_swmm600.patch) |

## The problem

`RULE_STEP` lets a model evaluate its control rules less often than it routes flow, for example every 5 minutes with a 30 s routing step. Two parts of the rule evaluation depend on how much time one evaluation stands for, and both are given the routing step instead of the rule step.

**Time premises with `=`.** A premise like `IF SIMULATION TIME = 1.05` cannot ask for exact equality, so SWMM treats it as true when the current time is within half a time step of 1.05 h. With `RULE_STEP 0` that window is one routing step wide and exactly one evaluation falls in it. With `RULE_STEP 00:05:00` the rules run at 01:00 and 01:05, but the window is still 01:03 +- 15 s, so no evaluation falls in it and the rule never acts. In the test, `SIMULATION TIME = 1.05` and `SIMULATION CLOCKTIME = 02:03:00` never fire in 5.2.4, 5.3.0 or 6.0.0. The same deck with `RULE_STEP 0` logs both actions, at 01:03:00 and 02:03:00, in 5.3.0 and 6.0.0. `TIMEOPEN` and `TIMECLOSED` premises with `=` or `<>` use the same window.

**PID actions.** The integral term adds `e*dt/ki` and the derivative term divides by `dt`, where `dt` should be the time since the controller last acted: one rule step. It is the routing step. With `RULE_STEP 00:05:00`, `PID 0.002 0.1 0` and a constant error of -1, the setting should drop by `kp*e*(10 min)/ki = 0.2` over two rule intervals. It drops by 0.0107 with 10 s routing steps and by 0.0440 with 60 s. The controller's tuning thus depends on the routing step, a numerical parameter the user may change for stability reasons.

## Why it happens

```c
// src/legacy/engine/routing.c, evaluateControlRules()
    // --- evaluate control rules if next evaluation time reached
    if (RuleStep == 0 || fabs(NewRoutingTime - NewRuleTime) < 1.0)
    {  
        controls_evaluate(currentDate, currentDate - StartDateTime,
            routingStep / SECperDAY);
    }
```

The `if` runs the rules once per rule step, but `tStep` is always the routing step. `controls_evaluate()` hands it to every premise and action:

```c
// src/legacy/engine/controls.c, evaluatePremise()
    case r_TIME:
    case r_CLOCKTIME:
        return compareTimes(lhsValue, p->relation, rhsValue, tStep / 2.0);

// compareTimes(), relation EQ
        if (lhsValue >= rhsValue - halfStep && lhsValue < rhsValue + halfStep)
            return TRUE;

// getPIDSetting()
    dt *= 1440.0;                       // tStep in minutes
    ...
        i = e0 * dt / a->ki;
    d = a->kd * (e0 - 2.0 * a->e1 + a->e2) / dt;
```

6.0.0 does the same: `stepRouting()` computes `rule_time_reached` from the rule clock and then calls `controls_.evaluate(ctx_, routing_date, dt_routing, rule_time_reached)`.

When `RULE_STEP` is shorter than the routing step, `routing_getRoutingStep()` cuts each routing step to end on the next rule time, so the rules still run once per rule step. In every case one evaluation stands for one rule step.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-53_time-eq.inp`](NUM-53_time-eq.inp) | `RULE_STEP 00:05:00`, 30 s routing. Rule TEQ: `SIMULATION TIME = 1.05` sets OR1 to 0.2; rule CEQ: `SIMULATION CLOCKTIME = 02:03:00` sets it to 0.5 |
| [`NUM-53_pid-rt10.inp`](NUM-53_pid-rt10.inp) | Isolated storage SU1 held at 2 ft, rule `IF NODE SU1 DEPTH <> 1 THEN ORIFICE OR1 SETTING = PID 0.002 0.1 0`, `RULE_STEP 00:05:00`, 10 s routing |
| [`NUM-53_pid-rt60.inp`](NUM-53_pid-rt60.inp) | The same with 60 s routing |
| [`NUM-53_test.c`](NUM-53_test.c) | Checks that each time rule acts within one rule step of its time, and that OR1's setting drops by 0.2 (within 5 %) between 12 and 22 min for both routing steps |
| [`NUM-53_test6.c`](NUM-53_test6.c) | The same through the 6.0.0 API |

```sh
tools/run-test.sh NUM-53            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-53 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (all three engines print the same):

```
RULE_STEP 5 min, ROUTING_STEP 30 s
  TIME = 1.05 (01:03):        OR1 -> 0.2 at never   (due by 63 + 5 + 0.5 min)
  CLOCKTIME = 02:03:00:       OR1 -> 0.5 at never   (due by 123 + 5 + 0.5 min)
PID 0.002 0.1 0, e = -1, RULE_STEP 5 min    setting at 12 min   22 min   change
  ROUTING_STEP 10 s                          0.9840         0.9733   -0.0107
  ROUTING_STEP 60 s                          0.9340         0.8900   -0.0440
  expected change kp*e*(10 min)/ki                                 -0.2000
FAIL: the time premises with '=' did not act within one rule step (OR1 -> 0.2 at -1.0 min, -> 0.5 at -1.0 min; -1 = never); the PID integral over 10 min is -0.0107 (10 s steps) and -0.0440 (60 s steps) instead of -0.2000;
NUM-53 5.2.4 base: FAIL
NUM-53 5.3.0 base: FAIL
NUM-53 6.0.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
RULE_STEP 5 min, ROUTING_STEP 30 s
  TIME = 1.05 (01:03):        OR1 -> 0.2 at 65.5 min   (due by 63 + 5 + 0.5 min)
  CLOCKTIME = 02:03:00:       OR1 -> 0.5 at 125.5 min   (due by 123 + 5 + 0.5 min)
PID 0.002 0.1 0, e = -1, RULE_STEP 5 min    setting at 12 min   22 min   change
  ROUTING_STEP 10 s                          0.6940         0.4900   -0.2040
  ROUTING_STEP 60 s                          0.6940         0.4900   -0.2040
  expected change kp*e*(10 min)/ki                                 -0.2000
PASS: '=' time premises act once per rule step and the PID integrates over the rule interval, independent of the routing step
NUM-53 5.3.0 patched: PASS
NUM-53 6.0.0 patched: PASS
```

The rules now act at the 01:05 and 02:05 evaluations (the setting is read at the end of the routing step that follows). The extra -0.004 in the PID change is [NUM-52](../NUM-52-pid-steady-error-reset/): its error reset adds `kp*e = -0.002` at each evaluation. With both patches applied the change is -0.2000 for both routing steps.

## The fix

```diff
     // --- evaluate control rules if next evaluation time reached
+    //     (an evaluation covers one rule step when RuleStep > 0)
     if (RuleStep == 0 || fabs(NewRoutingTime - NewRuleTime) < 1.0)
     {  
         controls_evaluate(currentDate, currentDate - StartDateTime,
-            routingStep / SECperDAY);
+            (RuleStep > 0 ? RuleStep : routingStep) / SECperDAY);
     }
```

6.0.0 passes `ctx_.options.rule_step` instead of `dt_routing` to `ControlEngine::evaluate()` when it is positive. Nothing changes for `RULE_STEP 0`, the default.

**Effect on other models.** Of the regression decks with control rules, only `control_rules_test.inp` sets a rule step (5 min); its rules use no time premise or PID and its `.out` files are byte-identical with the patch in both engines. The PID level-control deck from NUM-52 (storage unit, bottom orifice, `PID -0.5 5 0` holding 4 ft) run with `RULE_STEP 00:01:00`:

| Routing step | max depth, unpatched | patched | depth at 3 h, unpatched | patched | mean \|depth - 4\|, unpatched | patched |
|---|---|---|---|---|---|---|
| 5 s | 5.257 | 4.385 | 3.467 | 3.866 | 0.859 | 0.376 |
| 10 s | 5.208 | 4.389 | 3.345 | 3.898 | 0.721 | 0.377 |
| 20 s | 4.785 | 4.394 | 3.414 | 3.877 | 0.662 | 0.386 |

Unpatched, the controller's integral action is 1/12 to 1/3 of what its gains ask for, the level overshoots by up to 1.26 ft and the result depends on the routing step; patched, the three runs agree to within 0.04 ft. 5.3.0 and 6.0.0 give identical results; the flow-routing continuity error is -0.054 % against -0.053 % (10 s step).
