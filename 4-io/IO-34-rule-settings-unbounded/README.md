# IO-34: Rule actions give links settings outside their valid range

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | `PUMP P1 SETTING = -1` is accepted and runs the pump backwards (-5 cfs in the test). A `CURVE` or `TIMESERIES` action whose values stray outside 0..1 (a unit slip, a curve extrapolated past its end) gives an outlet three times its rating flow, or reverses it, and a pump a negative speed. Modulated actions are not written to the control log, so nothing shows it. 6.0.0 also accepts a numeric regulator setting such as `OUTLET OL1 SETTING = 3`, which 5.2.4 and 5.3.0 reject. |
| **Reached from** | `[CONTROLS]` actions `PUMP x SETTING = <negative number>`, and `SETTING = CURVE c` / `SETTING = TIMESERIES ts` on any pump, orifice, weir or outlet; in 6.0.0 also `ORIFICE`/`WEIR`/`OUTLET x SETTING = <number outside 0..1>` |
| **5.3.0** | `addAction()` in [`src/legacy/engine/controls.c:1490`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1490) checks only regulator numbers; `updateActionValue()` at [`controls.c:1635`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1635) uses curve and series values as they come |
| **5.2.4** | Same code, [`src/solver/controls.c:983`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/controls.c#L983) and [`:1097`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/controls.c#L1097) |
| **6.0.0** | Reproduces, and accepts out-of-range numeric regulator settings too: `ControlEngine::updateActionValue()` in [`src/engine/controls/Controls.cpp:399`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/controls/Controls.cpp#L399) and the action parser at [`Controls.cpp:1448`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/controls/Controls.cpp#L1448) |
| **Since** | 5.0.012, when SETTING actions, curves and time series were added to rules |
| **Fix** | Reject negative numeric pump settings; clamp modulated values as the PID branch does: [`IO-34_swmm530.patch`](IO-34_swmm530.patch), [`IO-34_swmm600.patch`](IO-34_swmm600.patch) |

## The problem

A pump's setting is its speed: 0 is off, 1 the pump curve as entered, and there is no upper limit. An orifice's, weir's or outlet's setting is the fraction it is open, 0 to 1. SWMM enforces this in some places and not in others:

| Source of the setting | Orifice, weir, outlet | Pump |
|---|---|---|
| Numeric rule action | rejected outside 0..1 (ERROR 211) | **any value accepted** |
| `CURVE` / `TIMESERIES` action | **used as is** | **used as is** |
| `PID` action | clamped to 0..1 | clamped to >= 0 |
| Toolkit `swmm_setValue()` | negative rejected, above 1 cut to 1 | negative rejected |

The flow routines multiply by the raw setting (`return qIn * Link[j].setting;` for an ideal pump, `return dir * Link[j].setting * outlet_getFlow(k, head);` for an outlet), so a setting of 3 triples the outlet's rating curve and a negative one reverses the flow.

In the test, outlet OL1 (`q = 1.0*h^0.5`) follows a series that is 1, then 3 from 1:01, then -0.5 from 2:01, and an ideal pump P1 follows one that is 1, then -1. At 1:30 OL1 carries 4.243 cfs, three times its rating at that head; at 2:30 it carries -0.707 cfs and P1 pumps -5.000 cfs back up into its wet well. A separate deck with `PUMP P1 SETTING = -1` is accepted, while the same rule with `OUTLET OL1 SETTING = 3` is rejected in 5.2.4 and 5.3.0. 6.0.0 accepts both, and in its run OL1's flow stays at 0 under the -0.5 setting while P1 still pumps -5 cfs.

## Why it happens

```c
// src/legacy/engine/controls.c, addAction()
    else if (obj == r_PUMP)
    {
        ...
        else if (attrib == r_SETTING)
        {
            err = setActionSetting(tok, nToks, &curve, &tseries,
                                   &attrib, values);
            if (err > 0)
                return err;
        }                                       // no range check
        ...
    }

    else if (obj == r_ORIFICE || obj == r_WEIR || obj == r_OUTLET)
    {
        if (attrib == r_SETTING)
        {
            ...
            if (attrib == r_SETTING && (values[0] < 0.0 || values[0] > 1.0))
                return error_setInpError(ERR_NUMBER, tok[5]);
```

```c
// src/legacy/engine/controls.c, updateActionValue()
    if (a->curve >= 0)
    {
        a->value = table_lookup(&Curve[a->curve], ControlValue);
    }
    else if (a->tseries >= 0)
    {
        a->value = table_tseriesLookup(&Tseries[a->tseries], currentTime, TRUE);
    }
    else if (a->attribute == r_PID)
    {
        a->value = getPIDSetting(a, dt);        // the only branch that clamps
    }
```

`executeActionList()` then copies `a->value` into `Link[].targetSetting`. 6.0.0's parser converts a numeric action with `tryParseDouble()` and no range check at all, and its `updateActionValue()` mirrors the legacy curve and series branches.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-34_modulated.inp`](IO-34_modulated.inp) | Outlet OL1 under `SETTING = TIMESERIES TSSET` (1, 3, -0.5) and ideal pump P1 under `SETTING = TIMESERIES TSPUMP` (1, -1), 3 hours |
| [`IO-34_pump-minus-one.inp`](IO-34_pump-minus-one.inp) | The same network with the rule `THEN PUMP P1 SETTING = -1` |
| [`IO-34_outlet-three.inp`](IO-34_outlet-three.inp) | The same network with the rule `THEN OUTLET OL1 SETTING = 3` |
| [`IO-34_test.c`](IO-34_test.c) | Runs the first deck through the legacy toolkit and checks at every step that OL1's setting is within 0..1, P1's is >= 0 and neither link carries negative flow; opens the other two and requires both to be rejected with ERROR 211 |
| [`IO-34_test6.c`](IO-34_test6.c) | The same through the 6.0.0 API |

```sh
tools/run-test.sh IO-34            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-34 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0):

```
  time   OL1 setting  OL1 flow (cfs)   P1 setting  P1 flow (cfs)
  0.5 h       1.00         1.414          1.00         1.000
  1.5 h       3.00         4.243          1.00         1.000
  2.5 h      -0.50        -0.707         -1.00        -5.000
  over the run: OL1 setting -0.50 to 3.00, P1 setting >= -1.00, OL1 flow >= -0.707, P1 flow >= -5.000
  PUMP P1 SETTING = -1:   accepted (error code 0)
  OUTLET OL1 SETTING = 3: rejected with ERROR 211 (error code 200)
FAIL: settings outside the valid range are used: OL1 at -0.50..3.00; P1 at -1.00; reverse flow (OL1 -0.707, P1 -5.000 cfs); PUMP SETTING = -1 accepted;
IO-34 5.2.4 base: FAIL
IO-34 5.3.0 base: FAIL
```

6.0.0:

```
  2.5 h      -0.50         0.000         -1.00        -5.000
  over the run: OL1 setting -0.50 to 3.00, P1 setting >= -1.00, OL1 flow >= 0.000, P1 flow >= -5.000
  PUMP P1 SETTING = -1:   accepted (error code 0)
  OUTLET OL1 SETTING = 3: accepted (error code 0)
FAIL: settings outside the valid range are used: OL1 at -0.50..3.00; P1 at -1.00; reverse flow (OL1 0.000, P1 -5.000 cfs); PUMP SETTING = -1 accepted; OUTLET SETTING = 3 accepted;
IO-34 6.0.0 base: FAIL
```

**With the fix** (5.3.0; 6.0.0 prints the same values, with error code 5 from `swmm_engine_initialize()` instead of 200):

```
  time   OL1 setting  OL1 flow (cfs)   P1 setting  P1 flow (cfs)
  0.5 h       1.00         1.414          1.00         1.000
  1.5 h       1.00         1.414          1.00         1.000
  2.5 h       0.00         0.000          0.00         0.000
  over the run: OL1 setting 0.00 to 1.00, P1 setting >= 0.00, OL1 flow >= 0.000, P1 flow >= 0.000
  PUMP P1 SETTING = -1:   rejected with ERROR 211 (error code 200)
  OUTLET OL1 SETTING = 3: rejected with ERROR 211 (error code 200)
PASS: modulated settings stay in range and out-of-range numeric settings are rejected
IO-34 5.3.0 patched: PASS
IO-34 6.0.0 patched: PASS
```

## The fix

5.3.0: reject a negative numeric pump setting as a regulator's out-of-range number is rejected, and clamp the value of every action after it is updated, with the limits `getPIDSetting()` uses:

```diff
         else if (attrib == r_SETTING)
         {
             err = setActionSetting(tok, nToks, &curve, &tseries,
                                    &attrib, values);
             if (err > 0)
                 return err;
+            if (attrib == r_SETTING && values[0] < 0.0)
+                return error_setInpError(ERR_NUMBER, tok[5]);
         }
 ...
     else if (a->attribute == r_PID)
     {
         a->value = getPIDSetting(a, dt);
     }
+
+    // --- keep a curve or time series setting within its feasible
+    //     range, as getPIDSetting() does
+    if (a->value < 0.0)
+        a->value = 0.0;
+    if (Link[a->link].type != PUMP && a->value > 1.0)
+        a->value = 1.0;
```

Numeric, STATUS and PID values are already in range when they reach the clamp, so only curve and time-series values change. 6.0.0 gets the same clamp in `ControlEngine::updateActionValue()`, and its parser now rejects a numeric `SETTING` below 0, or above 1 for an orifice, weir or outlet, with ERROR 211, as legacy `addAction()` does.

A curve or series that leaves 0..1 is now cut off silently. Rejecting such a curve at input is not possible, because the same curve can drive a pump, whose settings above 1 are legitimate.

**Effect on other models.** Only `gate_control_3.inp` among the regression decks uses a modulated action (`SETTING = CURVE GATE_RATE`, values 1.0 to 0); it and the other five decks with control rules (`extran10`, `gate_control_2`, `Example3`, `control_rules_test`, `Type5_Pump_Test`, whose pumps run at settings up to 1.2) give byte-identical `.out` files with the patch in 5.3.0 and 6.0.0.
