# API-02: A refused out-of-order call blocks the project or aborts the run

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | One call in the wrong order, which the toolkit correctly refuses, also disables the project. `swmm_step` before `swmm_start` makes `swmm_start` fail with the same code (5.2.4: 502, 5.3.0: -999902); `swmm_start` called again during a run (5.3.0 also `swmm_useHotStart`) makes the next `swmm_step` fail, and the run stops after 10 of 120 steps with no mass balance or report. Only `swmm_close` + `swmm_open` recovers. 6.0.0 does the same through its engine state. |
| **Reached from** | Legacy toolkit: `swmm_step` or `swmm_stride` before `swmm_start`; `swmm_start` during a run; in 5.3.0 also `swmm_saveHotStart` before and `swmm_useHotStart` during a run. 6.0.0: any of `swmm_engine_open/initialize/start/step/stride/end/report` called in the wrong state |
| **5.3.0** | `swmm_start()`, `swmm_step()`, `swmm_stride()`, `swmm_useHotStart()`, `swmm_saveHotStart()` in [`src/legacy/engine/swmm5.c:695`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L695), [`:805`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L805), [`:865`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L865), [`:910`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L910), [`:941`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L941) |
| **5.2.4** | Same in `swmm_start()`, `swmm_step()`, `swmm_stride()`: [`src/solver/swmm5.c:326`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L326), [`:425`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L425), [`:483`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L483) |
| **6.0.0** | Reproduces by a different route: `SWMMEngine::set_error()` ([`src/engine/core/SWMMEngine.cpp:7988`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L7988)) moves the engine to `ERROR_STATE` also for the `SWMM_ERR_LIFECYCLE` refusals in `start()` ([`:1301`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L1301)), `step()` ([`:1517`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L1517)) and the other lifecycle calls |
| **Since** | 5.2.0, which introduced the `ERR_API_*` codes; 6.0.0's engine from its first release |
| **Fix** | Return the refusal without recording it as the run's error: [`API-02_swmm530.patch`](API-02_swmm530.patch), [`API-02_swmm600.patch`](API-02_swmm600.patch) |

## The problem

The toolkit protects itself against calls in the wrong order: `swmm_step` before `swmm_start` returns "simulation not started", a second `swmm_start` during a run returns "simulation not ended". The refusal is right, but it is stored as if the simulation had failed, so the project stays broken after the caller has noticed and corrected the mistake.

A wrapper that, for example, polls `swmm_step` once before starting, or a GUI that calls `swmm_start` twice on a double click, sees this in the test below (a 1-hour run with a 30-s routing step):

| Out-of-order call | 5.3.0 result |
|---|---|
| `swmm_step` before `swmm_start` | `swmm_start` then returns -999902 and the run never starts |
| `swmm_start` at step 10 of a running simulation | step 11 returns -999903; the run stops at 0.075 h of 1 h |
| `swmm_useHotStart` at step 10 | the same |

`swmm_end` then skips the mass balance and statistics, and `swmm_report` writes nothing, because both check `ErrorCode` first. 5.2.4 behaves the same with codes 502 and 503.

6.0.0 refuses the same calls with `SWMM_ERR_LIFECYCLE` (6) and ends up in the same place: `swmm_engine_step` before `swmm_engine_start` makes the start fail, and `swmm_engine_start`, `swmm_engine_initialize`, `swmm_engine_report` or `swmm_engine_open` called during a run make the next step fail.

## Why it happens

The legacy call-order checks assign the API code to the global `ErrorCode`, which is the simulation's fatal-error flag, and the same functions return `ErrorCode` before anything else:

```c
// src/legacy/engine/swmm5.c, swmm_start()
    if (ErrorCode)
        return ErrorCode;
    if (!IsOpenFlag)
        return (ErrorCode = ERR_API_NOT_OPEN);
    if (IsStartedFlag)
        return (ErrorCode = ERR_API_NOT_ENDED);

// src/legacy/engine/swmm5.c, swmm_step()
    if (ErrorCode)
        return ErrorCode;
    if (!IsOpenFlag)
        return (ErrorCode = ERR_API_NOT_OPEN);
    if (!IsStartedFlag)
        return (ErrorCode = ERR_API_NOT_STARTED);
```

Only `swmm_open` sets `ErrorCode` back to 0. The newer getters and setters in the same file (`setNodeValue()` and the rest) return their `ERR_API_*` codes without touching `ErrorCode`, which is the right pattern.

6.0.0 keeps an engine state instead of a global code, but every error goes through one function that also changes the state:

```cpp
// src/engine/core/SWMMEngine.cpp
int SWMMEngine::step(double* elapsed_time) noexcept {
    if (ctx_.state != EngineState::RUNNING) {
        if (elapsed_time) *elapsed_time = 0.0;
        set_error(SWMM_ERR_WRONG_STATE,
                  "swmm_engine_step: engine is not running");
        return SWMM_ERR_WRONG_STATE;
    }
...
void SWMMEngine::set_error(int code, const char* message) noexcept {
    ctx_.error_code    = code;
    ctx_.error_message = message ? message : "";
    ctx_.state         = EngineState::ERROR_STATE;
```

`start()` requires `INITIALIZED` and `step()` requires `RUNNING`, so after any refused lifecycle call neither can proceed.

## How to reproduce

| File | What it is |
|---|---|
| [`API-02_model.inp`](API-02_model.inp) | One subcatchment, one junction, one conduit, one outfall; 1 in/hr of rain; DYNWAVE, 30-s step, 1 hour |
| [`API-02_test.c`](API-02_test.c) | For each out-of-order call: open, make the call, start, run to the end. 3 cases on 5.2.4, 5 on 5.3.0 |
| [`API-02_test6.c`](API-02_test6.c) | The same for 6.0.0's seven lifecycle calls |

A case passes when the out-of-order call is refused, `swmm_start` then returns 0, no step fails, and the run gets to within one 30-s step of 1 h before the engine signals the end. (The legacy `swmm_step` returns elapsed time 0 on the step that reaches the end, so the last value it reports is 0.992 h.)

```sh
tools/run-test.sh API-02            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh API-02 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0:

```
out-of-order call                             its rc  start rc  steps reached(h)   last rc  verdict
swmm_step before swmm_start                  -999902   -999902      0      0.000   -999902  PROJECT BLOCKED
swmm_stride before swmm_start                -999902   -999902      0      0.000   -999902  PROJECT BLOCKED
swmm_start during the run (step 10)          -999903         0     10      0.075   -999903  PROJECT BLOCKED
swmm_saveHotStart before swmm_start          -999902   -999902      0      0.000   -999902  PROJECT BLOCKED
swmm_useHotStart during the run (step 10)    -999903         0     10      0.075   -999903  PROJECT BLOCKED
FAIL: in 5 of 5 cases a refused out-of-order call stopped the run from starting or finishing
API-02 5.3.0 base: FAIL
```

5.2.4:

```
swmm_step before swmm_start                      502       502      0      0.000       502  PROJECT BLOCKED
swmm_stride before swmm_start                    502       502      0      0.000       502  PROJECT BLOCKED
swmm_start during the run (step 10)              503         0     10      0.075       503  PROJECT BLOCKED
FAIL: in 3 of 3 cases a refused out-of-order call stopped the run from starting or finishing
API-02 5.2.4 base: FAIL
```

6.0.0:

```
out-of-order call                             its rc  start rc  steps reached(h)  last rc  verdict
swmm_engine_step before start                      6         6      0      0.000        6  PROJECT BLOCKED
swmm_engine_stride before start                    6         6      0      0.000        6  PROJECT BLOCKED
swmm_engine_end before start                       6         6      0      0.000        6  PROJECT BLOCKED
swmm_engine_start during the run (step 10)         6         0     10      0.075        6  PROJECT BLOCKED
swmm_engine_initialize during the run              6         0     10      0.075        6  PROJECT BLOCKED
swmm_engine_report during the run                  6         0     10      0.075        6  PROJECT BLOCKED
swmm_engine_open during the run                    6         0     10      0.075        6  PROJECT BLOCKED
FAIL: in 7 of 7 cases a refused out-of-order call stopped the run from starting or finishing
API-02 6.0.0 base: FAIL
```

**With the fix** every call is still refused with the same code, and every run completes:

```
swmm_step before swmm_start                  -999902         0    120      0.992         0  ok
swmm_stride before swmm_start                -999902         0    120      0.992         0  ok
swmm_start during the run (step 10)          -999903         0    120      0.992         0  ok
swmm_saveHotStart before swmm_start          -999902         0    120      0.992         0  ok
swmm_useHotStart during the run (step 10)    -999903         0    120      0.992         0  ok
PASS: every out-of-order call is refused and the run still starts and reaches the end
API-02 5.3.0 patched: PASS
```

```
swmm_engine_step before start                      6         0    121      1.000        0  ok
...
swmm_engine_open during the run                    6         0    121      1.000        0  ok
PASS: every out-of-order call is refused and the run still starts and reaches the end
API-02 6.0.0 patched: PASS
```

## The fix

5.3.0: return the call-order code without storing it, in the five places that store it today:

```diff
     if (IsStartedFlag)
-        return (ErrorCode = ERR_API_NOT_ENDED);
+        return ERR_API_NOT_ENDED;
 ...
     if (!IsStartedFlag)
-        return (ErrorCode = ERR_API_NOT_STARTED);
+        return ERR_API_NOT_STARTED;
```

The `ERR_API_NOT_OPEN` assignments are left alone: with no project open there is no run to protect, and `swmm_open` clears the code. One consequence: `swmm_getError` no longer reports a refused call, because it reads `ErrorCode`; the caller has the returned code, and `swmm_getErrorFromCode` turns it into text (once [API-03](../API-03-api-error-messages-empty/) gives the negative codes a message).

6.0.0: `set_error()` still records the code and message (so `swmm_get_last_error` reports the refusal), but no longer changes the state for `SWMM_ERR_WRONG_STATE`:

```diff
     ctx_.error_code    = code;
     ctx_.error_message = message ? message : "";
-    ctx_.state         = EngineState::ERROR_STATE;
+    // A call made in the wrong state is refused; it must not end the run.
+    if (code != SWMM_ERR_WRONG_STATE)
+        ctx_.state     = EngineState::ERROR_STATE;
```

That code is used only by the six lifecycle checks (`open`, `initialize`, `start`, `step`, `end`, `report`); input, file and numerical errors still put the engine in `ERROR_STATE`. Neither patch changes anything for a run whose calls are in the right order, so no model result changes.
