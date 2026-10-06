# IO-56: The Flow Classification Summary counts time a conduit is dry as "Norm Ltd"

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A conduit that was normal-flow limited (or inlet controlled) on its last flowing step is counted as limited for as long as it then stays dry. In the test an overflow pipe that is dry 83 % of the run is reported "Norm Ltd 0.83" instead of 0.01. In the regression deck user5, conduit 817chan is wet 21 % of the time and reported Norm Ltd 0.94 (correct: 0.19). Across ten regression decks, 151 conduit rows report more normal-flow-limited time than wet time. No warning. Flow results are not affected. |
| **Reached from** | Dynamic wave routing, any conduit that goes dry, or is closed by a control, after a step in which it was normal-flow limited or under inlet control |
| **5.3.0** | `dwflow_findConduitFlow()` returns early for a dry or closed conduit at [`src/legacy/engine/dwflow.c:189-204`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dwflow.c#L189-L204), before the flags are cleared at [`dwflow.c:326-327`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dwflow.c#L326-L327); `stats_updateLinkStats()` counts them at [`stats.c:693-694`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/stats.c#L693-L694) |
| **5.2.4** | Same code, [`src/solver/dwflow.c:165-180`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dwflow.c#L165-L180), [`dwflow.c:246-247`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dwflow.c#L246-L247) and [`stats.c:710`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/stats.c#L710) |
| **6.0.0** | Not affected: the statistics clear both flags after counting them at every step ([`src/engine/core/SWMMEngine.cpp:5418-5424`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L5418-L5424)) |
| **Since** | 5.1.000, which added the Norm Ltd and Inlet Ctrl columns |
| **Fix** | Clear both flags on the dry/closed return too: [`IO-56_swmm530.patch`](IO-56_swmm530.patch) |

## The problem

The last two columns of the Flow Classification Summary give the fraction of time each conduit's flow was limited to normal flow (Norm Ltd) and the fraction it was under culvert inlet control (Inlet Ctrl). Both limits apply only to a flowing conduit. The normal-flow check runs only for subcritical and supercritical flow. So neither column can be larger than the fraction of time the conduit was wet, 1 − Dry − Up Dry − Down Dry.

In 5.2.4 and 5.3.0 they often are. In the test, an overflow pipe runs for the first hour of a 6-hour run and is dry for the rest; it is reported dry 0.83 of the time and normal-flow limited 0.83 of the time. In the regression decks the pattern is common wherever conduits dry out between events:

| Deck, conduit | Dry + Up Dry + Down Dry | Norm Ltd reported | Norm Ltd with the fix |
|---|---|---|---|
| user5, 817chan | 0.79 | 0.94 | 0.19 |
| user5, 771chan | 0.71 | 0.90 | 0.25 |

The column is used to judge whether `NORMAL_FLOW_LIMITED` shapes a model's results, so an inflated value points at a limitation that was not acting.

## Why it happens

`dwflow_findConduitFlow()` updates the two flags at the end of each flow computation. It clears them first, then sets one if the culvert or normal-flow check limits the flow:

```c
// src/legacy/engine/dwflow.c, dwflow_findConduitFlow()
    // --- check if any flow limitation applies
    Link[linkIndex].inletControl = FALSE;
    Link[linkIndex].normalFlow = FALSE;
    if ( q > 0.0 )
    {
        // --- check for inlet controlled culvert flow
        if ( xsect->culvertCode > 0 && !isFull )
            q = culvert_getInflow(linkIndex, q, h1);

        // --- check for normal flow limitation based on surface slope & Fr
        else if (NormalFlowLtd != NEITHER && y1 < Link[linkIndex].xsect.yFull &&
                ( Link[linkIndex].flowClass == SUBCRITICAL ||
                  Link[linkIndex].flowClass == SUPCRITICAL ))
            q = checkNormalFlow(linkIndex, q, y1, y2, a1, r1);
    }
```

A dry or closed conduit never gets there. The function returns much earlier:

```c
    // --- set new flow to zero if conduit is dry or if flap gate is closed
    if ( Link[linkIndex].flowClass == DRY ||
         Link[linkIndex].flowClass == UP_DRY ||
         Link[linkIndex].flowClass == DN_DRY ||
         isClosed ||
         aMid <= FUDGE )
    {
        ...
        Link[linkIndex].newFlow = 0.0;
        return;
    }
```

So the flags keep the values of the last flowing step, and `stats_updateLinkStats()` adds every following routing step to the totals:

```c
// src/legacy/engine/stats.c, stats_updateLinkStats()
        if ( Link[j].normalFlow ) LinkStats[j].timeNormalFlow += tStep;
        if ( Link[j].inletControl ) LinkStats[j].timeInletControl += tStep;
```

Nothing else reads the flags, so the flows are right and only the summary is wrong.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-56_overflow.inp`](IO-56_overflow.inp) | A junction fed 4 cfs for the first hour of a 6-hour run, drained by a 0.5-ft low-level pipe (C2) and by a steep 1.5-ft overflow pipe (C1) whose inlet is 0.5 ft above the junction floor |
| [`IO-56_test.c`](IO-56_test.c) | Runs the deck through the legacy toolkit, writes the report and checks C1's Norm Ltd and Inlet Ctrl against its wet fraction from the same row, allowing 0.02 for the rounding of four `%4.2f` columns |
| [`IO-56_test6.c`](IO-56_test6.c) | The same check against 6.0.0 |

```sh
tools/run-test.sh IO-56            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-56 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0 print the same):

```
C1 fraction of time:  Dry 0.83  Up Dry 0.00  Down Dry 0.00  Sub 0.00  Sup 0.17  Up Crit 0.00  Down Crit 0.00
wet (1 - dry classes)  0.17
Norm Ltd               0.83
Inlet Ctrl             0.00
FAIL: C1 is reported normal-flow limited 0.83 of the time but was wet only 0.17 of the time
```

**6.0.0, and 5.3.0 with the fix:**

```
C1 fraction of time:  Dry 0.83  Up Dry 0.00  Down Dry 0.00  Sub 0.00  Sup 0.17  Up Crit 0.00  Down Crit 0.00
wet (1 - dry classes)  0.17
Norm Ltd               0.01
Inlet Ctrl             0.00
PASS: Norm Ltd (0.01) and Inlet Ctrl (0.00) do not exceed the time C1 was wet (0.17)
```

## The fix

Clear both flags on the early-return path as well:

```diff
         Link[linkIndex].newFlow = 0.0;
+        Link[linkIndex].inletControl = FALSE;
+        Link[linkIndex].normalFlow = FALSE;
         return;
     }
```

**Effect on other models.** Only the Norm Ltd and Inlet Ctrl columns change; the `.out` files and every other report line are identical. The ten regression decks in which the review had found Norm Ltd above the wet fraction were rerun: CoS-Reduced-Inlets, CoS-Reduced-Outlets, Example7-Final, Example7-Inlets, events_example, gate_control_2, gate_control_3, user2, user3 and user5. With the fix, 190 conduit rows change, all downward, and the 151 rows with Norm Ltd above the wet fraction drop to none. On nine of the ten decks the corrected column is identical to 6.0.0's. On events_example, three conduits that are never dry already differ from 6.0.0 by 0.01 to 0.02 in Norm Ltd. The fix does not change those rows, so the difference has another cause.
