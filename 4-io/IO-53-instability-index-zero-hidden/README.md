# IO-53: "All links are stable" when the most unstable link is the first one in the input

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | When the worst link is the first link of the input file, the "Highest Flow Instability Indexes" section says `All links are stable.` and lists nothing. The same happens to "Most Frequent Nonconverging Nodes" (`Convergence obtained at all time steps.`) when the worst node is the first node, even next to "% of Steps Not Converging: 64.17". Reordering the input file changes the diagnosis. |
| **Reached from** | Any dynamic wave run (any run, for the instability list) whose first link in `[CONDUITS]`/`[PUMPS]`/... or first node in `[JUNCTIONS]`/`[OUTFALLS]`/... ranks highest |
| **5.3.0** | `report_writeMaxFlowTurns()` in [`src/legacy/engine/report.c:994`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/report.c#L994) and `report_writeNonconvergedStats()` in [`report.c:1019`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/report.c#L1019) |
| **5.2.4** | Same code, [`src/solver/report.c:994`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/report.c#L994) and [`:1019`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/report.c#L1019). In the test deck 5.2.4's worst node is the outfall, so only its instability list is hidden. |
| **6.0.0** | Reproduces, on purpose ("a top entry at index 0 reads as 'stable' there (index <= 0) and is reproduced"): [`DefaultReportPlugin.cpp:1607`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1607) and [`:1634`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L1634) |
| **Since** | Flow instability list: 5.0.011 (the line carries the comment `//(5.0.011 - LR)` in 5.0.022). Nonconverging nodes list: 5.2.0. |
| **Fix** | Test for `index < 0`: [`IO-53_swmm530.patch`](IO-53_swmm530.patch), [`IO-53_swmm600.patch`](IO-53_swmm600.patch) |

## The problem

The two decks hold the same model: J1 -> C1 -> J2 -> C2 -> O1, with an inflow to J1 that alternates between 1 and 3 cfs at every 30 s routing step, and `MAX_TRIALS 1`, `HEAD_TOLERANCE 0.0001` so that the junctions often fail to converge. `IO-53_c1-first.inp` lists J1 and C1 first; `IO-53_c2-first.inp` lists J2 and C2 first. The physics is identical, and 5.3.0 computes identical numbers for both. The reports differ:

```
IO-53_c1-first.inp                         IO-53_c2-first.inp
  Highest Flow Instability Indexes           Highest Flow Instability Indexes
    Link C2 (43)                               All links are stable.
    Link C1 (42)
  Most Frequent Nonconverging Nodes          Most Frequent Nonconverging Nodes
    Node J2 (60.83%)                           Convergence obtained at all time steps.
    Node J1 (60.00%)
```

C2 and J2 are the worst link and node. Listed first in the file, they get index 0, and the whole list disappears. The same report says, a few lines up, that 64.17 % of the steps did not converge.

## Why it happens

`stats_findMaxStats()` fills the rankings with `index = -1` for "no entry", and the summary of continuity errors tests for that correctly. The other two lists test `index <= 0`:

```c
// src/legacy/engine/report.c
// report_writeMaxStats()  (Highest Continuity Errors)
if ( maxMassBalErrs[0].index >= 0 )

// report_writeMaxFlowTurns()
if ( nMaxStats <= 0 || flowTurns[0].index <= 0 )
    fprintf(Frpt.file, "\n  All links are stable.");

// report_writeNonconvergedStats()
if (nMaxStats <= 0 || maxNonconverged[0].index <= 0 ||
    maxNonconverged[0].value < 0.00005)
    fprintf(Frpt.file, "\n  Convergence obtained at all time steps.");
```

Index 0 is a valid link or node, the first one read from the input file. Entries only enter the rankings above a threshold (a flow instability index above 1, a nonconvergence fraction above 0), so an index of 0 at the top always means the first element is the worst one, never that the list is empty.

6.0.0 copies both tests and documents the copy as intended parity.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-53_c1-first.inp`](IO-53_c1-first.inp) | The model with J1 and C1 first in their sections |
| [`IO-53_c2-first.inp`](IO-53_c2-first.inp) | The same model with J2 and C2 first |
| [`IO-53_test.c`](IO-53_test.c) | 5.2.4/5.3.0: runs both decks, counts each conduit's flow turns from the flows read at every step (the rule of `stats_updateLinkStats()`), and checks the two report sections against that count and against the report's own "% of Steps Not Converging" |
| [`IO-53_test6.c`](IO-53_test6.c) | The same against the 6.0.0 API |

A report fails the check if it says `All links are stable.` while a conduit turned at 5 % or more of the steps (the decks give 7.5 % to 28.6 %), or says `Convergence obtained at all time steps.` while "% of Steps Not Converging" is above 0.005 % (the list's own threshold; the decks give 22.50 % and 64.17 %).

```sh
tools/run-test.sh IO-53            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-53 --patched  # 5.3.0, 6.0.0: PASS
```

**Without the fix**, 5.3.0:

```
IO-53_c2-first.inp (119 routing steps)
    link 0 = C2: flow turned 34 times (28.6 % of steps)
    link 1 = C1: flow turned 33 times (27.7 % of steps)
  Highest Flow Instability Indexes:
    |   All links are stable.
  Most Frequent Nonconverging Nodes (% of Steps Not Converging = 64.17):
    |   Convergence obtained at all time steps.
  -> wrong: a conduit turned at 28.6 % of the steps but the report says all links are stable
  -> wrong: 64.17 % of the steps did not converge but the report says convergence was obtained at all time steps

FAIL: the report says the links are stable or the nodes converged although they are not (IO-53_c2-first.inp)
IO-53 5.3.0 base: FAIL
```

5.2.4 hides the instability list the same way; its worst nonconverging node is the outfall O1 (64.17 %), which is not first, so that list prints. 6.0.0 computes different flows for these decks (C1 turns 11 times, C2 9 times, 22.50 % of steps not converging), so its worst link is C1, and it hides the list for `IO-53_c1-first.inp` instead; its two junctions tie for the nonconvergence ranking, so node 0 is on top in both decks:

```
IO-53_c1-first.inp (120 routing steps)
    link 0 = C1: flow turned 11 times (9.2 % of steps)
    link 1 = C2: flow turned 9 times (7.5 % of steps)
  Highest Flow Instability Indexes:
    |   All links are stable.
  Most Frequent Nonconverging Nodes (% of Steps Not Converging = 22.50):
    |   Convergence obtained at all time steps.
...
FAIL: the report says the links are stable or the nodes converged although they are not (both decks)
IO-53 6.0.0 base: FAIL
```

**With the fix**, both decks list the same entries. 5.3.0:

```
IO-53_c2-first.inp (119 routing steps)
  ...
  Highest Flow Instability Indexes:
    |   Link C2 (43)
    |   Link C1 (42)
  Most Frequent Nonconverging Nodes (% of Steps Not Converging = 64.17):
    |   Node J2 (60.83%)
    |   Node J1 (60.00%)

PASS: both reports list the unstable links and nonconverging nodes, whatever their order in the input
IO-53 5.3.0 patched: PASS
```

6.0.0:

```
IO-53_c1-first.inp (120 routing steps)
  ...
  Highest Flow Instability Indexes:
    |   Link C1 (14)
    |   Link C2 (11)
  Most Frequent Nonconverging Nodes (% of Steps Not Converging = 22.50):
    |   Node J1 (20.83%)
    |   Node J2 (20.83%)
...
PASS: both reports list the unstable links and nonconverging nodes, whatever their order in the input
IO-53 6.0.0 patched: PASS
```

The 6.0.0 numbers differ from 5.3.0's because the two engines compute different flows for this deck, before any report is written; that is not caused by this defect or its fix.

## The fix

```diff
-    if ( nMaxStats <= 0 || flowTurns[0].index <= 0 )
+    if ( nMaxStats <= 0 || flowTurns[0].index < 0 )
 ...
-    if (nMaxStats <= 0 || maxNonconverged[0].index <= 0 ||
+    if (nMaxStats <= 0 || maxNonconverged[0].index < 0 ||
```

The 6.0.0 patch makes the same change in `DefaultReportPlugin.cpp` and corrects the two comments that called the old test intended. Only those two report sections change, and only for models whose first link or node tops the ranking; no computed result changes.
