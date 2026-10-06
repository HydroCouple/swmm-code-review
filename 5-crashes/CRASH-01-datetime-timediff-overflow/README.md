# CRASH-01: datetime_timeDiff() overflows an int conversion once a rain series has ended

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Undefined behaviour at every runoff step after a rain gage's series ends. No wrong number results on current compilers, because the caller only tests the sign, but it stops every sanitizer build (it stops a sanitizer build on 68 of the review's 249 test decks (the 73 regression-suite decks plus 176 from the OpenSWMM repository) in 5.2.4 and 73 in 5.3.0) and a compiler is free to do anything with it. |
| **Reached from** | Any input file with a rain gage whose time series or file ends before the simulation does |
| **5.3.0** | `datetime_timeDiff()` in [`src/legacy/engine/datetime.c:432`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/datetime.c#L432) |
| **5.2.4** | Same code, [`src/solver/datetime.c:432`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/datetime.c#L432) |
| **6.0.0** | `datetime::timeDiff()` in [`src/engine/core/DateTime.hpp:198`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/DateTime.hpp#L198) converts to `long` instead of `int`. That is enough on 64-bit Linux and macOS, but not on Windows (32-bit `long`) or for out-of-range arguments to the public `swmm_datetime_time_diff()` |
| **Since** | Every release in the repository's history: the same line is in the oldest tagged source, 5.1.002 (`src/datetime.c`) |
| **Fix** | Compute in double and clamp to the range of `long`: [`CRASH-01_swmm530.patch`](CRASH-01_swmm530.patch), [`CRASH-01_swmm600.patch`](CRASH-01_swmm600.patch) |

## The problem

`runoff_getTimeStep()` limits the runoff time step to the time until each gage's next rainfall value:

```c
// src/legacy/engine/runoff.c, runoff_getTimeStep()
for (j = 0; j < Nobjects[GAGE]; j++)
{
    timeStep = datetime_timeDiff(gage_getNextRainDate(j, currentDate), currentDate);
    if ( timeStep > 0 && timeStep < maxStep ) maxStep = timeStep;
}
```

Once a gage's series has no more values, `gage_getNextRainDate()` returns `NO_DATE`, which is day -693594 (1 January 0001). For a run in 2020 the difference is -6.37e10 seconds, and `datetime_timeDiff()` puts it through an `int`:

```c
// src/legacy/engine/datetime.c, datetime_timeDiff()
secs = (int)(floor((d1 - d2)*SecsPerDay + 0.5));   // -6.37e10 does not fit in an int
secs += (s1 - s2);
```

Converting a floating-point value that is outside the range of the target type is undefined behaviour in C (C17 6.3.1.4). On x86-64 and ARM64 the instruction happens to return `INT_MIN`, the result stays negative, and the caller ignores it, so the numbers come out right. At a clock time of 02:30, for example, the function returns -2,147,492,648 on LP64 platforms (64-bit `long`) and +2,147,474,648 on Windows, where adding `s1 - s2` wraps the 32-bit `long` a second time. Both fail the `timeStep > 0 && timeStep < maxStep` test, by luck rather than design.

The practical cost is in testing. Every deck whose rain ends before the run does trips UndefinedBehaviorSanitizer here, so a sanitizer build of SWMM stops on Example1.inp and on 68 of the review's 249 test decks (73 for 5.3.0) before reaching anything else. The review's own builds make this one check recoverable and suppress it (`tools/ubsan.supp`) in every other test.

6.0.0 keeps the algorithm but returns `static_cast<long>(...)`. On LP64 platforms -6.37e10 fits, so the regression decks are clean, but the same overflow happens on Windows, and on every platform through the public API: `swmm_datetime_time_diff(1e15, 0, &out)` asks for 8.64e19 seconds.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-01_rain-ends-early.inp`](CRASH-01_rain-ends-early.inp) | One subcatchment on a gage whose series ends at 0:45 of a 3-hour run |
| [`CRASH-01_test.c`](CRASH-01_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0) |
| [`CRASH-01_test6.c`](CRASH-01_test6.c) | Calls 6.0.0's `swmm_datetime_time_diff()` with `NO_DATE` and with differences beyond the range of `long` |
| `no-ubsan-suppressions` | Tells `tools/run-test.sh` not to suppress this report for this issue |

```sh
tools/run-test.sh CRASH-01            # 5.2.4, 5.3.0 and 6.0.0: CRASH
tools/run-test.sh CRASH-01 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 report the conversion at the first runoff step after 0:45 (the run itself completes):

```
datetime.c:432:12: runtime error: -6.37135e+10 is outside the range of representable values of type 'int'
    #0 in datetime_timeDiff datetime.c:432:12
    #1 in runoff_getTimeStep runoff.c:316:20
    #2 in runoff_execute runoff.c:217:18
Routing steps run: 179, error code: 0
CRASH-01 5.2.4 base: CRASH
CRASH-01 5.3.0 base: CRASH
```

6.0.0 handles the `NO_DATE` case on Linux but not the larger difference:

```
DateTime.hpp:198:35: runtime error: 8.64e+19 is outside the range of representable values of type 'long'
NO_DATE - 2020-01-01      = -63713520000 s
1e15 days - day 0         = -9223372036854775808 s (exact value 8.64e19 s)
-1e15 days - day 0        = -9223372036854775808 s
FAIL: an out-of-range time difference is not clamped
CRASH-01 6.0.0 base: CRASH
```

A difference of +8.64e19 s comes back as the most negative `long`, with the wrong sign.

**With the fix** there is no sanitizer report; the `NO_DATE` differences are returned exactly and the out-of-range ones as `LONG_MAX`/`LONG_MIN`:

```
NO_DATE - 2020-01-01      = -63713520000 s
1e15 days - day 0         = 9223372036854775807 s (exact value 8.64e19 s)
-1e15 days - day 0        = -9223372036854775808 s
CRASH-01 5.3.0 patched: PASS
CRASH-01 6.0.0 patched: PASS
```

Without a sanitizer, the legacy test prints `PASS` before and after the fix: the defect is the undefined conversion itself.

## The fix

Compute the difference in double, where both terms are whole numbers held exactly, clamp it to the range of `long`, and convert once:

```diff
-    secs = (int)(floor((d1 - d2)*SecsPerDay + 0.5));
-    secs += (s1 - s2);
-    return secs;
+    secs = floor((d1 - d2)*SecsPerDay + 0.5) + (double)(s1 - s2);
+
+    // --- a difference that does not fit in a long (e.g. one date is
+    //     NO_DATE) is clamped instead of overflowing the conversion
+    if ( secs >= (double)LONG_MAX ) return LONG_MAX;
+    if ( secs <= (double)LONG_MIN ) return LONG_MIN;
+    return (long)secs;
```

Every in-range result is bit-identical to before, so no model output changes. The 6.0.0 patch makes the same change in `datetime::timeDiff()`.
