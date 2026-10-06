# IO-50: A continuity table full of NaN reports "Continuity Error (%) 0.000"

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | When NaN reaches a continuity ledger, the continuity error can print as `0.000` %, a perfect balance, right under rows that read `nan`. `swmm_getMassBalErr()` (and in 6.0.0 the `swmm_get_*_continuity_error()` getters) return 0. The continuity error is the first thing most users check, and this is the case where it matters most. |
| **Reached from** | Any run whose ledger totals become NaN on both the inflow and the outflow side: a NaN through the toolkit API (`swmm_setValue(swmm_NODE_LATFLOW, ...)`, 6.0.0's `swmm_node_set_quality_mass_flux()`), a `nan` in the input file ([IO-01](../IO-01-nan-inf-accepted-as-numbers/)), or a defect that produces NaN, such as [NUM-01](../../1-numerical/NUM-01-dry-conduit-quality-nan/) in 5.3.0 |
| **5.3.0** | `massbal_getRunoffError()`, `massbal_getLoadingError()`, `massbal_getGwaterError()`, `massbal_getFlowError()` and `massbal_getQualError()` in [`src/legacy/engine/massbal.c:693`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L693), [`:742`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L742), [`:801`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L801), [`:849`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L849-L862), [`:899`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L899-L917) |
| **5.2.4** | Same code, [`src/solver/massbal.c:887`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/massbal.c#L887-L900) (flow) and the four other functions |
| **6.0.0** | Reproduces. The same formula is copied into the report tables of [`DefaultReportPlugin.cpp:1468`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1468-L1471) (quality routing), [`:1201`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1201-L1204) (flow routing), [`:1120`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1120-L1126) (groundwater), [`:1024`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1024-L1027) (runoff quality), into `swmm_get_quality_continuity_error()` ([`openswmm_massbalance_impl.cpp:97`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_massbalance_impl.cpp#L97-L105)) and, as `total_in > 0 ? ... : 0`, into `runoff_error()`, `routing_error()` and `gw_error()` ([`SimulationContext.hpp:1338`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SimulationContext.hpp#L1338)). A NaN head or link flow stops a 6.0.0 run with error 14, so the flow table is protected in practice; the quality table is not. |
| **Since** | Every release in the repository: 5.0.022 already has the same three branches |
| **Fix** | Add a fourth branch that returns NaN when a total is NaN: [`IO-50_swmm530.patch`](IO-50_swmm530.patch), [`IO-50_swmm600.patch`](IO-50_swmm600.patch) |

## The problem

The test network is one junction, one conduit and a free outfall, with a steady 2 cfs inflow at J1 carrying 100 mg/L of TSS. For one routing step (step 10 of 240) the test sets J1's lateral inflow to NaN through `swmm_setValue(swmm_NODE_LATFLOW, ...)`, which accepts any double. From then on J1's depth is NaN and so is everything that depends on it. 5.2.4 and 5.3.0 finish the run and write:

```
  **************************        Volume        Volume
  Flow Routing Continuity        acre-feet      10^6 gal
  **************************     ---------     ---------
  ...
  External Inflow ..........         0.329         0.107
  External Outflow .........          -nan          -nan
  ...
  Final Stored Volume ......           nan           nan
  Continuity Error (%) .....         0.000
```

and `swmm_getMassBalErr()` returns a flow routing error of 0. The summary tables are full of `nan` as well, but the table a user reads to judge the run says it balances exactly.

6.0.0 stops this particular run with `ERROR 14: the routing solution diverged at node 'J1'`, because it checks heads and link flows after every step. It has no such check for quality. A NaN TSS mass flux set for one step with `swmm_node_set_quality_mass_flux()` leaves the hydraulics alone and gives:

```
  External Inflow ..........           nan
  External Outflow .........           nan
  ...
  Final Stored Mass ........           nan
  Continuity Error (%) .....         0.000
```

with `swmm_get_quality_continuity_error()` returning 0.

The NaN in these runs comes from the API, but the masking does not depend on where the NaN comes from. EPA's Example1 run with 5.3.0 shows `0.000` under a NaN quality ledger because of [NUM-01](../../1-numerical/NUM-01-dry-conduit-quality-nan/), and a `nan` typed into an input file does the same ([IO-01](../IO-01-nan-inf-accepted-as-numbers/)). Once those defects are fixed, this one is still there for the next source of NaN.

## Why it happens

Every continuity error in SWMM is computed the same way: start from 0, and overwrite it in one of three branches.

```c
// src/legacy/engine/massbal.c, massbal_getFlowError()
FlowTotals.pctError = 0.0;
if ( fabs(totalInflow - totalOutflow) < 1.0 )
{
    FlowTotals.pctError = TINY;
}
else if ( fabs(totalInflow) > 0.0 )
{
    FlowTotals.pctError = 100.0 * (1.0 - totalOutflow / totalInflow);
}
else if ( fabs(totalOutflow) > 0.0 )
{
    FlowTotals.pctError = 100.0 * (totalInflow / totalOutflow - 1.0);
}
FlowError = FlowTotals.pctError;
```

Every comparison with NaN is false. If only one of the two totals is NaN, one of the last two branches still runs and the NaN shows up in the result (that is why the Quality Routing Continuity table of the same 5.2.4/5.3.0 run already prints `nan`: its inflow total stayed finite). If both totals are NaN, no branch runs and the error stays 0.0. In the flow table both are NaN as soon as one inflow-side and one outflow-side term are: here a NaN inflow is booked as a negative inflow, which moves to the outflow side, and the NaN outflow and final storage do the rest.

The runoff, runoff-loading, groundwater and quality-routing functions have the same shape. `massbal_getQualError()` then picks the pollutant with the largest `fabs(pctError)`, and `fabs(NaN) > x` is false, so a NaN pollutant error would not reach `swmm_getMassBalErr()` either.

6.0.0 copies the formula into its report tables and API getters, and `routing_error()` (used by `swmm_get_routing_continuity_error()`) is `total_in > 0.0 ? ... : 0.0`, which also gives 0 for NaN.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-50_nan-inflow.inp`](IO-50_nan-inflow.inp) | J1 -> C1 -> O1 (dynamic wave), 2 cfs and 100 mg/L TSS at J1, 2 hours |
| [`IO-50_test.c`](IO-50_test.c) | 5.2.4/5.3.0: NaN lateral inflow at J1 for one step; reads the Flow and Quality Routing Continuity tables from the report and compares them with `swmm_getMassBalErr()` |
| [`IO-50_test6.c`](IO-50_test6.c) | 6.0.0: run A with a NaN lateral inflow, run B with a NaN TSS mass flux; reads the report and the C API getters |

The tests pass when a table with `nan` rows has a NaN continuity error, or when the ledger stays finite (a run that stops with an error also passes).

```sh
tools/run-test.sh IO-50            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-50 --patched  # 5.3.0, 6.0.0: PASS
```

**Without the fix** (5.2.4 and 5.3.0 print the same):

```
Flow Routing Continuity table of IO-50.rpt:
  ...
  External Inflow ..........         0.329         0.107
  External Outflow .........          -nan          -nan
  ...
  Final Stored Volume ......           nan           nan
  Continuity Error (%) .....         0.000
...
  nan rows: 2, error in report: 0, from swmm_getMassBalErr(): 0

FAIL: Flow Routing Continuity has 2 nan rows but its continuity error reads 0.000 % in the report and 0.000 % from swmm_getMassBalErr()
IO-50 5.2.4 base: FAIL
IO-50 5.3.0 base: FAIL
```

```
Run A: NaN lateral inflow at J1 for one step
run stopped at step 10 with error 14: ERROR 14: the routing solution diverged at node 'J1' (head nan) after 0.0704 hours -- the run cannot continue.
  stopped with an error: nothing is reported as balanced

Run B: NaN TSS mass flux at J1 for one step
Quality Routing Continuity table of IO-50b6.rpt:
  ...
  External Inflow ..........           nan
  External Outflow .........           nan
  Flooding Loss ............         0.000
  Exfiltration Loss ........           nan
  ...
  Final Stored Mass ........           nan
  Continuity Error (%) .....         0.000
  nan rows: 4, error in report: 0 %, from the C API: 0 (fraction)

FAIL: Quality Routing Continuity (run B) has 4 nan rows but its continuity error reads 0.000 % in the report and 0.000 % from the C API
IO-50 6.0.0 base: FAIL
```

**With the fix**:

```
  Final Stored Volume ......           nan           nan
  Continuity Error (%) .....           nan
...
  nan rows: 2, error in report: nan, from swmm_getMassBalErr(): nan
  nan rows: 1, error in report: nan, from swmm_getMassBalErr(): nan

PASS: the flow (2 nan rows) and quality (1 nan rows) continuity errors are reported as nan, not as a number
IO-50 5.3.0 patched: PASS
```

```
  Final Stored Mass ........           nan
  Continuity Error (%) .....           nan
  nan rows: 4, error in report: nan %, from the C API: nan (fraction)

PASS: the quality continuity error with 4 nan rows is reported as nan, not as a number
IO-50 6.0.0 patched: PASS
```

## The fix

Add a fourth branch to each of the five legacy functions, and let a NaN pollutant error win the "largest error" search:

```diff
     else if ( fabs(totalOutflow) > 0.0 )
     {
         FlowTotals.pctError = 100.0 * (totalInflow / totalOutflow - 1.0);
     }
+    else if ( isnan(totalInflow) || isnan(totalOutflow) )
+    {
+        FlowTotals.pctError = NAN;
+    }
 ...
-        if ( fabs(QualTotals[p].pctError) > fabs(maxQualError) )
+        if ( fabs(QualTotals[p].pctError) > fabs(maxQualError) ||
+             isnan(QualTotals[p].pctError) )
```

The 6.0.0 patch adds the same branch to the four report tables and to `swmm_get_quality_continuity_error()`, and returns NaN from `runoff_error()`, `routing_error()` and `gw_error()` when a total is NaN.

The new branch is reached only when a total is NaN, so every finite ledger gives the same error as before and no regression output changes. The report then prints `nan` as the continuity error. With `[REPORT] CONTINUITY NO` a NaN error still does not force the table to be written; see [IO-51](../IO-51-continuity-forcing-ignores-negative-errors/).
