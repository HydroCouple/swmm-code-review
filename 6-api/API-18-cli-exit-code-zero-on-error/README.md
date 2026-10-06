# API-18: The command line programs exit with status 0 when the run fails

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | `runswmm` (5.2.4) and `openswmm-legacy` (5.3.0) return exit status 0 after any failed run: an input error (ERROR 211), a missing input file (ERROR 303), any run-time error. They print "There are errors." and nothing more. Batch scripts, CI jobs and calibration drivers that check `$?` treat the run as good and go on to read a report with no results. 6.0.0's `openswmm` returns non-zero for open/initialize/start failures, but 0 when a simulation step fails ("ERROR 14: the routing solution diverged ... the run cannot continue"). |
| **Reached from** | Running the command line program on an input that fails |
| **5.3.0** | `main()` in [`src/legacy/cli/main.c:93`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/cli/main.c#L93) and [`:104`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/cli/main.c#L104) |
| **5.2.4** | Same: [`src/run/main.c:92`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/run/main.c#L92) and [`:103`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/run/main.c#L103) |
| **6.0.0** | Partly: `main()` in [`src/cli/main.cpp`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/cli/main.cpp#L87) returns the error code when open, initialize or start fail, but after a failed `swmm_engine_step` it breaks out of the loop ([`:115`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/cli/main.cpp#L115)) and returns 0 ([`:141`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/cli/main.cpp#L141)) |
| **Since** | Every release: 5.1.015's `src/run/main.c` already ends with `return 0` under a header that says "returns error status" |
| **Fix** | Return a non-zero status when the run failed: [`API-18_swmm530.patch`](API-18_swmm530.patch), [`API-18_swmm600.patch`](API-18_swmm600.patch) |

## The problem

The legacy command line stub runs the model and reports the outcome only as text:

```
$ openswmm-legacy API-18_bad-number.inp API-18_bad-number.rpt API-18_bad-number.out
...
 o  Retrieving project data

... OPEN-SOURCE SWMM completed in 0.00 seconds. There are errors.
$ echo $?
0
```

The report holds `ERROR 211: invalid number 1O0 at line 32 of [JUNC] section`, and no results. The same happens when the input file does not exist (then no report is written at all). A script has to parse the console text or the report to find out.

6.0.0's `openswmm` returns the error code for failures before the first step (exit 2 for a missing file), but not for a failure during the run. Its only run-time step error is the divergence guard: when a head or a flow passes 1e9 or is not finite, `swmm_engine_step` returns `ERROR 14`, the CLI prints `Error at step 1: ERROR 14: the routing solution diverged at link 'C1' (flow 2.26256e+09) after 0.0001 hours -- the run cannot continue.`, writes that line into the report, and exits with 0.

## Why it happens

```c
// src/legacy/cli/main.c, main()   (5.2.4: src/run/main.c)
* \return Error status
...
        swmm_run(inputFile, reportFile, binaryFile);
        ...
        if      ( swmm_getError(errMsg, msgLen) > 0 ) printf(" There are errors.\n");
        else if ( swmm_getWarnings() > 0 ) printf(" There are warnings.\n");
        else printf("\n");
    }
    ...
    return 0;
```

```cpp
// src/cli/main.cpp, main()
    while (true) {
        err = swmm_engine_step(engine, &elapsed);
        if (err != SWMM_OK) {
            std::printf("\nError at step %ld: %s\n", step_count,
                        swmm_get_last_error_msg(engine));
            break;
        }
        ...
    }
    ...
    return 0;
```

## How to reproduce

| File | What it is |
|---|---|
| [`API-18_clean.inp`](API-18_clean.inp) | A valid model (should exit 0) |
| [`API-18_bad-number.inp`](API-18_bad-number.inp) | The same with junction J1's invert written `1O0` (letter O) |
| [`API-18_diverges.inp`](API-18_diverges.inp) | A 2e9 cfs inflow into a 1000 ft x 20000 ft channel: 6.0.0 stops with ERROR 14; the legacy engines run it to the end without an error |
| [`API-18_test.c`](API-18_test.c) | Finds the command line program built next to the engine library the test is linked with (`dladdr`), runs it on the three decks and on a missing file, and checks that the exit status is non-zero exactly when the report has an `ERROR` line or was not written |
| [`API-18_test6.c`](API-18_test6.c) | The same for 6.0.0's `openswmm` |

```sh
tools/run-test.sh API-18            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh API-18 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0 (5.2.4's `runswmm` prints the same table):

```
command line program: openswmm-legacy

input                report says                                                      exit  verdict
API-18_clean         (no error)                                                          0  ok
API-18_bad-number    ERROR 211: invalid number 1O0 at line 32 of [JUNC] section:         0  FAILED RUN EXITS 0
API-18_missing       (no report written)                                                 0  FAILED RUN EXITS 0
API-18_diverges      (no error)                                                          0  ok
FAIL: in 2 of 4 runs the exit status does not match the report (a failed run exits with status 0)
API-18 5.3.0 base: FAIL
```

6.0.0:

```
command line program: openswmm

input                report says                                                      exit  verdict
API-18_clean         (no error)                                                          0  ok
API-18_bad-number    (no error)                                                          0  ok
API-18_missing       (no report written)                                                 2  ok
API-18_diverges      ERROR 14: the routing solution diverged at link 'C1' (flow 2        0  FAILED RUN EXITS 0
FAIL: in 1 of 4 runs the exit status does not match the report (a failed run exits with status 0)
API-18 6.0.0 base: FAIL
```

(6.0.0 accepts `1O0` as an invert of 1.0 and runs the model; that is a separate input-parsing defect, reported on its own, and the test treats it as the clean run it reports.)

**With the fix**:

```
API-18_clean         (no error)                                                          0  ok
API-18_bad-number    ERROR 211: invalid number 1O0 at line 32 of [JUNC] section:         1  ok
API-18_missing       (no report written)                                                 1  ok
API-18_diverges      (no error)                                                          0  ok
PASS: failed runs exit with a non-zero status and clean runs with 0
API-18 5.3.0 patched: PASS
```

```
API-18_missing       (no report written)                                                 2  ok
API-18_diverges      ERROR 14: the routing solution diverged at link 'C1' (flow 2       14  ok
PASS: failed runs exit with a non-zero status and clean runs with 0
API-18 6.0.0 patched: PASS
```

## The fix

5.3.0 keeps the code `swmm_getError()` already returns and turns it into the exit status:

```diff
-        if      ( swmm_getError(errMsg, msgLen) > 0 ) printf(" There are errors.\n");
+        errorCode = swmm_getError(errMsg, msgLen);
+        if      ( errorCode > 0 ) printf(" There are errors.\n");
 ...
-    return 0;
+    // --- exit status tells the caller whether the run failed
+    return errorCode ? 1 : 0;
```

The status is 1 rather than the SWMM error code because exit statuses are taken modulo 256 (error 303 would show as 47). 6.0.0 returns the step's error code, as its CLI already does for the earlier stages:

```diff
-    return 0;
+    return err;  // non-zero when a step failed
```

`err` is `SWMM_OK` after a run that completes, so clean runs still exit 0, and nothing in the engines changes. The legacy program also exits 0 for a wrong number of arguments; that is left as is.
