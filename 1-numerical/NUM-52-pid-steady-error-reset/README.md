# NUM-52: A PID controller with a steady error keeps integrating through its P and D terms

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Any PID rule whose error stops changing (link at a limit, set point out of reach, steady offset) moves its link by `kp*e` at every rule evaluation, so a proportional-only controller winds its orifice, weir or pump to 0 or 1, at a rate set by the routing step. In the test a P-only controller closes an orifice in 5 min with 30 s steps and in 10 min with 60 s steps instead of holding 0.9; a PI controller integrates 21 times faster than `kp*e/ki`. Modulated settings are not written to the control log, so nothing shows it. |
| **Reached from** | `[CONTROLS]` actions `... SETTING = PID kp ki kd` |
| **5.3.0** | `getPIDSetting()` in [`src/legacy/engine/controls.c:1679`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1679) |
| **5.2.4** | Same code, [`src/solver/controls.c:1140`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/controls.c#L1140) |
| **6.0.0** | Reproduces: `ControlEngine::computePIDSetting()` copies the reset and the dead band, [`src/engine/controls/Controls.cpp:653`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/controls/Controls.cpp#L653) |
| **Since** | 5.0.012 (2008), when PID controls were added |
| **Fix** | Remove the error reset and the per-update dead band: [`NUM-52_swmm530.patch`](NUM-52_swmm530.patch), [`NUM-52_swmm600.patch`](NUM-52_swmm600.patch) |

## The problem

SWMM's PID action uses the velocity (incremental) form of the controller. At each rule evaluation the setting changes by

```
update = kp * [ (e0 - e1) + e0*dt/ki + kd*(e0 - 2*e1 + e2)/dt ]
```

where `e0`, `e1` and `e2` are the relative errors `(SetPoint - value)/SetPoint` now, one evaluation ago and two evaluations ago, and `dt` is the time step in minutes. With a constant error `e`, the proportional and derivative terms are zero after the first evaluation, and only the integral term moves the setting, at `kp*e/ki` per minute. A proportional-only controller (`ki = 0`) moves once, to `1 + kp*e`, and stays there: the steady offset that every P controller has.

SWMM does something else when the error stops changing. It treats the controller as "stuck" and resets `e1` and `e2` to zero, so the next update uses `e0 - 0 = e0` and adds `kp*e0` again, at every evaluation. The proportional term becomes a second integrator with a rate of `kp*e0` **per evaluation**, and the derivative term adds `kp*kd*e0/dt` per evaluation, which is large for short steps.

In the test, node SU1 is held at 2 ft and the rule asks for 1 ft, so `e = -1` at every evaluation. `ORIFICE OR1 SETTING = PID 0.1 0 0` should set OR1 to 0.9 and hold it. Instead OR1 goes 0.9, 0.8, 0.7, ... and is fully closed after 10 evaluations: 5 minutes with a 30 s routing step, 10 minutes with 60 s. A PI controller (`PID 0.1 10 0`) facing a steady error of -0.01 should move OR1 at -0.0001 per minute; it moves at -0.0021 per minute with 30 s steps.

For a real controller the effect depends on how often its error stalls. In a level-control loop on a storage unit (side-effect check below), the depth at 3 h changes from 3.876 to 3.992 ft between a 5 s and a 10 s routing step; with the fix it is 3.804 ft for 5, 10 and 20 s steps.

## Why it happens

```c
// src/legacy/engine/controls.c, getPIDSetting()
    double tolerance = 0.0001;
    ...
    // --- reset previous errors to 0 if controller gets stuck
    if (fabs(e0 - a->e1) < tolerance)
    {
        a->e2 = 0.0;
        a->e1 = 0.0;
    }

    // --- use the recursive form of the PID controller equation to
    //     determine the new setting for the controlled link
    p = (e0 - a->e1);
    if (a->ki == 0.0)
        i = 0.0;
    else
        i = e0 * dt / a->ki;
    d = a->kd * (e0 - 2.0 * a->e1 + a->e2) / dt;
    update = a->kp * (p + i + d);
    if (fabs(update) < tolerance)
        update = 0.0;
    setting = Link[a->link].targetSetting + update;

    // --- update previous errors
    a->e2 = a->e1;
    a->e1 = e0;
```

`e1` is set back to `e0` at the end, so with a steady error the reset fires again at the next evaluation, and at every one after it.

The reset works around the dead band two lines further down. An update smaller than 0.0001 is discarded, but `e1` and `e2` are still advanced, so in the velocity form that increment is lost for good. A PI controller with a small steady error produces integral increments below 0.0001 (in the test, `0.1 * 0.01 * 0.5/10 = 0.00005` per 30 s step) and would never remove the error; the reset hides this by adding the whole proportional term again. Removing only the reset exposes the stall: with the dead band kept, the PI run in the test stays at 0.99895 for the whole run.

6.0.0 has the same code, with the reset labelled "Anti-windup". It is not anti-windup: the velocity form cannot wind up, because each update starts from the clamped `targetSetting`.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-52_p-rs30.inp`](NUM-52_p-rs30.inp) | Isolated storage unit SU1 held at 2 ft; rule `IF NODE SU1 DEPTH <> 1 THEN ORIFICE OR1 SETTING = PID 0.1 0 0`; 30 s routing step |
| [`NUM-52_p-rs60.inp`](NUM-52_p-rs60.inp) | The same with a 60 s routing step |
| [`NUM-52_pi-small-error.inp`](NUM-52_pi-small-error.inp) | SU1 held at 1.01 ft, `PID 0.1 10 0`, 30 s step |
| [`NUM-52_test.c`](NUM-52_test.c) | Steps the three decks through the legacy toolkit and reads OR1's setting after each step: the P runs must hold 0.9 (within 0.001) and the PI run must move at `kp*e/ki = -0.0001` per minute (within 20 %) |
| [`NUM-52_test6.c`](NUM-52_test6.c) | The same through the 6.0.0 API |

```sh
tools/run-test.sh NUM-52            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-52 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0 print the same numbers; 6.0.0 too, except that its last reading is at 30 min instead of 29.5 min, so the PI minimum is 0.93700):

```
OR1 setting at                1 min    2 min    5 min   10 min   20 min      min
P 0.1, e=-1, step 30 s      0.80000  0.60000  0.00000  0.00000  0.00000  0.00000
P 0.1, e=-1, step 60 s      0.90000  0.80000  0.50000  0.00000  0.00000  0.00000
PI 0.1 10, e=-0.01, 30 s    0.99790  0.99580  0.98950  0.97900  0.95800  0.93805
P runs: largest |setting - 0.9| at any step: 0.90000 (30 s), 0.90000 (60 s)
PI run: slope 10-20 min -0.002100 per min, expected kp*e/ki = -0.000100 per min
FAIL: the P-only controller drives OR1 to 0.000 (30 s) and 0.000 (60 s) instead of holding 0.9; the PI controller moves -0.002100 per min instead of -0.000100;
NUM-52 5.2.4 base: FAIL
NUM-52 5.3.0 base: FAIL
NUM-52 6.0.0 base: FAIL
```

**With the fix** (5.3.0; 6.0.0 prints the same values, with a PI minimum of 0.99600 at 30 min):

```
OR1 setting at                1 min    2 min    5 min   10 min   20 min      min
P 0.1, e=-1, step 30 s      0.90000  0.90000  0.90000  0.90000  0.90000  0.90000
P 0.1, e=-1, step 60 s      0.90000  0.90000  0.90000  0.90000  0.90000  0.90000
PI 0.1 10, e=-0.01, 30 s    0.99890  0.99880  0.99850  0.99800  0.99700  0.99605
P runs: largest |setting - 0.9| at any step: 0.00000 (30 s), 0.00000 (60 s)
PI run: slope 10-20 min -0.000100 per min, expected kp*e/ki = -0.000100 per min
PASS: with a constant error the P controller holds 0.9 and the PI controller integrates at kp*e/ki, for both routing steps
NUM-52 5.3.0 patched: PASS
NUM-52 6.0.0 patched: PASS
```

A version of the patch that removed only the reset gave `PI run: slope 10-20 min 0.000000 per min` (OR1 stuck at 0.99895), which is why the dead band goes too.

## The fix

Drop the reset and the dead band; the controller then follows its equation at every evaluation:

```diff
     double p, i, d, update;
-    double tolerance = 0.0001;
 ...
-    // --- reset previous errors to 0 if controller gets stuck
-    if (fabs(e0 - a->e1) < tolerance)
-    {
-        a->e2 = 0.0;
-        a->e1 = 0.0;
-    }
-
 ...
     update = a->kp * (p + i + d);
-    if (fabs(update) < tolerance)
-        update = 0.0;
     setting = Link[a->link].targetSetting + update;
```

The 6.0.0 patch removes the same lines from `ControlEngine::computePIDSetting()`, and both patched engines give the same settings.

**What changes for users.** PID gains now mean what the equation says: `kp` is a proportional gain, not partly an integral gain applied once per routing step, and results no longer depend on the routing step through the controller. Models tuned against the old behaviour will respond more gently when their error stalls; a P-only controller now keeps its steady offset, as P controllers do.

**Effect on other models.** None of the regression decks uses a PID action; `control_rules_test.inp` and `gate_control_2.inp` give byte-identical `.out` files with the patch. A storage unit with a 2 to 20 cfs inflow hydrograph and a bottom orifice under `PID -0.5 5 0` holding 4 ft (6 h run):

| Routing step | depth at 3 h, unpatched | patched | max depth, unpatched | patched | mean \|depth - 4\|, unpatched | patched |
|---|---|---|---|---|---|---|
| 5 s | 3.876 | 3.804 | 4.340 | 4.393 | 0.333 | 0.390 |
| 10 s | 3.992 | 3.804 | 4.359 | 4.395 | 0.338 | 0.393 |
| 20 s | 3.981 | 3.803 | 4.399 | 4.399 | 0.347 | 0.398 |

Unpatched, the result depends on the routing step; patched, it does not. With the user's gains the patched loop tracks a little less tightly (the hidden extra integral action is gone) and ends exactly on the set point (4.000 ft against 4.013 ft). Flow-routing continuity error: -0.053 % against -0.051 % (10 s step). At 10 s, 5.3.0 and 6.0.0 give the same numbers, unpatched and patched.
