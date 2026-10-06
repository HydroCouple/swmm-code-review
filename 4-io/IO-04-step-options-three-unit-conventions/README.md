# IO-04: A bare number in a time option means hours or seconds depending on the option, and 6.0.0 reads it as seconds everywhere

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | 6.0.0 runs a different simulation period and report step from EPA SWMM for the same deck when [OPTIONS] times or steps are written as decimal hours. With `START_TIME 6.5`, `END_TIME 12.5`, `REPORT_STEP 0.25`, 5.2.4 and 5.3.0 run 06:30 to 12:30 and report every 15 minutes; 6.0.0 runs from 00:00:06 to 00:00:13 (a single routing step) and reports every 0.25 s. No warning. |
| **Reached from** | [OPTIONS] `START_TIME`, `END_TIME`, `REPORT_START_TIME`, `WET_STEP`, `DRY_STEP`, `REPORT_STEP`, `RULE_STEP` given as a bare number |
| **5.3.0** | Not affected: it defines the convention, in `project_readOption()` ([`src/legacy/engine/project.c:584-601`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L584-L601), [`:676-704`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L676-L704)) |
| **5.2.4** | Not affected, same code ([`src/solver/project.c:567`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L567), [`:659-687`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L659-L687)) |
| **6.0.0** | Reproduces: `handle_options()` reads all of these with `parse_time_seconds()` ([`OptionsHandler.cpp:280-285`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/OptionsHandler.cpp#L280-L285), [`:314-328`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/OptionsHandler.cpp#L314-L328), [`:351-352`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/OptionsHandler.cpp#L351-L352)), which takes a bare number as seconds ([`InputParseUtils.hpp:145-151`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/InputParseUtils.hpp#L145-L151)) |
| **Since** | 6.0.0's new input reader |
| **Fix** | Read a whole-token number as decimal hours in those options, as legacy does: [`IO-04_swmm600.patch`](IO-04_swmm600.patch) (requires [IO-02](../IO-02-time-options-accept-nan-inf-huge/)) |

## The problem

The legacy reader has three conventions for the time options in [OPTIONS]:

| Options | Bare number | `H:MM:SS` |
|---|---|---|
| `ROUTING_STEP`, `LENGTHENING_STEP` | seconds (`getDouble()`) | accepted (fallback to `datetime_strToTime()`) |
| `MINIMUM_STEP` | seconds (`getDouble()`) | refused, ERROR 211 |
| `START_TIME`, `END_TIME`, `REPORT_START_TIME`, `WET_STEP`, `DRY_STEP`, `REPORT_STEP`, `RULE_STEP` | **decimal hours** (`datetime_strToTime()`) | accepted |

So `ROUTING_STEP 900` is 900 seconds and `REPORT_STEP 900` is 900 hours (on a 6-hour run, 5.3.0 reports `Report Time Step ......... 06:00:00`), and `MINIMUM_STEP 0:00:01` stops the run with `ERROR 211: invalid number 0:00:01`. This is awkward, but it matches the documented units (seconds for the first two rows, `HH:MM:SS` for the third), and the one refusal comes with an error message. It is the format every existing deck was written for, and 6.0.0 is meant to read those decks the same way.

6.0.0 reads every one of these options with `parse_time_seconds()`, where a bare number is seconds. For the first two rows that matches legacy, and 6.0.0 also accepts `MINIMUM_STEP 0:00:01`. For the third row it changes the meaning of the deck. The test deck writes the run period and report step as decimal hours:

| Option | Legacy (5.2.4, 5.3.0) | 6.0.0 |
|---|---|---|
| `START_TIME 6.5` | 06:30:00 | 00:00:06 (6.5 s) |
| `END_TIME 12.5` | 12:30:00 | 00:00:13 (12.5 s) |
| `REPORT_STEP 0.25` | 900 s | 0.25 s |
| `ROUTING_STEP 30` | 30 s | 30 s |

6.0.0 simulates 6 seconds (a single routing step) instead of 6 hours, and writes a 326-byte .out file instead of 3,590 bytes. 6.0.0 already fixed the same mistake for rain gage intervals, whose comment reads "a bare decimal such as 0.08333 means 0.08333 hours (= 300 s); the generic parse_time_seconds() would mis-read it as 0.08333 s" ([`CatchmentHandler.cpp:354-366`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/CatchmentHandler.cpp#L354-L366)), and for [TIMESERIES] times (`parse_time_day_fraction()`), but not for [OPTIONS].

## Why it happens

Legacy reads the third group through `datetime_strToTime()`, which tries the whole token as decimal hours before `hr:min:sec`:

```c
// src/legacy/engine/datetime.c, datetime_strToTime()
//  Note:    accepts time as hr:min:sec or as decimal hours.
    *t = strtod(s, &endptr);
    if ( *endptr == 0 )
    {
        *t /= 24.0;
        return 1;
    }
```

6.0.0 reads them with the helper meant for `ROUTING_STEP`:

```cpp
// src/engine/input/InputParseUtils.hpp, parse_time_seconds()
//   - N        → N (plain seconds, floating point)
    if (ec == std::errc{} && ptr == sv.data() + sv.size()) {
        return val;  // plain number = seconds
    }
// src/engine/input/handlers/OptionsHandler.cpp, handle_options()
        } else if (key == "START_TIME") {
            start_time_part = parse_time_seconds(val) / datetime::SecsPerDay;
```

## How to reproduce

| File | What it is |
|---|---|
| [`IO-04_decimal-hours.inp`](IO-04_decimal-hours.inp) | One conduit, kinematic wave; `START_TIME 6.5`, `REPORT_START_TIME 6.5`, `END_TIME 12.5`, `REPORT_STEP 0.25`, `ROUTING_STEP 30` |
| [`IO-04_test.c`](IO-04_test.c) | Legacy toolkit: reads the start date, report step and routing step after `swmm_start`, runs the model and checks a 06:30 start, a 6-hour run, 900 s and 30 s |
| [`IO-04_test6.c`](IO-04_test6.c) | The same check with the 6.0.0 API |

```sh
tools/run-test.sh IO-04            # 5.2.4 and 5.3.0: PASS, 6.0.0: FAIL
tools/run-test.sh IO-04 --patched  # 6.0.0 with IO-02 and this fix: PASS
```

**Without the fix:**

```
---- IO-04 on 5.3.0 (base) ----
                       expected      read
start date (days)  43831.270833  43831.270833   (01/01/2020 06:30)
run length (h)           6.0000        5.9917
report step (s)             900           900
routing step (s)             30            30
error code 0, 719 steps
PASS: START_TIME 6.5, END_TIME 12.5 and REPORT_STEP 0.25 are read as decimal hours, ROUTING_STEP 30 as seconds
IO-04 5.3.0 base: PASS
---- IO-04 on 6.0.0 (base) ----
                       expected      read
start date (days)  43831.270833  43831.000075   (01/01/2020 06:30)
run length (h)           6.0000        0.0014
report step (s)             900          0.25
routing step (s)             30            30
error code 0, 1 steps
FAIL: decimal-hour START_TIME/END_TIME/REPORT_STEP not read as hours (start 43831.000075, run 0.0014 h, report step 0.25 s)
IO-04 6.0.0 base: FAIL
```

5.2.4 prints the same as 5.3.0. The legacy engine returns 0 instead of the final elapsed time, so its run length reads one 30-s step short; the test allows for that.

**With the fix:**

```
---- IO-04 on 6.0.0 (patched) ----
                       expected      read
start date (days)  43831.270833  43831.270833   (01/01/2020 06:30)
run length (h)           6.0000        6.0000
report step (s)             900           900
routing step (s)             30            30
error code 0, 720 steps
PASS: START_TIME 6.5, END_TIME 12.5 and REPORT_STEP 0.25 are read as decimal hours, ROUTING_STEP 30 as seconds
IO-04 6.0.0 patched: PASS
```

## The fix

In `handle_options()`, a token that is a whole number is taken as decimal hours for the seven options legacy reads with `datetime_strToTime()`, converted the way legacy converts it: `h / 24` days for a time of day, and whole seconds for a step (legacy rounds a step to the second in `datetime_decodeTime()`). Anything else is read as a clock time by a small helper, `clock_seconds()`, which calls `parse_time_seconds()` for a token containing `:` and otherwise returns NaN, so that a token that is neither (`abc`) fails IO-02's range check with ERROR 211 instead of being read as 0:

```diff
         } else if (key == "REPORT_STEP") {
-            const double step = parse_time_seconds(val);
+            double h = 0.0;   // a bare number is decimal hours, as in legacy
+            const double step = parse_double_strict(val, h)
+                ? std::floor(h / 24.0 * 86400.0 + 0.5) : clock_seconds(val);
```

```diff
         } else if (key == "START_TIME") {
-            const double t = parse_time_seconds(val) / datetime::SecsPerDay;
+            double h = 0.0;   // a bare number is decimal hours, as in legacy
+            const double t = parse_double_strict(val, h)
+                ? h / 24.0 : clock_seconds(val) / datetime::SecsPerDay;
```

`ROUTING_STEP`, `LENGTHENING_STEP` and `MINIMUM_STEP` keep seconds. The patch edits the lines that [IO-02](../IO-02-time-options-accept-nan-inf-huge/) adds range checks to, so it applies on top of `IO-02_swmm600.patch`.

**What changes for users.** Clock strings (`06:30:00`) are read exactly as before; with the patch, the test deck written in decimal hours gives a binary output file byte-identical to the same deck written with clock strings. A deck written for 6.0.0 that relied on a bare number meaning seconds in these seven options (outside the documented `HH:MM:SS` form) now gets hours, as in EPA SWMM. The 6.0.0 input reference describes `RULE_STEP` as "number or HH:MM:SS ... Interval, in seconds" ([`Chapter2-InputFileReference.md:158`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/docs/manuals/engine/sections/Chapter2-InputFileReference.md?plain=1#L158)); that row should say that a bare number is hours. None of the 73 regression decks writes these seven options as a bare number (all use `H:MM:SS`), so their results do not change.

Four of 6.0.0's own unit tests did rely on it: `test_dw_unsteady_friction.cpp`, `test_dw_tpa.cpp`, `test_fv_tpa_closure.cpp` and `test_fv_unsteady_friction.cpp` write `REPORT_STEP` as 0.5, 1, 2 or 5 meaning seconds. Read as hours, their 2- to 30-minute runs have no reporting period, and two tests that look for results in the output file fail (`DwUnsteadyFriction.ValveClosureK3EngagesAndStaysBoundedUnderExtran`, `FvTpa.SignedHeadsReachTheOutFile`); the rest pass only because two empty output files are identical. The patch rewrites those eight lines as clock strings (`00:00:00.5`, `00:00:01`, `00:00:02`, `00:00:05`), which give the same steps on the unpatched engine. If the project prefers bare seconds in these options, the alternative is to document that 6.0.0 departs from EPA SWMM here and warn when a bare number is used; but a deck written for EPA SWMM would then still run a different period.

The legacy conventions themselves are left as they are. Accepting `H:MM:SS` for `MINIMUM_STEP` would be a small convenience, but the current refusal comes with an error message, so it is not treated as a defect here.
