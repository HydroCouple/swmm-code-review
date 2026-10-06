# IO-44: A hot start file forgets how much Modified Horton has infiltrated toward Fmax

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A run restarted from a hot start file infiltrates up to the maximum infiltration volume Fmax again. In the test (Fmax 1 in, two 0.8 in storms a day apart) day 2 of the restarted run infiltrates 0.800 in and produces no runoff; day 2 of the continuous run infiltrates 0.203 in and produces 0.594 in of runoff. The two days together infiltrate 1.6 in on a soil that holds 1 in. Nothing warns |
| **Reached from** | `INFILTRATION MODIFIED_HORTON` with a maximum infiltration volume (5th `[INFILTRATION]` value > 0) and `USE HOTSTART` |
| **5.3.0** | `horton_getState()` / `horton_setState()` in [`src/legacy/engine/infil.c:375`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/infil.c#L375), which leave out `Fmh`, the accumulator `modHorton_getInfil()` caps at Fmax ([`infil.c:551`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/infil.c#L551)) |
| **5.2.4** | Not affected: 5.2.4 has no `Fmh`; it tests Fmax against `Fe`, which the hot start file carries ([`src/solver/infil.c:370`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/infil.c#L370)). Its Fmax test is wrong in another way, already fixed in 5.3.0: `Fe = MAX(Fe, Fmax)` ([`infil.c:545`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/infil.c#L545)) sets `Fe` to Fmax at the first wet step, so infiltration stops after 0.067 in |
| **6.0.0** | Not affected: the native hot start format carries `Fmh` ([`src/engine/hydrology/Runoff.cpp:794`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L794) and [`:835`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Runoff.cpp#L835)). A legacy `.hsf` file restores no subcatchment state at all in 6.0.0 ([`src/engine/core/HotStartManager.cpp:1368`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/HotStartManager.cpp#L1368), with a warning), and a file written by 6.0.0's own `[FILES] SAVE HOTSTART` cannot be reused for a model with subcatchments, broader gaps outside this issue (see [further-findings.md](../../further-findings.md)) |
| **Since** | 5.3.0, fork commit 353f8fe2 "Addressing bug for mod. horton #59" (December 2023), which added `Fmh` without adding it to the saved state |
| **Fix** | Save and restore `Fmh` in the third, so far unused, infiltration slot: [`IO-44_swmm530.patch`](IO-44_swmm530.patch) |

## The problem

Modified Horton's fifth parameter, Fmax, is the maximum infiltration volume of the soil. Once the soil has taken Fmax, infiltration stops, apart from a slow recovery in dry weather set by the drying time. 5.3.0 tracks the volume taken in a new state variable, `Fmh`.

A hot start file is meant to let a long simulation be split into pieces: run to time T, save the state, and later continue from T as if the run had never stopped. For a Modified Horton soil with Fmax that does not work in 5.3.0. The file does not hold `Fmh`, so the continuation starts with an empty soil store.

The test runs one pervious acre (f0 3 in/hr, fmin 0.5 in/hr, decay 4/hr, drying time 1000 days, Fmax 1 in) through two 0.8 in storms, one at the start of each day:

| | Infiltration (in) | Runoff (in) |
|---|---|---|
| continuous 2-day run | 1.003 | 0.594 |
| day-1 run, saves the hot start file | 0.800 | 0.000 |
| day 2 of the continuous run (difference) | 0.203 | 0.594 |
| **day-2 run from the hot start file** | **0.800** | **0.000** |

In the continuous run the second storm meets a soil that already holds 0.8 in, so it infiltrates 0.2 in and runs off 0.6 in. Restarted from the hot start file, the same storm infiltrates completely. (The continuous total exceeds 1 in by 0.003 in because the 1000-day drying time restores a little capacity between the storms.)

## Why it happens

Commit 353f8fe2 made `modHorton_getInfil()` cap infiltration with `Fmh`:

```c
// src/legacy/engine/infil.c, modHorton_getInfil()
        // --- limit cumulative infiltration to Fmax
        if (infil->Fmax > 0.0)
        {
            if (infil->Fmh + f * tstep > infil->Fmax)
                f = (infil->Fmax - infil->Fmh) / tstep;

            f = MAX(f, 0.0);
            
            infil->Fmh += f * tstep;
        }
```

`horton_initState()` sets `Fmh` to 0, but the two functions `saveRunoff()` and `readRunoff()` use for the infiltration state were not changed:

```c
// src/legacy/engine/infil.c
void horton_getState(THorton *infil, double x[])
{
    x[0] = infil->tp;
    x[1] = infil->Fe;
}

void horton_setState(THorton *infil, double x[])
{
    infil->tp = x[0];
    infil->Fe = x[1];
}
```

`saveRunoff()` writes six doubles per subcatchment for the infiltration state ([`hotstart.c:512`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/hotstart.c#L512)); for Horton the last four are always zero. After `USE HOTSTART`, `Fmh` keeps the 0 from `horton_initState()`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-44_continuous.inp`](IO-44_continuous.inp) | One pervious acre, MODIFIED_HORTON with Fmax 1 in, 0.8 in storms at 00:00 on 1 and 2 June; runs both days |
| [`IO-44_day1.inp`](IO-44_day1.inp) | Day 1 only; `SAVE HOTSTART IO-44_day1.hsf` |
| [`IO-44_day2.inp`](IO-44_day2.inp) | Day 2 only; `USE HOTSTART IO-44_day1.hsf` |
| [`IO-44_day2_v6.inp`](IO-44_day2_v6.inp) | Day 2 without `[FILES]`, for 6.0.0's native hot start API |
| [`IO-44_test.c`](IO-44_test.c) | Runs the three decks and compares day 2 of the restarted run with day 2 of the continuous run, using the runoff continuity table of each report (5.2.4 and 5.3.0) |
| [`IO-44_test6.c`](IO-44_test6.c) | The same with 6.0.0, saving after day 1 with `swmm_hotstart_save()` and applying with `swmm_hotstart_apply()` |

```sh
tools/run-test.sh IO-44            # 5.2.4: PASS, 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh IO-44 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix**:

```
---- IO-44 on 5.3.0 (base) ----
                                         infiltration       runoff
                                                 (in)         (in)
continuous run, both days                       1.003        0.594
day-1 run (saves the hot start file)            0.800        0.000
day 2 of the continuous run                     0.203        0.594
day-2 run from the hot start file               0.800        0.000
day-1 run + day-2 run                           1.600
FAIL: the restarted day 2 infiltrates 0.800 in and runs off 0.000 in instead of 0.203 in and 0.594 in; the two days infiltrate 1.600 in with Fmax = 1.0 in
IO-44 5.3.0 base: FAIL
```

6.0.0 restores the state from its own hot start format:

```
day 2 of the continuous run                     0.203        0.594
day-2 run from the hot start file               0.203        0.594
day-1 run + day-2 run                           1.003
PASS: the run restarted from the hot start file reproduces day 2 of the continuous run, within Fmax
IO-44 6.0.0 base: PASS
```

5.2.4 passes the test because its restart is consistent, but its numbers show its own Fmax defect (infiltration stops after 0.067 in):

```
continuous run, both days                       0.067        1.531
day-1 run (saves the hot start file)            0.067        0.730
day 2 of the continuous run                     0.000        0.801
day-2 run from the hot start file               0.000        0.801
IO-44 5.2.4 base: PASS
```

**With the fix**, 5.3.0 gives the same numbers as 6.0.0:

```
day 2 of the continuous run                     0.203        0.594
day-2 run from the hot start file               0.203        0.594
day-1 run + day-2 run                           1.003
PASS: the run restarted from the hot start file reproduces day 2 of the continuous run, within Fmax
IO-44 5.3.0 patched: PASS
```

The 6.0.0 test names the outfall `OF1` so that its native hot start file is 196 bytes long. For lengths that are not a multiple of 4, `swmm_hotstart_open()` reads the file's CRC through a misaligned `uint32_t` pointer ([`HotStartManager.cpp:336`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/HotStartManager.cpp#L336)), which stops the sanitizer build; that is a separate defect.

## The fix

Use the third of the six infiltration slots, which the file already has and which has always been written as 0 for Horton:

```diff
 void horton_getState(THorton *infil, double x[])
 {
     x[0] = infil->tp;
     x[1] = infil->Fe;
+    x[2] = infil->Fmh;
 }
 
 void horton_setState(THorton *infil, double x[])
 {
     infil->tp = x[0];
     infil->Fe = x[1];
+    infil->Fmh = x[2];
 }
```

The file layout does not change. A file written by 5.2.4 or by an unpatched 5.3.0 restores `Fmh = 0`, as now. Plain Horton never changes `Fmh`, so its files are unchanged. Only runs that use a hot start file with Modified Horton and Fmax > 0 give different results; no deck in the regression suite does.
