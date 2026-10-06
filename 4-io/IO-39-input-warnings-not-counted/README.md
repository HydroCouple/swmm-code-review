# IO-39: Unknown-option and unknown-section warnings are not counted, and the section warning is written twice

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A misspelt `[OPTIONS]` keyword is replaced by its default (e.g. `FLOW_ROUTNG KINWAVE` leaves the run on dynamic wave, the default) while `swmm_getWarnings()` returns 0 and the command-line program ends with "... completed" and no "There are warnings". The only trace is a line in the report. An unknown section is reported twice, so a host that counts warning callbacks sees 3 warnings for 2 problems. In 6.0.0 the warnings are counted, but the duplicate is too (3) |
| **Reached from** | Input files with an unknown `[OPTIONS]` keyword or an unknown section name |
| **5.3.0** | `input_countObjects()` and `input_readData()` in [`src/legacy/engine/input.c:116`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L116) and [`input.c:226`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L226); `project_readOption()` at [`project.c:470`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L470); only `report_writeWarningMsg()` counts warnings ([`report.c:1509`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/report.c#L1509)) |
| **5.2.4** | Not affected: both lines are input errors (ERROR 205) that stop the run ([`src/solver/input.c:116`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/input.c#L116), [`project.c:462`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L462)) |
| **6.0.0** | Reproduces the duplicate on purpose, for parity with 5.3.0: `SWMMEngine::open()` adds each unknown-section warning to the warning list and the callback twice ([`src/engine/core/SWMMEngine.cpp:554`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L554)) |
| **Since** | 5.3.0, fork commit a2869102 ("Optional options and sections in legacy", May 2026); 6.0.0 copied the duplicate in e830b754 (October 2026) |
| **Fix** | Count the two warnings and issue the section warning in one pass only: [`IO-39_swmm530.patch`](IO-39_swmm530.patch); drop 6.0.0's second copy: [`IO-39_swmm600.patch`](IO-39_swmm600.patch) Apply after IO-28 (its `Requires:` line). |

## The problem

5.3.0 made the reader tolerant: an unknown `[OPTIONS]` keyword or an unknown section, which 5.2.4 rejects with ERROR 205, now produces a warning and is skipped. That makes the warning the only signal that part of the input was ignored, so it has to reach the user. It does not reliably:

- `swmm_getWarnings()` stays 0, and the command-line program, which prints "There are warnings." when it is above 0, prints only "... completed".
- The unknown-section warning appears twice in the report and is passed twice to the warning callback.

The test deck has two problems, `FLOW_ROUTNG DYNWAVE` (misspelt `FLOW_ROUTING`) and a `[FOO]` section. 5.3.0's report has three WARNING lines and `swmm_getWarnings()` returns 0:

```
  WARNING: Unknown option keyword 'FLOW_ROUTNG' in [OPTIONS] section - option will be ignored.

  WARNING: Unknown section '[FOO]' at line 14 will be skipped.

  WARNING: Unknown section '[FOO]' at line 14 will be skipped.
```

6.0.0 writes the same three lines (the section warning first) and `swmm_get_warning_count()` returns 3.

## Why it happens

The new warnings are written with `report_writeLine()` and passed to the callback directly:

```c
// src/legacy/engine/input.c, input_countObjects() (line 116) and input_readData() (line 226)
                // --- unknown section: warn and skip
                char warnMsg[MAXLINE+1];
                snprintf(warnMsg, MAXLINE,
                    "\n  WARNING: Unknown section '%s' at line %ld will be skipped.", tok, lineCount);
                report_writeLine(warnMsg);
                report_invokeWarningCallback(warnMsg);

// src/legacy/engine/project.c, project_readOption()
        report_writeLine(warnMsg);
        report_invokeWarningCallback(warnMsg);
        return 0;
```

Every other warning goes through `report_writeWarningMsg()`, which is where the counter is kept (`Warnings++;`, report.c:1509). The input file is read twice, once to count the objects and once to read their data, and both loops contain the section warning. The option warning is issued only once because `[OPTIONS]` lines are read only in the counting pass.

6.0.0 records each unknown header once while reading, but `SWMMEngine::open()` then emits it twice, by design ("legacy reads the file twice ... and warns for every unknown header on each pass"):

```cpp
// src/engine/core/SWMMEngine.cpp, SWMMEngine::open()
        ctx_.warnings.insert(ctx_.warnings.begin(), msgs.begin(), msgs.end());
        for (const auto& m : msgs) emit_warning(100, m.c_str());
        for (const auto& m : msgs) push_report_warning(m, 100);   // adds and emits again
```

(5.2.4, for comparison, reports `ERROR 205: invalid keyword FLOW_ROUTNG` three times: for the option line, for the `[FOO]` header and for the data line under it. The last two name the wrong token, because the count pass sets the error code for an unknown section without updating the error text. 5.3.0 replaced that code.)

## How to reproduce

| File | What it is |
|---|---|
| [`IO-39_typos.inp`](IO-39_typos.inp) | A one-conduit model with `FLOW_ROUTNG DYNWAVE` in `[OPTIONS]` and an unknown `[FOO]` section |
| [`IO-39_test.c`](IO-39_test.c) | Runs the deck through the legacy toolkit and compares the WARNING lines in the report and `swmm_getWarnings()` with the 2 problems; an input error that stops the run (5.2.4) also passes |
| [`IO-39_test6.c`](IO-39_test6.c) | The same check through the 6.0.0 API, with `swmm_get_warning_count()` |

```sh
tools/run-test.sh IO-39            # 5.2.4: PASS (input errors); 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-39 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix:**

```
---- IO-39 on 5.2.4 (base) ----
  swmm_open error         200
  ERROR lines in report   3
  WARNING lines in report 0
  swmm_getWarnings()      0
PASS: the problems are reported as input errors and the run is stopped
---- IO-39 on 5.3.0 (base) ----
  swmm_open error         0
  ERROR lines in report   0
  WARNING lines in report 3
  swmm_getWarnings()      0
FAIL: 2 problems give 3 WARNING lines in the report and swmm_getWarnings() = 0 (both should be 2)
---- IO-39 on 6.0.0 (base) ----
  WARNING lines in report 3
  swmm_get_warning_count  3
FAIL: 2 problems give 3 WARNING lines in the report and swmm_get_warning_count() = 3 (both should be 2)
```

**With the fix**, 5.3.0 and 6.0.0 print the same counts:

```
  WARNING lines in report 2
  swmm_getWarnings()      2
PASS: each problem is written once as a warning and counted
IO-39 5.3.0 patched: PASS
IO-39 6.0.0 patched: PASS
```

## The fix

5.3.0: add `Warnings++;` where the section warning (counting pass) and the option warning are issued, and remove the section warning from `input_readData()`, which still skips the section:

```diff
                 report_writeLine(warnMsg);
                 report_invokeWarningCallback(warnMsg);
+                Warnings++;
                 sect = -1;
 ...
-                // --- unknown section: warn and skip until next known section
-                char warnMsg[MAXLINE+1];
-                snprintf(warnMsg, MAXLINE,
-                    "\n  WARNING: Unknown section '%s' at line %ld will be skipped.", Tok[0], lineCount);
-                report_writeLine(warnMsg);
-                report_invokeWarningCallback(warnMsg);
+                // --- unknown section: skip until next known section
+                //     (input_countObjects has already warned about it)
                 sect = -1;
```

The warning text is unchanged. The messages are kept as written rather than routed through `report_writeWarningMsg()`, so the report looks the same apart from the removed duplicate.

6.0.0: remove the `push_report_warning()` loop, so each unknown section is added to the warning list and sent to the callback once.

Whether a misspelt `[OPTIONS]` keyword should be a warning at all, rather than ERROR 205 as in 5.2.4, is a design decision this patch leaves alone; with the count fixed, a host or the command line at least reports that there are warnings.

**Effect on other models.** None of the regression decks has an unknown section or option keyword; their reports and warning counts are unchanged.
