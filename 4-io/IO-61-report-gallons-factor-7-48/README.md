# IO-61: Summary tables convert ft³ to gallons with 7.48, the continuity tables with 7.48056

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | In US units the summary tables (Subcatchment Runoff, Node Inflow, Node Flooding, Outfall Loading, Pumping and others) print every volume 0.0074 % smaller than the continuity tables print the same water. In the test, 10 days of 100 cfs leave the outfall as 646.048 million gallons in the Outfall Loading Summary and 646.096 in Flow Routing Continuity. The difference shows in the third decimal once a volume passes about 14 million gallons. SI reports are not visibly affected. |
| **Reached from** | Every report with US flow units (CFS, GPM, MGD) |
| **5.3.0** | `statsrpt_writeReport()` in [`src/legacy/engine/statsrpt.c:99`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L99), against `report_writeFlowError()` in [`report.c:747`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/report.c#L747) |
| **5.2.4** | Same code, [`src/solver/statsrpt.c:96`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L96) and [`report.c:747`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/report.c#L747) |
| **6.0.0** | Reproduces: `DefaultReportPlugin` copies both factors side by side, with a comment saying they differ ([`DefaultReportPlugin.cpp:901`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L899-L908)) |
| **Since** | Every release in the repository: 5.0.022's `statsrpt.c` has `Vcf = 7.48 / 1.0e6` |
| **Fix** | Use `MGDperCFS / SECperDAY` (`MLDperCFS / SECperDAY` for SI) in the summary tables: [`IO-61_swmm530.patch`](IO-61_swmm530.patch), [`IO-61_swmm600.patch`](IO-61_swmm600.patch) |

## The problem

A US gallon is 231 in³, so a cubic foot is 1728/231 = 7.48052 gallons. SWMM uses two approximations of it in the same report:

- the continuity tables use `MGDperCFS / SECperDAY` = 0.64632 / 86400 = 7.48056 gal/ft³ (4.8e-6 high);
- the summary tables use 7.48 gal/ft³ (6.9e-5 low).

The test deck sends a constant 100 cfs through one conduit to outfall O1 for 10 days. Everything that enters leaves through O1, and the report says:

```
  External Outflow .........      1982.710       646.096      (Flow Routing Continuity)
  O1                   100.00    100.00    100.00     646.048 (Outfall Loading Summary)
```

The two numbers are the same water. The ratio, 1.0000743, is exactly 7.48056/7.48. A user who checks the summary tables against the continuity table, or adds up outfall volumes to compare with External Outflow, finds a 0.048 million gallon gap that has no physical meaning. The SI factors, 28.317 L/ft³ and 2.4466/86400 ML/ft³, agree to 5e-6 and do not show this.

## Why it happens

```c
// src/legacy/engine/statsrpt.c, statsrpt_writeReport()
// --- conversion factor from cu. ft. to mil. gallons or megaliters
if (UnitSystem == US) Vcf = 7.48 / 1.0e6;
else                  Vcf = 28.317 / 1.0e6;

// src/legacy/engine/report.c, report_writeFlowError()
if ( UnitSystem == US) ucf2 = MGDperCFS / SECperDAY;
else                   ucf2 = MLDperCFS / SECperDAY;
```

`Vcf` is used for every volume in the summary tables. 6.0.0 keeps both constants, with the comment "legacy statsrpt.c's own factor for every summary-table volume (7.48 gal/ft3, 28.317 L/ft3 — not the massbal/continuity constants)".

## How to reproduce

| File | What it is |
|---|---|
| [`IO-61_ten-days-100cfs.inp`](IO-61_ten-days-100cfs.inp) | J1 -> C1 -> O1, steady flow routing, a constant 100 cfs external inflow at J1 for 10 days |
| [`IO-61_test.c`](IO-61_test.c) | 5.2.4/5.3.0: reads O1's Total Volume from the Outfall Loading Summary and External Outflow from Flow Routing Continuity |
| [`IO-61_test6.c`](IO-61_test6.c) | The same for 6.0.0 |

The test passes when the two agree within 0.005 million gallons (both are printed to 0.001 and accumulated in different places; the bug gives 0.048).

```sh
tools/run-test.sh IO-61            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-61 --patched  # 5.3.0, 6.0.0: PASS
```

**Without the fix** (all three print the same):

```
volume leaving through O1            10^6 gal
  Flow Routing Continuity             646.096   (1982.710 acre-ft)
  Outfall Loading Summary             646.048
  difference                           -0.048
  ratio                            1.0000743   (7.48056 / 7.48 = 1.0000743)
FAIL: the same volume prints as 646.048 million gallons in the Outfall Loading Summary and 646.096 in Flow Routing Continuity
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
volume leaving through O1            10^6 gal
  Flow Routing Continuity             646.096   (1982.710 acre-ft)
  Outfall Loading Summary             646.096
  difference                            0.000
  ratio                            1.0000000   (7.48056 / 7.48 = 1.0000743)
PASS: both tables print the outfall volume as 646.096 million gallons (within rounding)
```

## The fix

```diff
     // --- conversion factor from cu. ft. to mil. gallons or megaliters
-    if (UnitSystem == US) Vcf = 7.48 / 1.0e6;
-    else                  Vcf = 28.317 / 1.0e6;
+    //     (the same factor as the continuity tables in report.c)
+    if (UnitSystem == US) Vcf = MGDperCFS / SECperDAY;
+    else                  Vcf = MLDperCFS / SECperDAY;
```

The 6.0.0 patch sets its `Vcf` to the same expression it already uses for the continuity tables. Using the continuity constant rather than the exact 7.48052 keeps the change to one place; the continuity tables, and every number derived from flows in MGD, stay as they are.

**Effect on other models.** Only report text changes; computed results, continuity errors and the binary output files are identical. With base and patched builds of 5.3.0 and 6.0.0: Example1.inp's report is unchanged (its volumes are too small), extran10.inp changes in 3 lines (outfall 201 and the system total 11.378 -> 11.379 million gallons, pump 90006's pumped volume 2.174 -> 2.175), and user2.inp in 8 of about 2450 lines, each by one unit in the last printed digit (for example a subcatchment runoff volume 2.95 -> 2.96).
