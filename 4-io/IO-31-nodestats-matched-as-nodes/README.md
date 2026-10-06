# IO-31: The deprecated [REPORT] keyword NODESTATS is read as NODES and rejected

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Valid input rejected with a misleading message. A deck with `NODESTATS YES` (or `NO`) in `[REPORT]`, which the code means to accept and ignore, fails to run with `ERROR 209: undefined object YES at line N of [REPORT] section` |
| **Reached from** | `[REPORT]` lines starting with `NODESTATS`, as written for older SWMM 5 releases |
| **5.3.0** | `report_readOptions()` in [`src/legacy/engine/report.c:104`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/report.c#L104); the ignored-keyword branch at [`report.c:121`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/report.c#L121) cannot be reached because of the keyword order in [`keywords.c:111`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/keywords.c#L111) |
| **5.2.4** | Same code, [`src/solver/report.c:104`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/report.c#L104) and [`keywords.c:111`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/keywords.c#L111) |
| **6.0.0** | Not affected: `handle_report()` compares keywords exactly and skips the ones it does not know, NODESTATS included ([`src/engine/input/handlers/ControlsHandler.cpp:136`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/ControlsHandler.cpp#L136)) |
| **Since** | NODE has been listed before NODESTATS in every release in the repository (initial commit, 2014); 5.2.0 marked NODESTATS as accepted and ignored, but that branch is unreachable |
| **Fix** | Select the NODESTATS branch before the NODE one: [`IO-31_swmm530.patch`](IO-31_swmm530.patch) |

## The problem

`report_readOptions()` has an explicit case for the old NODESTATS keyword:

```c
        case 9: return 0;                          // NODESTATS deprecated
```

so `NODESTATS YES` is meant to be accepted and ignored. Instead, the test deck, a valid one-conduit model whose `[REPORT]` section has a `NODESTATS YES` line, is rejected by 5.2.4 and 5.3.0:

```
ERROR 209: undefined object YES at line 37 of [REPORT] section:
```

The message points at `YES` as a missing object, which sends the user looking for a node that was never meant to exist.

## Why it happens

The keyword is looked up with `findmatch()`, which returns the first keyword in the list that the token starts with (`match()` in [`input.c:805`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L805) is a prefix test; that is how `NODES`, `SUBCATCHMENTS` and `LINKS` match the keywords `NODE`, `SUBCATCH` and `LINK`):

```c
// src/legacy/engine/keywords.c
char* ReportWords[] = { w_DISABLED, w_INPUT, w_SUBCATCH, w_NODE, w_LINK,
                        w_CONTINUITY, w_FLOWSTATS, w_CONTROLS,
                        w_AVERAGES, w_NODESTATS, NULL};

// src/legacy/engine/report.c, report_readOptions()
    k = (char)findmatch(tok[0], ReportWords);
```

`NODESTATS` starts with `NODE` (index 3), so `findmatch()` returns 3 before it reaches index 9. The line is then read as a list of nodes to report, and `project_findObject(NODE, "YES")` fails with ERROR 209. (If the model happened to have a node named YES, the line would silently switch node reporting to that one node.)

## How to reproduce

| File | What it is |
|---|---|
| [`IO-31_nodestats.inp`](IO-31_nodestats.inp) | One junction with a 1 cfs inflow draining through one conduit to an outfall; `[REPORT]` has `NODESTATS YES` |
| [`IO-31_test.c`](IO-31_test.c) | Opens and runs the deck through the legacy toolkit; it must run without an error |
| [`IO-31_test6.c`](IO-31_test6.c) | The same check through the 6.0.0 API |

```sh
tools/run-test.sh IO-31            # 5.2.4, 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-31 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0):

```
[REPORT] NODESTATS YES: swmm_open returned 200, run error 200, 0 routing steps
FAIL: the deck is rejected with error 200; the report says "ERROR 209: undefined object YES at line 37 of [REPORT] section:"
IO-31 5.3.0 base: FAIL
```

6.0.0 runs the deck (`IO-31 6.0.0 base: PASS`).

**With the fix:**

```
[REPORT] NODESTATS YES: swmm_open returned 0, run error 0, 360 routing steps
PASS: the deprecated NODESTATS line is accepted and the model runs
IO-31 5.3.0 patched: PASS
```

## The fix

Take the NODESTATS branch whenever the token is NODESTATS, with the same prefix rule `findmatch()` uses:

```diff
     k = (char)findmatch(tok[0], ReportWords);
     if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);
+    if ( match(tok[0], w_NODESTATS) ) k = 9;  // NODE is a prefix of NODESTATS
```

The line's YES/NO value is still checked by the existing branch, and the other `[REPORT]` keywords are unaffected. Reordering `ReportWords` would also work but would renumber every case in the switch.

**Effect on other models.** None of the regression decks uses NODESTATS; decks without it are read exactly as before.
