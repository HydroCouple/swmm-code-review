# IO-57: System runoff and total lateral inflow in the .out count runon twice

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | When a subcatchment drains onto another, the system "Runoff" and "Total lateral inflow" time series in the .out add its runoff twice. In the test, 15.1 cfs is reported for 10.1 cfs entering the network (50 % too high), and 1.255 ac-ft over the run for 0.834 ac-ft. In Example4 the system runoff volume is 2.2 times the runoff continuity table's; in the rain garden regression deck, roof runoff that all infiltrates in the garden shows up as system runoff although nothing reaches the drainage system. The .rpt is right. No warning. |
| **Reached from** | Any project with a subcatchment whose outlet is another subcatchment (common for roofs onto pervious areas, LID areas, swales) |
| **5.3.0** | `output_saveSubcatchResults()` in [`src/legacy/engine/output.c:620`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L620); `SYS_INFLOW` built from it at [`output.c:511`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L511) |
| **5.2.4** | Same code, [`src/solver/output.c:613`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/output.c#L613) |
| **6.0.0** | Reproduces with the same numbers: `SWMMEngine::postOutputSnapshot()` sums every subcatchment's runoff into `sys_runoff` ([`src/engine/core/SWMMEngine.cpp:6634`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L6634)) |
| **Since** | Every release (the line is in the first commit of the EPA repository, 5.0.021) |
| **Fix** | Add a subcatchment's runoff only when the runoff mass balance books it: [`IO-57_swmm530.patch`](IO-57_swmm530.patch), [`IO-57_swmm600.patch`](IO-57_swmm600.patch) |

## The problem

The binary output file carries system-wide series for runoff and for "total lateral inflow" (runoff plus dry weather, groundwater, RDII and direct inflow). Clients plot them as the inflow hydrograph of the whole network.

If subcatchment S1 drains onto S2, S1's runoff is runon to S2 and leaves the system again as part of S2's runoff. The .out adds both. In the test deck (two 5-acre impervious subcatchments, 1 in/hr for an hour, S1 onto S2, S2 to junction J1):

```
Time   S1 runoff  S2 runoff  Sys runoff  Sum node lat. inflow  Sys lateral inflow (cfs)
 1:00      5.042     10.084      15.126                10.084              15.126  <-- wrong
```

10.1 cfs falls on the project and 10.1 cfs enters J1; the .out says 15.1. The runoff continuity table, the routing continuity table and the node lateral inflows all count the water once.

The regression decks show the size of it:

| Deck | System runoff volume in the .out | Water entering the nodes | Peak system runoff |
|---|---|---|---|
| `examples/Example4.inp` (subcatchments onto swales) | 57,040 ft3 | 26,392 ft3 | 23.93 cfs, 10.96 after the fix |
| `update_v52/CoS-Reduced-Inlets.inp` | 5,117 m3 | 4,039 m3 | 4.30 m3/s, 3.06 after the fix |
| `update_v5111/rain_garden.inp` (roof onto a rain garden that infiltrates everything) | 3,977 L | 0 | 0.091 L/s, 0 after the fix |

(volumes are the .out series summed over the reporting periods; the runoff continuity tables give 0.585 ac-ft = 25,483 ft3, 0.401 ha-m = 4,010 m3 and 0.)

## Why it happens

```c
// src/legacy/engine/output.c, output_saveSubcatchResults()
for ( j=0; j<Nobjects[SUBCATCH]; j++)
{
    subcatch_getResults(j, f, SubcatchResults);
    ...
    SysResults[SYS_RUNOFF] += (REAL4)SubcatchResults[SUBCATCH_RUNOFF];
}
```

```c
// src/legacy/engine/output.c, output_saveResults()
SysResults[SYS_INFLOW] = SysResults[SYS_RUNOFF] + SysResults[SYS_DWFLOW] + SysResults[SYS_GWFLOW] +
                         SysResults[SYS_IIFLOW] + SysResults[SYS_EXFLOW];
```

The runoff mass balance has the rule this sum is missing:

```c
// src/legacy/engine/subcatch.c, subcatch_getRunoff()
// --- include this subcatchment's contribution to overall flow balance
//     only if its outlet is a drainage system node
if ( Subcatch[subcatchIndex].outNode == -1 && Subcatch[subcatchIndex].outSubcatch != subcatchIndex )
{
    vOutflow = 0.0;
}
```

6.0.0 applies that rule to its mass balance (`SWMMEngine.cpp`, `sheds_to_node || sheds_to_self`) but not to `sys_runoff`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-57_runon.inp`](IO-57_runon.inp) | S1 (5 ac, impervious) drains onto S2 (5 ac, impervious), S2 to J1, J1 to outfall O1; 1 in/hr for one hour |
| [`IO-57_test.c`](IO-57_test.c) | Legacy toolkit run; reads the .out and checks (1) at every period, system runoff = S2's runoff (0.1 %), (2) over the run, the volume of the system lateral inflow = the volume of the node lateral inflows (3 %; compared as volumes because node lateral inflow lags subcatchment runoff by one runoff step) |
| [`IO-57_test6.c`](IO-57_test6.c) | The same through `swmm_engine_run()` |
| [`IO-57_check.h`](IO-57_check.h) | The .out checks both tests share |
| [`IO-57_swmm530.patch`](IO-57_swmm530.patch), [`IO-57_swmm600.patch`](IO-57_swmm600.patch) | The fixes |

```sh
tools/run-test.sh IO-57            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-57 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 6.0.0 prints the same, 5.2.4 differs in the third decimal):

```
Time   S1 runoff  S2 runoff  Sys runoff  Sum node lat. inflow  Sys lateral inflow (cfs)
 0:30      5.038     10.066      15.104                10.064              15.104  <-- wrong
 1:00      5.042     10.084      15.126                10.084              15.126  <-- wrong
 1:30      0.137      0.395       0.532                 0.408               0.532  <-- wrong
 2:00      0.033      0.096       0.129                 0.098               0.129  <-- wrong
 2:30      0.013      0.039       0.052                 0.040               0.052  <-- wrong
 3:00      0.007      0.020       0.027                 0.020               0.027  <-- wrong
Volume over all periods: system lateral inflow 1.255 ac-ft, node lateral inflows 0.834 ac-ft (rain: 1 in on 10 ac = 0.833)
FAIL: in 36 of 36 periods system runoff is not S2's runoff; system lateral inflow volume 1.255 ac-ft against 0.834 ac-ft entering the nodes
IO-57 5.3.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Time   S1 runoff  S2 runoff  Sys runoff  Sum node lat. inflow  Sys lateral inflow (cfs)
 0:30      5.038     10.066      10.066                10.064              10.066
 1:00      5.042     10.084      10.084                10.084              10.084
 1:30      0.137      0.395       0.395                 0.408               0.395
 2:00      0.033      0.096       0.096                 0.098               0.096
 2:30      0.013      0.039       0.039                 0.040               0.039
 3:00      0.007      0.020       0.020                 0.020               0.020
Volume over all periods: system lateral inflow 0.836 ac-ft, node lateral inflows 0.834 ac-ft (rain: 1 in on 10 ac = 0.833)
PASS: system runoff and lateral inflow count each subcatchment's water once in all 36 periods
IO-57 5.3.0 patched: PASS
IO-57 6.0.0 patched: PASS
```

## The fix

Use the mass balance's rule: a subcatchment's runoff counts if its outlet is a node, or if it is routed back onto itself.

```diff
-        SysResults[SYS_RUNOFF] += (REAL4)SubcatchResults[SUBCATCH_RUNOFF];
+        // --- runoff sent onto another subcatchment is counted there
+        if ( Subcatch[j].outNode >= 0 || Subcatch[j].outSubcatch == j )
+            SysResults[SYS_RUNOFF] += (REAL4)SubcatchResults[SUBCATCH_RUNOFF];
```

The 6.0.0 patch makes the same test with `outlet_node` and `outlet_subcatch` in `postOutputSnapshot()`.

Effect on other models: only the system runoff and total lateral inflow series (system variables 4 and 9) of projects with subcatchment-to-subcatchment routing change; every subcatchment, node and link value and the whole .rpt stay byte-identical. Example1 is unchanged. For Example4, CoS-Reduced-Inlets and rain_garden the changes are those in the table above, and the patched 5.3.0 and 6.0.0 write identical system series.

LID underdrains routed to another subcatchment and outfalls routed onto a subcatchment (`RouteTo`) also produce runon; they are not changed here.
