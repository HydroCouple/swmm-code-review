# API-03: 5.3.0's API error codes have no message text

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | Every API error in 5.3.0 comes back with an empty message: `swmm_getErrorFromCode()` returns `""` for all twelve codes -999901 to -999912, and `swmm_getError()` returns the code with an empty message. Bindings and GUIs that show the error text after a failed call show nothing. The 5.2.4 codes 501..509, which have text, are no longer returned, so callers that test for them also stop matching. |
| **Reached from** | `swmm_getError()` after an API error is stored as the current error (e.g. `swmm_start` with no project open); `swmm_getErrorFromCode()` with any `ERR_API_*` code |
| **5.3.0** | Codes declared in [`include/openswmm/legacy/engine/openswmm_solver.h:643`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/include/openswmm/legacy/engine/openswmm_solver.h#L643); messages looked up by `error_getMsg()` in [`src/legacy/engine/error.c:33`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/error.c#L33) from [`error.txt:127`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/error.txt#L127); `swmm_getError()` in [`swmm5.c:1218`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L1218) |
| **5.2.4** | Not affected: the codes are 501..509 ([`src/solver/error.h:165`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/error.h#L165)) and [`error.txt:127`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/error.txt#L127) has their text |
| **6.0.0** | Not affected: `swmm_error_message()` has text for every code the C API returns on misuse ([`src/engine/core/openswmm_engine_impl.cpp:339`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_engine_impl.cpp#L339)) |
| **Since** | 5.3.0. The fork renumbered the codes in commit 9cad38b8 (2024) together with matching `ERR(-999901, ...)` entries, but those entries lived in the `src/openswmm++/solver/error.txt` copy, which the refactor in eb2ddc6c / 03ed283a ("Finalizing refactor and documention", 2026-03-25) deleted; the legacy engine kept the 5.2 `error.txt` |
| **Fix** | Replace the stale 501..509 entries with the twelve negative codes and look up the text for any non-zero code: [`API-03_swmm530.patch`](API-03_swmm530.patch) |

## The problem

In 5.2.4 a failed toolkit call can be explained to the user:

```
swmm_start with no project open returned 501
swmm_getError returned 501, message "API Error 501: project not opened."
```

5.3.0 returns a new code and no text:

```
swmm_start with no project open returned -999901
swmm_getError returned -999901, message ""
```

and `swmm_getErrorFromCode()`, which 5.3.0 added to turn a returned code into a message, gives `""` for every one of its own API codes.

## Why it happens

5.3.0 declares the API errors as an enum in the public header:

```c
// include/openswmm/legacy/engine/openswmm_solver.h
    ERR_API_NOT_OPEN = -999901,
    ERR_API_NOT_STARTED = -999902,
    ...
    ERR_API_IS_RUNNING = -999912,
```

The message table is still the one from 5.2:

```c
// src/legacy/engine/error.txt
// API Error Keys
ERR(500,"\n  ERROR 500: System exception thrown.")
ERR(501,"\n  API Error 501: project not opened.")
ERR(502,"\n  API Error 502: simulation not started.")
...
ERR(509,"\n  API Error 509: invalid time period.")
```

`error_getMsg()` expands that table into a `switch`; a code with no `case` falls to `default: strcpy(msg, "")`. `swmm_getError()` has a second filter, written when every error code was positive:

```c
// src/legacy/engine/swmm5.c, swmm_getError()
    if (ErrorCode > 0 && strlen(ErrorMsg) == 0)
        error_getMsg(ErrorCode, ErrorMsg);
```

No code in 5.3.0 returns 501..509 any more, so those nine entries are dead.

## How to reproduce

| File | What it is |
|---|---|
| [`API-03_test.c`](API-03_test.c) | Calls `swmm_start` with no project open and reads `swmm_getError`; in 5.3.0 also calls `swmm_getErrorFromCode` for each of the twelve `ERR_API_*` codes |
| [`API-03_test6.c`](API-03_test6.c) | Provokes four errors through 6.0.0's C API (wrong state, NULL handle, index out of range, missing file) and checks `swmm_error_message` and `swmm_get_last_error_msg` |
| [`API-03_model.inp`](API-03_model.inp) | A small valid model, opened by the 6.0.0 test for the index check |

The test passes when every code it sees has a non-empty message.

```sh
tools/run-test.sh API-03            # 5.2.4: PASS, 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh API-03 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0:

```
swmm_start with no project open returned -999901
swmm_getError returned -999901, message ""

     code  swmm_getErrorFromCode message
  -999901  ""
  -999902  ""
...
  -999912  ""
FAIL: 13 of 13 API error codes have an empty message
API-03 5.3.0 base: FAIL
```

6.0.0:

```
swmm_engine_step before open             rc   6  swmm_error_message: "Function called in wrong lifecycle state"
                                                swmm_get_last_error 6: "swmm_engine_step: engine is not running"
swmm_engine_initialize(NULL)             rc   7  swmm_error_message: "NULL or invalid engine handle"
swmm_node_get_depth(index 999)           rc   8  swmm_error_message: "Object index out of range"
swmm_engine_open(missing file)           rc   2  swmm_error_message: "Cannot open input file"
PASS: every error code checked (4) has a message
API-03 6.0.0 base: PASS
```

**With the fix**, 5.3.0:

```
swmm_start with no project open returned -999901
swmm_getError returned -999901, message "API Error -999901: project not opened."

     code  swmm_getErrorFromCode message
  -999901  "API Error -999901: project not opened."
  -999902  "API Error -999902: simulation not started."
  -999903  "API Error -999903: simulation not ended."
  -999904  "API Error -999904: invalid object type."
  -999905  "API Error -999905: invalid object index."
  -999906  "API Error -999906: invalid object name."
  -999907  "API Error -999907: invalid property type."
  -999908  "API Error -999908: invalid property value."
  -999909  "API Error -999909: invalid time period."
  -999910  "API Error -999910: cannot open hot start file."
  -999911  "API Error -999911: invalid hot start file format."
  -999912  "API Error -999912: simulation is running."
PASS: every API error code checked (13) has a message
API-03 5.3.0 patched: PASS
```

## The fix

The table gets the codes 5.3.0 actually returns, with the wording of the 5.2 messages (and of the fork's earlier `error.txt`, minus a `%s` that `error_getMsg()` would print literally):

```diff
 ERR(500,"\n  ERROR 500: System exception thrown.")
-ERR(501,"\n  API Error 501: project not opened.")
 ...
-ERR(509,"\n  API Error 509: invalid time period.")
+ERR(-999901,"\n  API Error -999901: project not opened.")
 ...
+ERR(-999912,"\n  API Error -999912: simulation is running.")
```

and `swmm_getError()` looks up the text for any non-zero code:

```diff
-    if (ErrorCode > 0 && strlen(ErrorMsg) == 0)
+    if (ErrorCode != 0 && strlen(ErrorMsg) == 0)
         error_getMsg(ErrorCode, ErrorMsg);
```

Only message text changes; no simulation result or return code does. Restoring the old numbers (501..509) instead would be a second incompatible change for 5.3.0 callers, so the patch keeps the new ones. `swmm_getErrorFromCode()` also has an odd parameter type (`char *outErrMsg[1024]`) and writes up to 1025 bytes; that is [CRASH-17](../../5-crashes/CRASH-17-getname-geterror-buffer-overrun/).

6.0.0 note: its public enum also declares `SWMM_ERR_DEPENDENCY = 15`, which `swmm_error_message()` answers with its "Unknown error" fallback. No 6.0.0 function returns that code today, so it is not part of this issue.
