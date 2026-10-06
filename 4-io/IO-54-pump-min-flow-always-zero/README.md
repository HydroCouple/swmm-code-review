# IO-54: The Pumping Summary's "Min Flow" is always 0.00

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | The Min Flow column of every Pumping Summary row reads 0.00, whatever the pump did. In the test an ideal pump runs all 6 hours at 1 to 3 cfs and the report gives Min 0.00, Avg 1.83, Max 3.00. On the regression decks with pumps the corrected minimum is 0.45 cfs (Example3), 18.60 cfs (each of the five extran10 pumps) and 0.63 / 2.22 / 0.05 cfs (Type5_Pump_Test). In 5.3.0 `swmm_getPumpStats()` returns the same 0 in `minFlow`. Nothing indicates that the column is not computed. |
| **Reached from** | Any model with a pump and link results in the report (`[REPORT] LINKS` not `NONE`) |
| **5.3.0** | `stats_open()` sets `minFlow` to 0 in [`src/legacy/engine/stats.c:275`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/stats.c#L275); `stats_updateLinkStats()` lowers it with `MIN()` at [`stats.c:672`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/stats.c#L672); printed by `writePumpFlows()`, [`statsrpt.c:902`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L902) |
| **5.2.4** | Same code, [`src/solver/stats.c:278`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/stats.c#L278) and [`stats.c:689`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/stats.c#L689) |
| **6.0.0** | Reproduces: the engine keeps no minimum pump flow and `DefaultReportPlugin` prints a literal `0.0` in the column ([`src/engine/plugins/DefaultReportPlugin.cpp:3069`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L3069)) |
| **Since** | Every release in the repository history (5.0.022 onwards); EPA's `develop` branch has the same code |
| **Fix** | Start the minimum at the pump's first running flow (5.3.0); keep and print the minimum (6.0.0): [`IO-54_swmm530.patch`](IO-54_swmm530.patch), [`IO-54_swmm600.patch`](IO-54_swmm600.patch) |

## The problem

The Pumping Summary gives, for each pump, the minimum, average and maximum flow over the time the pump was running. The average and the maximum are right, but the minimum is 0.00 for every pump in every model. A pump that never stopped and never ran below 1 cfs is reported with Min Flow 0.00, which reads as "the pump ran at zero flow at some point". So the column cannot be used to check how far down its curve a pump was driven.

## Why it happens

`stats_open()` starts the minimum at zero:

```c
// src/legacy/engine/stats.c, stats_open()
            PumpStats[j].utilized = 0.0;
            PumpStats[j].minFlow  = 0.0;
            PumpStats[j].avgFlow  = 0.0;
```

`stats_updateLinkStats()` then takes the smaller of that value and the current flow, for each routing step in which the pump runs:

```c
// src/legacy/engine/stats.c, stats_updateLinkStats()
        if ( q > MIN_RUNOFF_FLOW )
        {
            k = Link[j].subIndex;
            PumpStats[k].minFlow = MIN(PumpStats[k].minFlow, q);
```

Inside this block `q` is above 0.001 cfs, so `MIN(0, q)` is 0 at every step and `minFlow` never changes. `writePumpFlows()` prints `PumpStats[k].minFlow*UCF(FLOW)` = 0.00, and `swmm_getPumpStats()` copies the same field.

6.0.0 accumulates the pump's running time, volume, energy and start-ups, but has no minimum. `DefaultReportPlugin` writes the constant:

```cpp
// src/engine/plugins/DefaultReportPlugin.cpp, Pumping Summary
                std::fprintf(f, "\n  %-20s %8.2f  %10d %9.2f %9.2f %9.2f %9.3f %9.2f",
                    ctx.link_names.name_of(j).c_str(),
                    pctUtilized, startUps, 0.0, avgFlow, max_flow, vol, energyKwh);
```

## How to reproduce

| File | What it is |
|---|---|
| [`IO-54_two-rates.inp`](IO-54_two-rates.inp) | A wet well drained by an ideal pump to an outfall. Inflow 1 cfs for 3 hours, a ramp to 3 cfs over the 4th hour, then 3 cfs to the end at 6:00 |
| [`IO-54_test.c`](IO-54_test.c) | Steps the run through the legacy toolkit and records the smallest and largest pump flow of the steps in which the pump ran (flow above 0.001 cfs). Then it writes the report and reads P1's Min Flow from the Pumping Summary |
| [`IO-54_test6.c`](IO-54_test6.c) | The same check against the 6.0.0 API |

```sh
tools/run-test.sh IO-54            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-54 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4, 5.3.0 and 6.0.0 print the same):

```
                       Min Flow   Max Flow  (CFS)
pump flow while on        1.00       3.00
Pumping Summary           0.00       3.00   (Avg Flow 1.83, 100.00 % utilized)
FAIL: Pumping Summary Min Flow is 0.00 cfs, but the pump never ran below 1.00 cfs
```

**With the fix** (5.3.0 and 6.0.0):

```
                       Min Flow   Max Flow  (CFS)
pump flow while on        1.00       3.00
Pumping Summary           1.00       3.00   (Avg Flow 1.83, 100.00 % utilized)
PASS: Pumping Summary Min Flow (1.00 cfs) is the smallest flow the pump delivered while running
```

## The fix

5.3.0: take the first running flow as the starting value. `totalPeriods` counts the steps in which the pump ran and is incremented further down in the same block, so it is 0 only on the first one. A pump that never runs keeps `minFlow` = 0, as before.

```diff
             k = Link[j].subIndex;
+            if ( PumpStats[k].totalPeriods == 0 ) PumpStats[k].minFlow = q;
             PumpStats[k].minFlow = MIN(PumpStats[k].minFlow, q);
```

6.0.0: a new `LinkData::stat_pump_min_flow` (sized, grown and erased with the other pump statistics) holds the smallest flow over the steps in which legacy counts the pump as on (`q > 0.001` cfs). Zero marks "not yet running". The report prints it in place of the constant:

```diff
+                double& qmin = ctx_.links.stat_pump_min_flow[uj];
+                if (q > 0.001 && (qmin == 0.0 || q < qmin)) qmin = q;
```

```diff
-                    pctUtilized, startUps, 0.0, avgFlow, max_flow, vol, energyKwh);
+                    pctUtilized, startUps, minFlow, avgFlow, max_flow, vol, energyKwh);
```

**Effect on other models.** Only the Min Flow column changes; the `.out` files and every other report line are identical. On the six regression decks with pumps, patched 5.3.0 and patched 6.0.0 print the same minimum for every pump: Example3 0.45 cfs; Type5_Pump_Test 0.63, 2.22 and 0.05; extran10 18.60 for each of its five pumps; extran6 0.01; extran7 0.02. user3 (CMS) still prints 0.00, correctly: its pumps' smallest running flows are about 0.001 m³/s, which the column's `%9.2f` rounds to 0.00.
