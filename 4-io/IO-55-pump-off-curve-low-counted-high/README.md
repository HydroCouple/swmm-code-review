# IO-55: The Pumping Summary books all off-curve time of Type 1, 2, 3 and 5 pumps as "High"

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | The "% Time Off Pump Curve" columns put all of a Type1, Type2, Type3 or Type5 pump's off-curve time under High and leave Low at 0.0. In the test a wet well that is below its pump's curve 48.6 % of the time, and never above it, is reported as Low 0.0, High 48.7. The report says the pump ran above the top of its curve when it ran below the bottom, for example with a nearly empty wet well, which points a capacity check the wrong way. In the regression decks extran6 and extran7 the wet well is below the curve 55.5 % and 67.8 % of the pumps' running time and both are reported as High. 6.0.0 does not track off-curve time at all and prints 0.0 / 0.0. No warning. |
| **Reached from** | Any Type1 (volume), Type2 (depth), Type3 (head) or Type5 (variable-speed head) pump that runs while its curve variable is outside the range of its curve, with link results in the report |
| **5.3.0** | `pump_getInflow()` marks off-curve operation with `Link[j].flowClass = YES` in [`src/legacy/engine/link.c:1587`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1587), [`:1596`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1596) and [`:1614`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1614); `stats_updateLinkStats()` books `DN_DRY` as Low and `UP_DRY` as High at [`stats.c:678-681`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/stats.c#L678-L681) |
| **5.2.4** | Same code, [`src/solver/link.c:1584`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L1584), [`:1593`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L1593), [`:1611`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L1611) and [`stats.c:695-698`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/stats.c#L695-L698) |
| **6.0.0** | Different defect, same columns: off-curve operation is not tracked and `DefaultReportPlugin` prints a literal `0.0` in both ([`src/engine/plugins/DefaultReportPlugin.cpp:3070`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L3070)) |
| **Since** | 5.0.022, which split the off-curve time into Low and High but kept the `YES` marks from 5.0.012 for every curve type except Type4 |
| **Fix** | Mark `DN_DRY` below the curve and `UP_DRY` above it, as the Type4 case does (5.3.0); track and print both times (6.0.0, after [IO-54](../IO-54-pump-min-flow-always-zero/)): [`IO-55_swmm530.patch`](IO-55_swmm530.patch), [`IO-55_swmm600.patch`](IO-55_swmm600.patch) |

## The problem

The Pumping Summary ends with two columns, "% Time Off Pump Curve, Low / High". They give the share of the pump's running time in which its curve variable was below the first point of the curve (Low) or above the last (High). The variable is wet-well volume for Type1, inlet depth for Type2 and Type4, and head for Type3 and Type5. The split tells the user which way the pump is mis-sized: a wet well that keeps falling below the curve, or one that keeps rising above it.

For every pump type except Type4, the report puts all off-curve time under High. In the test, two Type2 pumps drain two identical wet wells. P1's curve starts at 2 ft and the well is below that for 48.6 % of the time, never above 6 ft; the report gives Low 0.0, High 48.7. P2's curve covers 0.5 to 1.5 ft; the well is below it 7.9 % and above it 67.2 % of the time, and the report gives Low 0.0, High 75.1, the sum of the two.

The regression deck extran6 shows the effect on a real model. Its Type1 pump has a volume curve from 200 to 1200 ft³, the wet well is below 200 ft³ for more than half of the run and never above 1200, and the report says High 55.5 %.

## Why it happens

`pump_getInflow()` marks the off-curve state in `Link[j].flowClass`. For Type4 it uses the two flow classes that stand for the two sides of the curve, but for the other curve types it stores the `NoYesType` value `YES`:

```c
// src/legacy/engine/link.c, pump_getInflow()
    Link[j].flowClass = NO;
    ...
      case PUMP2_CURVE:
        depth = Node[n1].newDepth * UCF(LENGTH);
        qIn = table_intervalLookup(&Curve[m], depth) / UCF(FLOW);

        // --- check if off of pump curve
        if ( depth < Pump[k].xMin || depth > Pump[k].xMax )
            Link[j].flowClass = YES;
        break;
    ...
      case PUMP4_CURVE:
        ...
        if ( depth < Pump[k].xMin ) Link[j].flowClass = DN_DRY;
        if ( depth > Pump[k].xMax ) Link[j].flowClass = UP_DRY;
```

`stats_updateLinkStats()` reads it as a `FlowClassType`:

```c
// src/legacy/engine/stats.c, stats_updateLinkStats()
            if ( Link[j].flowClass == DN_DRY )
                PumpStats[k].offCurveLow += tStep;
            if ( Link[j].flowClass == UP_DRY )
                PumpStats[k].offCurveHigh += tStep;
```

`YES` is 1, and so is `UP_DRY` (`DRY`, `UP_DRY`, `DN_DRY` = 0, 1, 2), so every off-curve step of a Type1, 2, 3 or 5 pump is added to `offCurveHigh`. The `PUMP1_CURVE` (volume) and `PUMP3_CURVE`/`PUMP5_CURVE` (head) cases have the same `YES`.

6.0.0's `computePumpFlowK()` does not record off-curve operation, and the report writes `std::fprintf(f, " %6.1f %6.1f", 0.0, 0.0);` for every pump.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-55_two-wells.inp`](IO-55_two-wells.inp) | Two 1000 ft² storage wet wells with the same inflow (1.5 cfs for 2 hours, then 0.5 cfs; each well rises to 3.6 ft and falls to 0.9 ft), each drained by a Type2 pump that delivers 1 cfs at every depth on its curve. P1's curve covers 2 to 6 ft, P2's 0.5 to 1.5 ft |
| [`IO-55_test.c`](IO-55_test.c) | Steps the run through the legacy toolkit. For each step in which a pump runs (flow above 0.001 cfs, the threshold `stats.c` uses), it books the step as below, above or on the pump's curve from the wet-well depth. Then it compares those shares with the Low and High columns of the report, allowing 2 percentage points for the threshold crossings |
| [`IO-55_test6.c`](IO-55_test6.c) | The same check against the 6.0.0 API |

```sh
tools/run-test.sh IO-55            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-55 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 (identical output):

```
        curve depths   % time below curve    % time above curve
pump        (ft)       wet well   report      wet well   report
P1       2.0 - 6.0        48.6      0.0           0.0     48.7
P2       0.5 - 1.5         7.9      0.0          67.2     75.1
  P1: report Low = 0.0 %, wet well was below the curve 48.6 % of the time
  P1: report High = 48.7 %, wet well was above the curve 0.0 % of the time
  P2: report Low = 0.0 %, wet well was below the curve 7.9 % of the time
  P2: report High = 75.1 %, wet well was above the curve 67.2 % of the time
FAIL: the % Time Off Pump Curve columns do not match the time the wet wells spent below and above the pump curves
```

6.0.0:

```
        curve depths   % time below curve    % time above curve
pump        (ft)       wet well   report      wet well   report
P1       2.0 - 6.0        48.7      0.0           0.0      0.0
P2       0.5 - 1.5         7.9      0.0          67.2      0.0
  P1: report Low = 0.0 %, wet well was below the curve 48.7 % of the time
  P2: report Low = 0.0 %, wet well was below the curve 7.9 % of the time
  P2: report High = 0.0 %, wet well was above the curve 67.2 % of the time
FAIL: the % Time Off Pump Curve columns do not match the time the wet wells spent below and above the pump curves
```

**With the fix**, 5.3.0 (6.0.0 prints the same report values; its own depth count gives 48.7 for P1):

```
        curve depths   % time below curve    % time above curve
pump        (ft)       wet well   report      wet well   report
P1       2.0 - 6.0        48.6     48.7           0.0      0.0
P2       0.5 - 1.5         7.9      7.9          67.2     67.2
PASS: % Time Off Pump Curve Low and High match the time the wet wells spent below and above the pump curves
```

## The fix

5.3.0: in the three cases that store `YES`, mark the side of the curve, as the Type4 case already does:

```diff
         // --- check if off of pump curve
-        if ( depth < Pump[k].xMin || depth > Pump[k].xMax )
-            Link[j].flowClass = YES;
+        if ( depth < Pump[k].xMin ) Link[j].flowClass = DN_DRY;
+        if ( depth > Pump[k].xMax ) Link[j].flowClass = UP_DRY;
```

The same change is made for the volume (Type1) and head (Type3/Type5) cases. A pump's `flowClass` is read only by the pump statistics, so the hydraulics do not change.

6.0.0: `computePumpFlowK()` sets the pump's `flow_class` the same way, from the curve variable it looked up and the curve's first and last x. The variable is the volume for Type1, the depth for Type2 and Type4, and head/s² for Type3 and Type5. `flow_class` of a pump was otherwise unused. `updateStatistics()` adds the step to two new `LinkData` counters on the steps legacy counts as running (`q > 0.001` cfs), and the report prints them as a percentage of the pump's running time:

```diff
+                if (q > 0.001 && ctx_.links.flow_class[uj] == FlowClass::DN_DRY)
+                    ctx_.links.stat_pump_off_curve_low[uj] += dt_routing;
+                if (q > 0.001 && ctx_.links.flow_class[uj] == FlowClass::UP_DRY)
+                    ctx_.links.stat_pump_off_curve_high[uj] += dt_routing;
```

```diff
-                std::fprintf(f, " %6.1f %6.1f", 0.0, 0.0);
+                double offLow  = (on_time > 0.0) ? ctx.links.stat_pump_off_curve_low[uj] / on_time * 100.0 : 0.0;
+                double offHigh = (on_time > 0.0) ? ctx.links.stat_pump_off_curve_high[uj] / on_time * 100.0 : 0.0;
+                std::fprintf(f, " %6.1f %6.1f", offLow, offHigh);
```

The 6.0.0 patch changes the same pump block and report row as IO-54's, so it is written on top of it (`Requires: IO-54`). The 5.3.0 patch applies on its own.

**Effect on other models.** Only the two off-curve columns change; the `.out` files and all other report lines are identical, in both engines. On the six regression decks with pumps:

| Deck, pump (type) | Before (5.3.0) Low / High | After, 5.3.0 and 6.0.0 |
|---|---|---|
| extran6, 90011 (Type1) | 0.0 / 55.5 | 55.5 / 0.0 |
| extran7, 90010 (Type2) | 0.0 / 67.8 | 67.8 / 0.0 |
| extran10, 90002 (Type3) | 0.0 / 0.4 | 0.4 / 0.0 |
| user3, PUMP3 (Type4) | 18.5 / 12.2 | 18.5 / 12.2 (5.3.0 unchanged; 6.0.0 had 0.0 / 0.0) |

Every other pump stays at 0.0 / 0.0. In extran6 and extran7 the wet well was below the bottom of the curve, not above the top, and in extran10 the head across pump 90002 was below the curve's lowest head. The percentages agree between the engines whenever a pump's running time does; 6.0.0 counts a pump as running at any flow above 0, legacy above 0.001 cfs, so for user3's PUMP1, for example, Percent Utilized is 100.00 in 6.0.0 and 92.51 in 5.3.0. That difference predates this patch.
