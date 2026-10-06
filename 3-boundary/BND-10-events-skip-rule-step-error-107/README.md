# BND-10: Control rules stop between [EVENTS] and the run aborts with ERROR 107 when RULE_STEP is not a multiple of ROUTING_STEP

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | 5.2.4 and 5.3.0: no control rule is evaluated after the first step outside an event, and the run stops at the start of the next event with "ERROR 107: cannot compute a valid time step". Through the CLI the report shows no error message and no results. 6.0.0 finishes, but evaluates no rule between events: an action due at 01:00 happens when the next event starts (03:00 in the test). |
| **Reached from** | `[EVENTS]` together with `RULE_STEP` > 0 that is not a multiple of `ROUTING_STEP` (5.2.4, 5.3.0); `[EVENTS]` with any `[CONTROLS]` (6.0.0) |
| **5.3.0** | `routing_getRoutingStep()` in [`src/legacy/engine/routing.c:185`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L185); the error is raised in `execRouting()`, [`swmm5.c:966`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L966) |
| **5.2.4** | Same code: [`src/solver/routing.c:178`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L178) |
| **6.0.0** | Different defect with a similar effect: the rule-grid clamp is applied, so there is no ERROR 107, but `SWMMEngine::stepRouting()` returns between events before evaluating the control rules ([`SWMMEngine.cpp:4657`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L4657)) |
| **Since** | 5.1.013, which added `RULE_STEP` to the between-events step logic of 5.1.011/5.1.012 |
| **Fix** | 5.3.0: let the fixed step fall through to the rule-step clamp ([`BND-10_swmm530.patch`](BND-10_swmm530.patch)); 6.0.0: evaluate the control rules before the event test, as legacy does ([`BND-10_swmm600.patch`](BND-10_swmm600.patch)) |

## The problem

`[EVENTS]` lets a long continuous run route flow only in selected periods. Control rules are still evaluated outside them: legacy `routing_execute()` calls `evaluateControlRules()` on every step and only skips the flow routing between events. `RULE_STEP` limits the evaluation to a fixed grid (every 60 s, say), and `routing_getRoutingStep()` shortens each routing step so that it ends exactly on that grid.

The test deck routes flow only from 03:00 to 05:00 of a 6-hour DYNWAVE run with `ROUTING_STEP 45` and `RULE_STEP 00:01:00`. Rule R1 sets orifice OR1 to 0.2 once the simulation time exceeds 1 hour.

- **5.2.4 and 5.3.0**: R1 never acts, and the run stops at 2.9875 h (10,755 s), one step before the event, with error 107. `swmm_run` and the CLI print "There are errors", but the `.rpt` file has no error message, no control actions and no results. With `RULE_STEP 00:01:30` and `ROUTING_STEP 60` (KINWAVE) the same happens; with no `RULE_STEP`, or with a rule step that is a multiple of the routing step, the run is fine.
- **6.0.0**: the run finishes, but R1 acts at 03:00, when routing resumes, instead of 01:00. This does not depend on `RULE_STEP`: without it, 5.3.0 logs the action at 01:00:00 and 6.0.0 at 03:00:00. The setting at the start of the event is the same here, but a rule that should act and then reset between events (a time window, `TIMEOPEN`/`TIMECLOSED` premises, a gradual orifice opening) gives different results, and the Control Actions log shows the wrong time.

## Why it happens

### 5.2.4 and 5.3.0

Between events the routing step is either a jump to the next runoff or report time, or the fixed step. The fixed-step case returns at once, skipping the `RuleStep` clamp below it:

```c
// src/legacy/engine/routing.c, routing_getRoutingStep()
if ( NumEvents > 0 && BetweenEvents )
{
    ...
    else
    {
        date1 = getDateTime(NewRoutingTime + 1000.0 * fixedStep);
        if ( date1 < Event[NextEvent].start ) return fixedStep;     // skips the clamp
    }
}
...
// --- determine if control rule time interval reached
if (RuleStep > 0)
{
    nextRuleTime = NewRuleTime + 1000. * RuleStep;
    nextRoutingTime = NewRoutingTime + 1000. * routingStep;
    if (nextRoutingTime >= nextRuleTime)
        routingStep = (nextRuleTime - NewRoutingTime) / 1000.0;
}
```

The clock then runs 0, 45, 90, 135 s and never lands on 60 s. `evaluateControlRules()` only evaluates rules when `fabs(NewRoutingTime - NewRuleTime) < 1.0`, and only advances `NewRuleTime` when a step ends within 1 ms of `NewRuleTime + RuleStep`, so `NewRuleTime` stays at 0 and no rule is evaluated again. At 10,755 s the next fixed step would reach the event start (10,800 s), the early return is not taken, and the clamp runs with the stale rule time: `routingStep = (60,000 - 10,755,000) / 1000 = -10,695 s`. `execRouting()` sets `ErrorCode = ERR_TIMESTEP` and returns, without writing the message to the report.

### 6.0.0

6.0.0 sends the between-events fixed step through `TimestepController::compute_next()`, which applies the rule-grid clamp, so the clock stays on the grid. But `stepRouting()` tests for the event before it evaluates the rules and returns:

```cpp
// src/engine/core/SWMMEngine.cpp, SWMMEngine::stepRouting()
between_events_ = isBetweenEvents(routing_date);
if (between_events_) {
    ...
    return;                                   // before B1a/B1b below
}
// B1a. Evaluate pump startup/shutoff depth hysteresis ...
hydstruct_.updatePumpTargetSettings(ctx_);
// B1b. Evaluate control rules ...
controls_.evaluate(ctx_, routing_date, dt_routing, rule_time_reached);
```

Legacy evaluates the controls (and pump target settings) first, then sets `BetweenEvents` and skips only the flow routing.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-10_events-rulestep.inp`](BND-10_events-rulestep.inp) | J1 (2 cfs inflow) - orifice OR1 - J2 - conduit C1 - outfall; DYNWAVE, `ROUTING_STEP 45`, `RULE_STEP 00:01:00`, one event 03:00-05:00 in a 6-hour run; rule R1 sets OR1 to 0.2 after 1 hour |
| [`BND-10_test.c`](BND-10_test.c) | Legacy toolkit (5.2.4 and 5.3.0): steps the run, records the error code and the first time OR1's setting is 0.2 |
| [`BND-10_test6.c`](BND-10_test6.c) | The same for 6.0.0 |

```sh
tools/run-test.sh BND-10            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-10 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**:

```
---- 5.2.4 and 5.3.0 (identical) ----
Last step ended at          2.9875 h  (swmm_step returns 0 at the 6 h end)
Error code                     107
OR1 setting 0.2 from         never  (rule should act at 1.0000 h)
FAIL: the run stopped with error 107 at 2.9875 h and rule R1 never acted (due at 1 h)

---- 6.0.0 ----
Last step ended at          6.0000 h  (end is 6 h)
Error code                       0
OR1 setting 0.2 from        3.0125 h  (rule should act at 1.0000 h)
FAIL: the run finished and rule R1 acted at 3.0125 h instead of 1 h
```

**With the fix**, both engines finish and act at 01:00 (the setting is read after the step that starts at 01:00:00 and ends at 01:00:45):

```
---- 5.3.0 ----
Last step ended at          5.9958 h  (swmm_step returns 0 at the 6 h end)
Error code                       0
OR1 setting 0.2 from        1.0125 h  (rule should act at 1.0000 h)
PASS: the run finished and rule R1 acted at 1 h, between events

---- 6.0.0 ----
Last step ended at          6.0000 h  (end is 6 h)
Error code                       0
OR1 setting 0.2 from        1.0125 h  (rule should act at 1.0000 h)
PASS: the run finished and rule R1 acted at 1 h, between events
```

Both patched reports log `01/01/2020: 01:00:00 Link OR1 setting changed to 0.20 by Control R1` and a flow continuity error of -0.663 %, with the same link summary.

## The fix

5.3.0: take the fixed step without returning, so the rule-step clamp that follows applies to it (when `RULE_STEP` is 0, or the step does not cross a rule time, the result is the same as before):

```diff
             date1 = getDateTime(NewRoutingTime + 1000.0 * fixedStep);
-            if ( date1 < Event[NextEvent].start ) return fixedStep;
+            if ( date1 < Event[NextEvent].start ) routingStep = fixedStep;
```

6.0.0: move the event test and its early return, unchanged, from before the pump-target and control-rule block to after it, which is legacy's order. The 6.0.0 clock already respected the rule grid.

**Effect on other models.** Only models with `[EVENTS]` are affected; among the regression decks that is `events_example.inp`, which has no controls. Its `.out` files from the patched 5.3.0 and 6.0.0 are byte-identical to the unpatched ones.

ERROR 107 not reaching the `.rpt` file is a separate defect: `execRouting()` sets `ErrorCode` without calling `report_writeErrorMsg()`.
