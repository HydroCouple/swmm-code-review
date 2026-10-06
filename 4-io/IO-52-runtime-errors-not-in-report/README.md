# IO-52: Errors 107 and 307 stop the run but are not written to the report

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A run that stops because no valid time step can be computed (ERROR 107) or because the binary results file cannot be opened (ERROR 307) leaves a report with no error message and no results: it ends after the options block (or after "Control Actions Taken") and the analysis times. The command-line program prints "There are errors." and nothing in the report says which. |
| **Reached from** | ERROR 307: any input file run with a results file path that cannot be created (missing directory, no write permission). ERROR 107: a zero routing or runoff step during the run, for example through the `[EVENTS]`/`RULE_STEP` defect [BND-10](../../3-boundary/BND-10-events-skip-rule-step-error-107/) |
| **5.3.0** | `ErrorCode = ERR_TIMESTEP` in `execRouting()`, [`src/legacy/engine/swmm5.c:969`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L969) and `runoff_execute()`, [`runoff.c:224`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L224); `ErrorCode = ERR_OUT_FILE` in `output_openOutFile()`, [`output.c:460`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L460). The function meant to write them, `report_writeErrorCode()` ([`report.c:1446`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/report.c#L1446)), has no caller. |
| **5.2.4** | Same code: [`src/solver/swmm5.c:535`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L535), [`runoff.c:220`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/runoff.c#L220), [`output.c:451`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/output.c#L451), uncalled [`report.c:1446`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/report.c#L1446) |
| **6.0.0** | Not affected: the report plugin writes the error of a failed run into the report (for example `ERROR 14: the routing solution diverged ...`), and the `[EVENTS]` deck that stops 5.2.4/5.3.0 with ERROR 107 runs to completion. For an unopenable results file the message it writes is generic: `swmm_engine_start: plugin prepare() failed`. |
| **Since** | 5.2.0 (commit 65e8435e, "Release 5.2.0"): 5.1.015's `swmm_report()` began with `if ( ErrorCode ) report_writeErrorCode();`, and 5.2.0 replaced that with `if (!ErrorCode) report_writeReport();` |
| **Fix** | Raise the three errors with `report_writeErrorMsg()`: [`IO-52_swmm530.patch`](IO-52_swmm530.patch) |

## The problem

SWMM has two ways to stop a run with an error. Most errors are raised with `report_writeErrorMsg(code, ...)`, which writes `ERROR code: message` to the report at once. A few are raised by assigning `ErrorCode` directly, and were written later by `report_writeErrorCode()`, which handles exactly those codes (101-107, 301-307 and 500). Up to 5.1.015 `swmm_report()` called it when `ErrorCode` was set. Release 5.2.0 changed `swmm_report()` to write results only when there is no error, and the call went with it. 5.2.4 and 5.3.0 still contain `report_writeErrorCode()`, but nothing calls it.

Two of those errors are easy to reach:

- **ERROR 307**: the binary results file cannot be opened. Run any valid model with an output path in a directory that does not exist. The console shows "Cannot open output file", `swmm_start()` returns 307, and the report ends after the analysis options:

  ```
    Head Tolerance ........... 0.005000 ft
    

    Analysis begun on:  Mon Oct  5 17:47:30 2026
    Analysis ended on:  Mon Oct  5 17:47:30 2026
    Total elapsed time: < 1 sec
  ```

- **ERROR 107**: a routing or runoff step of zero. The test deck reaches it through [BND-10](../../3-boundary/BND-10-events-skip-rule-step-error-107/) (`[EVENTS]` with a 45 s fixed routing step and a 60 s `RULE_STEP`): the run stops at 3:00 and the report ends with an empty "Control Actions Taken" section and the analysis times.

In both cases the report looks like a run that wrote nothing, not one that failed. The command-line program also exits with status 0 ([API-18](../../6-api/API-18-cli-exit-code-zero-on-error/)), so a batch script cannot tell either.

## Why it happens

```c
// src/legacy/engine/swmm5.c, execRouting()
if (routingStep <= 0.0)
{
    ErrorCode = ERR_TIMESTEP;        // not written anywhere
    return;
}

// src/legacy/engine/output.c, output_openOutFile()
if ( (Fout.file = fopen(Fout.name, "w+b")) == NULL)
{
    writecon(FMT14);                 // console only
    ErrorCode = ERR_OUT_FILE;
}

// src/legacy/engine/swmm5.c, swmm_report()   (5.1.015: if ( ErrorCode ) report_writeErrorCode();)
if (!ErrorCode)
    report_writeReport();
```

`swmm_run()` calls `swmm_report()` only when `ErrorCode` is 0, and `swmm_end()` skips the continuity and summary tables when it is set. So the error is never written.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-52_simple.inp`](IO-52_simple.inp) | A valid one-conduit model, run with the results file `no_such_dir/IO-52a.out` (case A) |
| [`IO-52_events-rule-step.inp`](IO-52_events-rule-step.inp) | `[EVENTS]` with `ROUTING_STEP 45` and `RULE_STEP 00:01:00`, which stops 5.2.4 and 5.3.0 with ERROR 107 at 3:00 (case B, through BND-10) |
| [`IO-52_test.c`](IO-52_test.c) | 5.2.4/5.3.0: runs both cases and looks for `ERROR <code>:` in each report |
| [`IO-52_test6.c`](IO-52_test6.c) | 6.0.0: runs both cases and looks for the API's error message in each report |

Case B needs BND-10 to produce the error; with BND-10 fixed the run completes and the case passes with nothing to check. Case A does not depend on any other issue.

```sh
tools/run-test.sh IO-52            # 5.2.4, 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-52 --patched  # 5.3.0: PASS (6.0.0 unpatched: PASS)
```

**Without the fix** (5.2.4 and 5.3.0 print the same):

```
Case A: results file in a directory that does not exist
    error code returned: 307
    "ERROR 307" in IO-52a.rpt: no

Case B: [EVENTS] and RULE_STEP (zero routing step)
    error code returned: 107
    "ERROR 107" in IO-52b.rpt: no
FAIL: the run stopped with error 307 (case A) and 107 (case B) but the report does not say so
IO-52 5.2.4 base: FAIL
IO-52 5.3.0 base: FAIL
```

6.0.0:

```
Case A: results file in a directory that does not exist
    error code returned: 10
    error message: swmm_engine_start: plugin prepare() failed
    report:   swmm_engine_start: plugin prepare() failed
    message in IO-52a6.rpt: yes

Case B: [EVENTS] and RULE_STEP
    error code returned: 0
    the run completed, nothing to report
PASS: every error that stopped a run is written to its report
IO-52 6.0.0 base: PASS
```

**With the fix**:

```
Case A: results file in a directory that does not exist
    error code returned: 307
    report:   ERROR 307: cannot open binary results file.
    "ERROR 307" in IO-52a.rpt: yes

Case B: [EVENTS] and RULE_STEP (zero routing step)
    error code returned: 107
    report:   ERROR 107: cannot compute a valid time step.
    "ERROR 107" in IO-52b.rpt: yes
PASS: every error that stopped a run is written to its report
IO-52 5.3.0 patched: PASS
```

The case B report now ends:

```
  *********************
  Control Actions Taken
  *********************

  
  ERROR 107: cannot compute a valid time step.

  Analysis begun on:  Mon Oct  5 17:47:23 2026
```

## The fix

Raise the errors the way every other run-time error is raised:

```diff
         if (routingStep <= 0.0)
         {
-            ErrorCode = ERR_TIMESTEP;
+            report_writeErrorMsg(ERR_TIMESTEP, "");
             return;
         }
```

and the same in `runoff_execute()` (107) and `output_openOutFile()` (307). `report_writeErrorMsg()` writes the message to the report and sets `ErrorCode` (and the message returned by `swmm_getError()`), so the code that follows is unchanged. Restoring a call to `report_writeErrorCode()` instead would write some messages twice, since ERROR 101 (memory) is also raised with `report_writeErrorMsg()` in several places.

The same direct assignment remains in the Windows-only exception handlers (`ERR_SYSTEM`, 500, in `swmm5.c`) and in three allocation failures (`ERR_MEMORY`, 101, in `infil.c` and `lid.c`), which cannot be triggered in a test; they can be changed the same way. Only runs that stop with these errors change: their report now ends with the error message. No results change.
