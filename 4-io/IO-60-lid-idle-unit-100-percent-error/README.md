# IO-60: The LID Performance Summary reports a 100 % continuity error for a LID unit that received no water

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A LID unit that starts empty and receives nothing (a dry-weather run, a gage with no rain, a covered barrel with no contributing area) is listed with a continuity error of 100.00 %, although every term of its balance is zero. No result is wrong, but the number is alarming and QA scripts that scan for large LID errors flag it. |
| **Reached from** | The LID Performance Summary of any run with a LID unit whose initial storage plus total inflow is zero |
| **5.3.0** | `lid_writeWaterBalance()` in [`src/legacy/engine/lid.c:2006`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lid.c#L2006) |
| **5.2.4** | Same code, [`src/solver/lid.c:1963`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lid.c#L1963) |
| **6.0.0** | Not applicable: its LID Performance Summary ([`src/engine/plugins/DefaultReportPlugin.cpp:2360`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L2360)) has no continuity-error column (nor the initial and final storage columns) |
| **Since** | Every release in the repository (5.0.022 has the same line) |
| **Fix** | Report 0 % when there was nothing to balance: [`IO-60_swmm530.patch`](IO-60_swmm530.patch) |

## The problem

The test deck has two identical 1 ac subcatchments, each with 10 empty rain barrels that take half of the impervious runoff. S1's gage records no rain; S2's records 1.0 in. S1's barrels receive nothing, lose nothing and end empty, so their balance error is 0. 5.2.4 and 5.3.0 print:

```
  Subcatchment      LID Control             in        in        in        in        in        in        in           %
  --------------------------------------------------------------------------------------------------------------------
  S1                RB1                   0.00      0.00      0.00      0.00      0.00      0.00      0.00      100.00
  S2                RB1                  22.54      0.00      0.00      0.00      0.00      0.00     22.54        0.00
```

The same happens to every LID unit in a dry-period run, and to any unit with no contributing area and no rain on its surface (for example a covered rain barrel with FromImp = 0).

## Why it happens

The error is computed as (in - out) / in, with "in" the initial storage plus the total inflow. When "in" is zero the code falls back to 1.0, i.e. 100 %, whatever the outflow:

```c
// src/legacy/engine/lid.c, lid_writeWaterBalance()
inflow = lidUnit->waterBalance.initVol +
         lidUnit->waterBalance.inflow;
outflow = lidUnit->waterBalance.finalVol +
          lidUnit->waterBalance.evap +
          lidUnit->waterBalance.infil +
          lidUnit->waterBalance.surfFlow +
          lidUnit->waterBalance.drainFlow;
if ( inflow > 0.0 ) err = (inflow - outflow) / inflow;
else                err = 1.0;
fprintf(Frpt.file, "  %10.2f", err*100.0);
```

The system continuity tables handle the same case differently: `massbal_getRunoffError()` reports 0 when inflow and outflow agree, and (inflow / outflow - 1), i.e. -100 %, when there is outflow without inflow.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-60_idle-barrel.inp`](IO-60_idle-barrel.inp) | Two 1 ac subcatchments with 10 rain barrels each; S1 on a gage with no rain, S2 on a gage with 1 in/hr for 1 h |
| [`IO-60_test.c`](IO-60_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0) and reads the Continuity Error column of the LID Performance Summary |
| [`IO-60_test6.c`](IO-60_test6.c) | The same with the 6.0.0 engine API; a row without a continuity column passes |

```sh
tools/run-test.sh IO-60            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-60 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0):

```
unit     gage          inflow   final    continuity
                        (in)   storage   error (%)
S1  RB1  no rain         0.00     0.00      100.00
S2  RB1  1.0 in rain    22.54    22.54        0.00
FAIL: the LID Performance Summary reports a continuity error for 1 unit(s) that balance exactly: S1 RB1 100.00 % with 0.00 in inflow
IO-60 5.2.4 base: FAIL
IO-60 5.3.0 base: FAIL
```

6.0.0 prints no continuity column:

```
S1  RB1  no rain         0.00      (no continuity column)
S2  RB1  1.0 in rain    22.54      (no continuity column)
PASS: no LID unit is reported with a continuity error, including the one that received no water
IO-60 6.0.0 base: PASS
```

**With the fix:**

```
S1  RB1  no rain         0.00     0.00        0.00
S2  RB1  1.0 in rain    22.54    22.54        0.00
PASS: no LID unit is reported with a continuity error, including the one that received no water
IO-60 5.3.0 patched: PASS
```

## The fix

Report 0 % when the unit had nothing in and nothing out, and -100 % (the value `massbal.c` gives) if it somehow released water without receiving any:

```diff
             if ( inflow > 0.0 ) err = (inflow - outflow) / inflow;
-            else                err = 1.0;
+            else                err = (outflow > 0.0) ? -1.0 : 0.0;
```

Only the printed error of units with zero initial storage and zero inflow changes. No simulated value changes.

6.0.0 needs no patch for this, but its LID Performance Summary leaves out the Initial Storage, Final Storage and Continuity Error columns that legacy prints, and prints "No LID performance data." instead of the table when no unit had any inflow or outflow.
