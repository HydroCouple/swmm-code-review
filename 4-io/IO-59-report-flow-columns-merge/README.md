# IO-59: Flows of 100,000 or more run into the neighbouring column of the Node Inflow Summary

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | The Node Inflow Summary prints the maximum lateral and maximum total inflow with no space between them. When the total inflow reaches 100,000.00 (CFS, GPM, LPS, MLD) or 10,000.000 (MGD, CMS), the two numbers merge. The threshold is reached at 223 cfs in GPM and 100 m³/s in LPS, flows that large trunk sewers and channels carry. In the test, a junction receiving 150,000 GPM is printed `150000.00150000.00`, and its outfall `0.00150241.30`. A program that splits the report on whitespace reads 150000.0015 and 0.00 for the junction, and 0.00150241 and 0.30 for the outfall. No warning. |
| **Reached from** | Any model whose maximum total inflow at a node reaches the threshold for its flow units |
| **5.3.0** | `statsrpt_writeReport()` sets `FlowFmt` to `"%9.2f"` / `"%9.3f"` at [`src/legacy/engine/statsrpt.c:95-96`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L95-L96); `writeNodeFlows()` prints the two flows back to back at [`statsrpt.c:388-389`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L388-L389). The file header still says (Build 5.2.5) that the flow format was changed to prevent this ([`statsrpt.c:33-35`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/statsrpt.c#L33-L35)) |
| **5.2.4** | Same code without the header note, [`src/solver/statsrpt.c:92-93`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L92-L93) and [`statsrpt.c:385-386`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/statsrpt.c#L385-L386) |
| **6.0.0** | Reproduces: `flowFmt()` returns the same formats ([`src/engine/plugins/DefaultReportPlugin.cpp:151-153`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L151-L153)) and the two columns are printed back to back at [`DefaultReportPlugin.cpp:2498-2499`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L2498-L2499) |
| **Since** | Every release in the repository history (5.0.012 onwards). A fix for EPA issue [#109](https://github.com/USEPA/Stormwater-Management-Model/issues/109) (32c46eb6, `%9.2g`) was reverted in 0d3fdbec on EPA's `develop`; only its header note reached 5.3.0 |
| **Fix** | Print a space before each flow value, `" %8.2f"` / `" %8.3f"`, as proposed in #109: [`IO-59_swmm530.patch`](IO-59_swmm530.patch) (after [IO-61](../IO-61-report-gallons-factor-7-48/), which changes the next line), [`IO-59_swmm600.patch`](IO-59_swmm600.patch) |

## The problem

The summary tables print every flow in a 9-character field: `%9.2f`, or `%9.3f` for MGD and CMS. Most columns are separated from the field before them by literal spaces in the format, but in the Node Inflow Summary the maximum lateral inflow and the maximum total inflow follow each other directly. A total inflow of 100,000.00 or more needs all 9 characters, the field has no leading blank left, and the number runs into the lateral inflow before it:

```
                                   Inflow   Inflow   Occurrence      Volume      Volume       Error
  Node                 Type           GPM      GPM  days hr:min    10^6 gal    10^6 gal     Percent
  -------------------------------------------------------------------------------------------------
  J1                   JUNCTION 150000.00150000.00     0  00:00          18          18      -0.000
  O1                   OUTFALL       0.00150241.30     0  00:01           0          18       0.000
```

The flow at which this happens depends on the units:

| Flow units | Format | Merges from | In cfs |
|---|---|---|---|
| GPM | `%9.2f` | 100,000 GPM | 223 |
| LPS | `%9.2f` | 100,000 L/s | 3,531 (100 m³/s) |
| CFS | `%9.2f` | 100,000 cfs | 100,000 |
| MLD | `%9.2f` | 100,000 ML/d | 40,870 |
| MGD | `%9.3f` | 10,000 MGD | 15,472 |
| CMS | `%9.3f` | 10,000 m³/s | 353,147 |

In GPM, a model of a large collection system crosses the limit at its trunk sewers and treatment-plant outfall. A reader can still separate the digits by eye when both numbers have two decimals. Post-processing scripts and spreadsheet imports that split the report on whitespace cannot.

EPA's issue #109 reported this in 2023. The fix committed for it (32c46eb6) switched the format to `%9.2g`, which keeps only two significant digits, and was reverted (0d3fdbec). The 5.2.5 note it added to the header of `statsrpt.c`, "Changed flow format to scientific to prevent the merging of extremely large flows", is in 5.3.0, but the code it describes is not.

## Why it happens

```c
// src/legacy/engine/statsrpt.c, statsrpt_writeReport()
    if ( FlowUnits == MGD || FlowUnits == CMS ) sstrncpy(FlowFmt, "%9.3f", 5);
    else sstrncpy(FlowFmt, "%9.2f", 5);

// writeNodeFlows()
        fprintf(Frpt.file, " %-9s", NodeTypeWords[Node[j].type]);
        getElapsedTime(NodeStats[j].maxInflowDate, &days1, &hrs1, &mins1);
        fprintf(Frpt.file, FlowFmt, NodeStats[j].maxLatFlow * UCF(FLOW));
        fprintf(Frpt.file, FlowFmt, NodeStats[j].maxInflow * UCF(FLOW));
```

`%9.2f` pads to 9 characters but never truncates. `150000.00` is exactly 9 characters, so nothing separates it from the field before. The other tables that use `FlowFmt` (flooding, storage, outfall loading, link flows) print a literal space before it, so only this pair merges. 6.0.0's `DefaultReportPlugin` copies both the formats and the layout.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-59_trunk-gpm.inp`](IO-59_trunk-gpm.inp) | One junction receiving a constant 150,000 GPM (334 cfs), a 6-ft pipe and an outfall, in GPM units |
| [`IO-59_test.c`](IO-59_test.c) | Runs the deck through the legacy toolkit, writes the report and reads the Node Inflow Summary rows of J1 and O1 back as whitespace-separated fields. It expects the lateral inflow (150,000 at J1, 0 at O1) and a maximum total inflow within 1 % of 150,000 GPM |
| [`IO-59_test6.c`](IO-59_test6.c) | The same check against 6.0.0 |

```sh
tools/run-test.sh IO-59            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-59 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4, 5.3.0 and 6.0.0 print the same):

```
Node Inflow Summary rows:
  J1                   JUNCTION 150000.00150000.00     0  00:00   
  O1                   OUTFALL       0.00150241.30     0  00:01   

      read as:  max lateral   max total   (GPM)
  J1             150000.00          0.00   (7 of 7 fields)
  O1                  0.00          0.30   (7 of 7 fields)
FAIL: the maximum lateral and total inflow columns of the Node Inflow Summary run together for flows of 150,000 GPM
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
Node Inflow Summary rows:
  J1                   JUNCTION  150000.00 150000.00     0  00:00 
  O1                   OUTFALL       0.00 150241.30     0  00:01  

      read as:  max lateral   max total   (GPM)
  J1             150000.00     150000.00   (7 of 7 fields)
  O1                  0.00     150241.30   (7 of 7 fields)
PASS: the Node Inflow Summary columns stay separate and read back as 150,000 GPM
```

## The fix

Print the flow after a space in a field one character narrower, the padding proposed in #109:

```diff
-static char   FlowFmt[6];
+static char   FlowFmt[7];
...
-    if ( FlowUnits == MGD || FlowUnits == CMS ) sstrncpy(FlowFmt, "%9.3f", 5);
-    else sstrncpy(FlowFmt, "%9.2f", 5);
+    if ( FlowUnits == MGD || FlowUnits == CMS ) sstrncpy(FlowFmt, " %8.3f", 6);
+    else sstrncpy(FlowFmt, " %8.2f", 6);
```

`" %8.2f"` writes exactly the same 9 characters as `"%9.2f"` for every value of up to 8 characters, that is below 100,000 (and above −10,000). A larger value is preceded by a space and its row is one character longer. The 5.2.5 header note now describes this change. The 6.0.0 patch makes the same change in `flowFmt()`.

The 5.3.0 patch is made on top of [IO-61](../IO-61-report-gallons-factor-7-48/), whose hunk starts on the line after these two (`Requires: IO-61`); the 6.0.0 patch applies on its own.

**Effect on other models.** Reports change only where a flow column holds a value of 9 or more characters. None of the 73 regression decks has one, so their reports are byte-for-byte the same.
