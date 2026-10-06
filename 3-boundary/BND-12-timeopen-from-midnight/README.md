# BND-12: TIMEOPEN and TIMECLOSED count from midnight of the start date, not from the start of the run

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | In a run that does not start at midnight, every link has been "open" or "closed" for the start time of day at the first step. `IF PUMP P1 TIMEOPEN >= 4 THEN PUMP P1 STATUS = OFF` in a run starting at 06:00 switches P1 off on the first step instead of at 10:00. The toolkit's `swmm_LINK_TIMEOPEN` / `swmm_LINK_TIMECLOSED` values are 6 h too large as well. No warning. |
| **Reached from** | `[CONTROLS]` premises on `TIMEOPEN` / `TIMECLOSED` (and a `CURVE` / `TIMESERIES` action driven by them), and `swmm_getValue(swmm_LINK_TIMEOPEN / swmm_LINK_TIMECLOSED)`, in a run whose `START_TIME` is not 00:00, for a link that has not changed state since the start |
| **5.3.0** | `link_initState()` in [`src/legacy/engine/link.c:529`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L529) |
| **5.2.4** | Same code, [`src/solver/link.c:526`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L526) |
| **6.0.0** | Reproduces on purpose: `SWMMEngine` seeds `time_last_set` with `floor(start_date)` to match legacy, [`src/engine/core/SWMMEngine.cpp:979`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L979) |
| **Since** | 5.1.010, when TIMEOPEN and TIMECLOSED were added |
| **Fix** | Start the clock at the start date-time: [`BND-12_swmm530.patch`](BND-12_swmm530.patch), [`BND-12_swmm600.patch`](BND-12_swmm600.patch) |

## The problem

`TIMEOPEN` and `TIMECLOSED` are the time a link has been open (setting above 0) or closed since its state last changed. Until a link changes state, that is the time since the start of the run. SWMM counts it from midnight of the start date instead.

The test deck starts at 06:00 with pump P1 ON, pump P2 OFF and pump P3 ON:

```
RULE TOPEN
IF PUMP P1 TIMEOPEN >= 4
THEN PUMP P1 STATUS = OFF

RULE TCLOSED
IF PUMP P2 TIMECLOSED >= 2
THEN PUMP P2 STATUS = ON
```

P1 should run until 10:00 and P2 should start at 08:00. In 5.2.4, 5.3.0 and 6.0.0 both rules fire at 06:00, on the first step, because both pumps already count 6 hours. Through the toolkit, P3's `swmm_LINK_TIMEOPEN` reads 6.008 h after the first 30 s step.

Any run that does not start at midnight is shifted by its start time of day, so long as the link has not yet changed state. A start at 18:00, as in the regression deck `gate_control_3.inp`, makes every link 18 hours old at the first step.

## Why it happens

```c
// src/legacy/engine/link.c, link_initState()
    Link[j].setting   = 1.0;
    Link[j].targetSetting = 1.0;
    Link[j].timeLastSet = StartDate;
```

`StartDate` is the date part of the start (`StartDateTime = StartDate + StartTime`, `project.c:156`). The rule premise and the toolkit getter subtract it from the current date-time:

```c
// src/legacy/engine/controls.c, getVariableValue()
    case r_TIMEOPEN:
        ...
        return CurrentDate + CurrentTime - Link[j].timeLastSet;

// src/legacy/engine/swmm5.c, swmm_getValue() for swmm_LINK_TIMEOPEN
            return (getDateTime(NewRoutingTime) - link->timeLastSet) * 24.;
```

Once a link switches between open and closed, `routing.c` sets `timeLastSet` to the current date-time and the clock is right from then on.

6.0.0 copies the legacy value deliberately (`const double start_date0 = std::floor(ctx_.options.start_date);`, with a comment that legacy uses "the start DATE, midnight of the first day").

## How to reproduce

| File | What it is |
|---|---|
| [`BND-12_start-0600.inp`](BND-12_start-0600.inp) | 6-hour run starting at 06:00 with pumps P1 (ON, rule TOPEN), P2 (OFF, rule TCLOSED) and P3 (ON, no rule) |
| [`BND-12_test.c`](BND-12_test.c) | Steps the run through the legacy toolkit, records when P1 switches off and P2 on (each must be within one minute of 4 h and 2 h after the start) and reads P3's `swmm_LINK_TIMEOPEN` after the first step (must be under 0.02 h) |
| [`BND-12_test6.c`](BND-12_test6.c) | The rule checks through the 6.0.0 API, which has no time-open getter |

```sh
tools/run-test.sh BND-12            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-12 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0; 6.0.0 prints the same two rule lines):

```
Run starts at 06:00 (hours below are since the start)
  P3 TIMEOPEN after the first 30 s step: 6.008 h (expected 0.008)
  P1 switched off by TOPEN at    0.008 h (expected 4.000, i.e. 10:00)
  P2 switched on by TCLOSED at   0.008 h (expected 2.000, i.e. 08:00)
FAIL: TIMEOPEN/TIMECLOSED count from midnight: P1 off after 0.008 h, P2 on after 0.008 h, P3 TIMEOPEN 6.008 h at the first step
BND-12 5.2.4 base: FAIL
BND-12 5.3.0 base: FAIL
BND-12 6.0.0 base: FAIL
```

**With the fix** (5.3.0; 6.0.0 prints the same two rule lines):

```
Run starts at 06:00 (hours below are since the start)
  P3 TIMEOPEN after the first 30 s step: 0.008 h (expected 0.008)
  P1 switched off by TOPEN at    4.008 h (expected 4.000, i.e. 10:00)
  P2 switched on by TCLOSED at   2.008 h (expected 2.000, i.e. 08:00)
PASS: TIMEOPEN and TIMECLOSED count from the start of the run
BND-12 5.3.0 patched: PASS
BND-12 6.0.0 patched: PASS
```

The rules act at the 10:00 and 08:00 evaluations; the test reads the settings at the end of the step that follows, 30 s later.

## The fix

```diff
-    Link[j].timeLastSet = StartDate;
+    Link[j].timeLastSet = StartDateTime;
```

6.0.0 seeds `time_last_set` with `ctx_.options.start_date` (the start date-time) instead of its floor, and its comment is corrected.

A hot start file does not store `timeLastSet`, so after a hot start the clock still restarts; with the fix it restarts at the start of the new run instead of at midnight. Carrying the elapsed time across a hot start would need a change to the file format and is not part of this fix.

**Effect on other models.** `gate_control_3.inp` (start 18:00, `IF ORIFICE OF1 TIMEOPEN > 0:00 THEN ORIFICE OF1 SETTING = CURVE GATE_RATE`) is the only regression deck with a TIMEOPEN premise and a start time other than midnight. Its `.out` files are byte-identical with the patch in 5.3.0 and 6.0.0, because another rule closes OF1 at the first step and the clock restarts when the gate reopens. `gate_control_2.inp` (start at midnight) is also unchanged.
