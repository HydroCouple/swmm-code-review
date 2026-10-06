# IO-51: With CONTINUITY NO, a large negative continuity error is not reported

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | With `[REPORT] CONTINUITY NO`, SWMM still writes a continuity table when its error exceeds 10 %, but only if the error is positive (water or mass lost). A run that creates water or mass (negative error) keeps its table out of the report: the test run has a flow routing error of -100.000 % and the report has no continuity section at all. Nothing else warns. |
| **Reached from** | Any input with `[REPORT] CONTINUITY NO` whose runoff, runoff quality, groundwater (legacy only), flow routing or quality routing error is below -10 % |
| **5.3.0** | `massbal_report()` in [`src/legacy/engine/massbal.c:274`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L274-L310), and `massbal_getLoadingError()`, which returns the largest positive error, [`massbal.c:755`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L755) |
| **5.2.4** | Same code, [`src/solver/massbal.c:274`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/massbal.c#L274-L310) and [`:793`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/massbal.c#L793) |
| **6.0.0** | Reproduces: `DefaultReportPlugin` copies the four signed conditions, [`DefaultReportPlugin.cpp:964`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L964) (runoff), [`:1038`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1038) with [`:1027`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1027) (runoff quality), [`:1141`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1141) (flow routing), [`:1491`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1491) (quality routing). Its groundwater table ([`:1072`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1072)) is written only under `CONTINUITY YES`, whatever the error. |
| **Since** | Every release in the repository: 5.0.022 has the same conditions |
| **Fix** | Compare the magnitude of the error: [`IO-51_swmm530.patch`](IO-51_swmm530.patch), [`IO-51_swmm600.patch`](IO-51_swmm600.patch) (both need [IO-50](../IO-50-continuity-error-hides-nan/)'s patch first) |

## The problem

`CONTINUITY NO` turns the continuity tables off, with a safety net in the code: a table is written anyway when its error is larger than 10 %. The net only catches positive errors. SWMM's errors are signed, `100 * (1 - out/in)`: positive when less leaves (or is stored) than came in, negative when more does. A negative error means the model created water or mass, and is no less serious.

In the test network junction J1 receives 1 cfs of dry weather flow and has an `[INFLOWS]` baseline of -2 cfs, a withdrawal. Only 1 cfs is there to withdraw. The full 2 cfs is booked as outflow, so over the 2-hour run 0.331 acre-ft leaves against 0.165 acre-ft in, and the flow routing continuity error is -100 %. The deck sets `CONTINUITY NO`, and 5.2.4, 5.3.0 and 6.0.0 all leave the table out:

```
[REPORT] CONTINUITY              NO
flow routing continuity error   -100.000 % (swmm_getMassBalErr)
Flow Routing Continuity table   not written
```

The same run with a +100 % error would have shown the table.

## Why it happens

```c
// src/legacy/engine/massbal.c, massbal_report()
if ( massbal_getFlowError() > MAX_FLOW_BALANCE_ERR ||     // 10.0, signed
     RptFlags.continuity == TRUE
   ) report_writeFlowError(&FlowTotals);
```

All five tables (runoff, runoff quality, groundwater, flow routing, quality routing) use the same signed test. For runoff quality the error tested is not even the right one: `massbal_getLoadingError()` keeps

```c
maxError = MAX(maxError, LoadingTotals[j].pctError);   // starts at 0.0
```

so it returns the largest positive pollutant error and 0 when all are negative. `massbal_getQualError()` already returns the error of largest magnitude, with its sign.

6.0.0 writes the same conditions in `DefaultReportPlugin` (`runoff_err_pct > 10.0`, `max_err > 10.0`, `routing_error() * 100.0 > 10.0`) and keeps the runoff quality maximum with `std::max`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-51_withdrawal.inp`](IO-51_withdrawal.inp) | J1 -> C1 -> O1, 1 cfs dry weather flow and a -2 cfs `[INFLOWS]` baseline at J1, `CONTINUITY NO` |
| [`IO-51_test.c`](IO-51_test.c) | 5.2.4/5.3.0: runs the deck, reads the flow routing error from `swmm_getMassBalErr()` and looks for the Flow Routing Continuity table in the report |
| [`IO-51_test6.c`](IO-51_test6.c) | 6.0.0: the same with `swmm_get_routing_continuity_error()` |

The tests pass when the table is in the report exactly when the error's magnitude is over 10 %.

```sh
tools/run-test.sh IO-51            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-51 --patched  # 5.3.0, 6.0.0 (with IO-50): PASS
```

**Without the fix** (5.2.4 and 5.3.0 print the same; 6.0.0 reads the error from `swmm_get_routing_continuity_error`):

```
[REPORT] CONTINUITY              NO
flow routing continuity error   -100.000 % (swmm_getMassBalErr)
Flow Routing Continuity table   not written
FAIL: the flow routing continuity error is -100.000 % (|error| > 10 %) but the table is not written
IO-51 5.2.4 base: FAIL
IO-51 5.3.0 base: FAIL
IO-51 6.0.0 base: FAIL
```

**With the fix**:

```
[REPORT] CONTINUITY              NO
flow routing continuity error   -100.000 % (swmm_getMassBalErr)
Flow Routing Continuity table   written
PASS: the table is written for a continuity error of -100.000 %
IO-51 5.3.0 patched: PASS
IO-51 6.0.0 patched: PASS
```

and the report (5.3.0 and 6.0.0 write the same table) now has:

```
  Dry Weather Inflow .......         0.165         0.054
  ...
  External Outflow .........         0.331         0.108
  ...
  Continuity Error (%) .....      -100.000
```

## The fix

```diff
-        if ( massbal_getFlowError() > MAX_FLOW_BALANCE_ERR ||
+        if ( fabs(massbal_getFlowError()) > MAX_FLOW_BALANCE_ERR ||
 ...
-        maxError = MAX(maxError, LoadingTotals[j].pctError);
+        if ( fabs(LoadingTotals[j].pctError) > fabs(maxError) )
+            maxError = LoadingTotals[j].pctError;
```

The same `fabs()` goes into the other four conditions, and the 6.0.0 patch makes the matching changes in `DefaultReportPlugin`. The patches need IO-50's patch first only because both edit the lines around `maxError` in the runoff quality code; they do not depend on it otherwise.

Only reports written with `CONTINUITY NO` change, and only by gaining a table whose error is below -10 %. No computed result changes. A NaN error (see IO-50) still does not force a table, since `fabs(NaN) > 10` is false. The 6.0.0 groundwater table, which is never forced, is left as it is.
