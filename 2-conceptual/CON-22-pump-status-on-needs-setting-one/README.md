# CON-22: `PUMP x STATUS = ON` is false for a pump running at any speed other than 1

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | A rule premise on a pump's STATUS compares the pump's speed setting with exactly 1 (ON) or 0 (OFF). Once another rule, a curve, a time series or a PID action runs the pump at any other speed, `STATUS = ON` is false and `STATUS = OFF` is false as well, so interlock rules silently switch to their ELSE branch. In the test an interlock closes an orifice while the pump still delivers 1.3 cfs. No warning. |
| **Reached from** | `[CONTROLS]` premises `PUMP x STATUS = ON` / `= OFF` (and `<>`), for a pump whose setting is not 0 or 1 |
| **5.3.0** | `getVariableValue()` in [`src/legacy/engine/controls.c:1863`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1863) |
| **5.2.4** | Same code, [`src/solver/controls.c:1316`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/controls.c#L1316) |
| **6.0.0** | Reproduces: `ControlEngine::getVariableValue()`, `LINK_STATUS`, [`src/engine/controls/Controls.cpp:531`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/controls/Controls.cpp#L531) |
| **Since** | 5.0.012, when `PUMP x SETTING = value` actions made speeds other than 0 and 1 possible; the STATUS premise has compared the raw setting since 5.0 |
| **Fix** | Evaluate STATUS as 1 when the setting is above 0 and 0 otherwise: [`CON-22_swmm530.patch`](CON-22_swmm530.patch), [`CON-22_swmm600.patch`](CON-22_swmm600.patch) |

## The problem

A pump's setting is its speed: 0 is off, 1 is the pump curve as entered, and any other positive value scales the curve. Rule actions set it (`PUMP P1 STATUS = ON` sets 1, `PUMP P1 SETTING = 0.5` sets half speed), and so do modulated actions (`SETTING = CURVE`, `TIMESERIES`, `PID`) and the toolkit API.

A premise on the pump's status should ask whether the pump is running. It asks whether the setting is exactly 1. In the test deck, pump P1 starts ON, rule R1 slows it to half speed after 0:30 and rule R3 switches it off after 1:30. An interlock rule keeps orifice ORON open while P1 runs:

```
RULE RON
IF PUMP P1 STATUS = ON
THEN ORIFICE ORON SETTING = 1
ELSE ORIFICE ORON SETTING = 0
```

ORON closes at 0:30, one hour before P1 stops, while P1 still pumps 1.316 cfs. `IF PUMP P1 STATUS = OFF` would be false at the same time, so a pair of rules written for ON and OFF leaves the half-speed pump in neither state.

## Why it happens

The right-hand side of a STATUS premise is parsed to 0 or 1:

```c
// src/legacy/engine/controls.c, getPremiseValue()
    case r_STATUS:
        *value = findmatch(token, StatusWords);       // "OFF" = 0, "ON" = 1
        if (*value < 0.0)
            *value = findmatch(token, ConduitWords);  // "CLOSED" = 0, "OPEN" = 1
```

while the left-hand side is the raw setting:

```c
// src/legacy/engine/controls.c, getVariableValue()
    case r_STATUS:
        if (j < 0 ||
            (Link[j].type != CONDUIT && Link[j].type != PUMP))
            return MISSING;
        else
            return Link[j].setting;
```

and `compareValues()` tests `lhsValue == rhsValue` for `=`. A conduit's setting is only ever 0 or 1, so conduits are not affected; a pump's is any value from 0 up.

6.0.0 does the same in `ControlEngine::getVariableValue()` for `ConditionVar::LINK_STATUS`. Both its batched and its scalar premise paths go through that function.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-22_pump-half-speed.inp`](CON-22_pump-half-speed.inp) | Ideal pump P1 (ON at the start), rules R1 (`SETTING = 0.5` after 0:30), R3 (`STATUS = OFF` after 1:30, priority 5) and the interlock RON on orifice ORON |
| [`CON-22_test.c`](CON-22_test.c) | Reads P1's setting and flow and ORON's setting at 0:15, 1:00 and 1:45 through the legacy toolkit; ORON must be 1, 1, 0 |
| [`CON-22_test6.c`](CON-22_test6.c) | The same through the 6.0.0 API |

```sh
tools/run-test.sh CON-22            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-22 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (all three engines print the same):

```
  time   P1 setting   P1 flow (cfs)   ORON setting (expected)
  0.25 h     1.00         2.000           1.0  (1.0)
  1.00 h     0.50         1.316           0.0  (1.0)
  1.75 h     0.00         0.000           0.0  (0.0)
FAIL: 'PUMP P1 STATUS = ON' is false while P1 runs at setting 0.50 and pumps 1.316 cfs (ORON = 0.0 at 1:00)
CON-22 5.2.4 base: FAIL
CON-22 5.3.0 base: FAIL
CON-22 6.0.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
  time   P1 setting   P1 flow (cfs)   ORON setting (expected)
  0.25 h     1.00         2.000           1.0  (1.0)
  1.00 h     0.50         1.316           1.0  (1.0)
  1.75 h     0.00         0.000           0.0  (0.0)
PASS: 'PUMP P1 STATUS = ON' holds at full and half speed and not when P1 is off
CON-22 5.3.0 patched: PASS
CON-22 6.0.0 patched: PASS
```

## The fix

```diff
     case r_STATUS:
         if (j < 0 ||
             (Link[j].type != CONDUIT && Link[j].type != PUMP))
             return MISSING;
         else
-            return Link[j].setting;
+            // --- a link with any positive setting is ON/OPEN
+            return (Link[j].setting > 0.0) ? 1.0 : 0.0;
```

6.0.0 gets the same change in `ControlEngine::getVariableValue()`. A rule that needs the speed itself can still test `PUMP x SETTING`.

**Effect on other models.** `extran10.inp` has ten `PUMP ... STATUS = ON/OFF` premises, but its pumps only ever run at settings 0 and 1; its `.out` files, and those of `Type5_Pump_Test.inp` and `Example3.inp` (both with pump or regulator rules), are byte-identical with the patch in 5.3.0 and 6.0.0.
