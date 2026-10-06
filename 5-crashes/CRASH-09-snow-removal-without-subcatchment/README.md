# CRASH-09: A snow REMOVAL line with Fsub > 0 but no receiving subcatchment indexes Subcatch[-1]

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Heap out-of-bounds read as soon as the plowable snow is deeper than Dplow. Under AddressSanitizer the run stops in `snow_plowSnow()`; a release build reads the bytes before the `Subcatch` array as a pointer, and if they are not zero, adds snow to whatever address they hold, or crashes. No input error or warning is given. |
| **Reached from** | `[SNOWPACKS]` REMOVAL line with Fsub > 0 and the optional receiving subcatchment left out |
| **5.3.0** | `snow_plowSnow()` in [`src/legacy/engine/snow.c:478`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/snow.c#L478); `toSubcatch = -1` set by [`snow_readMeltParams()`, `:118`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/snow.c#L118) |
| **5.2.4** | Same code, [`src/solver/snow.c:434`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/snow.c#L434) |
| **6.0.0** | Not affected: `SnowSolver::plowSnow()` skips the transfer when `to_subcatch < 0` ([`src/engine/hydrology/Snow.cpp:541`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Snow.cpp#L541)) and the snow stays on the plowable area |
| **Since** | SWMM 5.0 (present in the 5.0.022 code of the repository's first commit), so every release |
| **Fix** | Transfer only when a subcatchment was named, as 6.0.0 does: [`CRASH-09_swmm530.patch`](CRASH-09_swmm530.patch) |

## The problem

A REMOVAL line in `[SNOWPACKS]` reads

    Name REMOVAL Dplow Fout Fimp Fperv Fimelt (Fsub Scatch)

where Fsub is the fraction of plowed snow sent to the pervious area of subcatchment Scatch. The parser accepts Fsub without Scatch, and nothing checks the pair afterwards: `snow_validateSnowmelt()` only checks that the fractions add up to no more than 1.01. A line such as

    SP1 REMOVAL 0.1 0 0 0 0 1.0

passes input checking. The first time the plowable snow on a subcatchment using SP1 is deeper than Dplow, `snow_plowSnow()` looks up the receiving subcatchment at index -1.

In the test, S1 (10 ac, all plowable impervious) starts with 1 in of snow and Dplow is 0.1 in, so this happens at the first runoff step. 5.2.4 and 5.3.0 stop with a heap-buffer-overflow. 6.0.0 runs: the snow that cannot be sent anywhere stays on S1 and melts there.

## Why it happens

```c
// src/legacy/engine/snow.c, snow_readMeltParams()
    // --- parse name of subcatch receiving snow plowed from current subcatch
    if ( k == SNOW_REMOVAL )
    {
        x[6] = -1.0;                          // no name given
        if ( ntoks >= 9 )
        {
            m = project_findObject(SUBCATCH, tok[8]);
            ...
            x[6] = m;
        }
    }

// src/legacy/engine/snow.c, setMeltParams()
        else               Snowmelt[j].toSubcatch = -1;

// src/legacy/engine/snow.c, snow_plowSnow()
            // --- send to another subcatchment
            if ( Snowmelt[k].sfrac[4] > 0.0 )
            {
                m = Snowmelt[k].toSubcatch;            // -1
                if ( Subcatch[m].snowpack )            // reads Subcatch[-1]
                {
                    f = Subcatch[m].snowpack->fArea[SNOW_PERV];
                ...
                    Subcatch[m].snowpack->wsnow[SNOW_PERV] +=
                        Snowmelt[k].sfrac[4] * exc * f;  // writes through it
```

When the receiving subcatchment exists but has no pervious snow area, the code already skips the transfer and leaves that share of the snow on the plowable area (`sfracTotal` does not include it). 6.0.0 applies the same rule to a missing subcatchment:

```cpp
// src/engine/hydrology/Snow.cpp, SnowSolver::plowSnow()
        if (soa_.sfrac[sf + 4] > 0.0 && soa_.to_subcatch[uj] >= 0) {
```

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-09_fsub-no-subcatch.inp`](CRASH-09_fsub-no-subcatch.inp) | S1 (10 ac, all plowable) with 1 in of snow; REMOVAL Dplow 0.1 in, Fsub 1.0, no subcatchment named; 60 °F for 6 h; no rain or losses |
| [`CRASH-09_test.c`](CRASH-09_test.c) | Runs the deck through the legacy toolkit. Passes if the input is rejected with an error code, or if the run completes with a runoff continuity error within 1% |
| [`CRASH-09_test6.c`](CRASH-09_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh CRASH-09            # 5.2.4 and 5.3.0: CRASH; 6.0.0: PASS
tools/run-test.sh CRASH-09 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0 (5.2.4 is the same at `snow.c:435:34`):

```
==13289==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x5150000004d8 at pc 0x7efcc859b696 bp 0x7ffe8db28270 sp 0x7ffe8db28268
READ of size 8 at 0x5150000004d8 thread T0
    #0 0x7efcc859b695 in snow_plowSnow .../src/legacy/engine/snow.c:479:34
    #1 0x7efcc8595deb in runoff_execute .../src/legacy/engine/runoff.c:254:32
    #2 0x7efcc85c112b in execRouting .../src/legacy/engine/swmm5.c:986:17
    #3 0x7efcc85c112b in swmm_step .../src/legacy/engine/swmm5.c:818:13
CRASH-09 5.2.4 base: CRASH
CRASH-09 5.3.0 base: CRASH
```

6.0.0 completes. The -0.591% continuity error is the engine's normal result for this melt: the same deck without the REMOVAL line gives the same figure.

```
Runoff steps run: 360
Initial snow cover   1.000 in
Surface runoff       1.002 in
Final snow cover     0.000 in
Runoff continuity error -0.591 %
PASS: the run completed, the snow with no receiving subcatchment stayed on S1 (runoff continuity error -0.591%)
CRASH-09 6.0.0 base: PASS
```

**With the fix**, 5.3.0 gives the same results as 6.0.0 (it counts one step fewer because the legacy loop stops on the step that returns elapsed time 0):

```
Runoff steps run: 359
Initial snow cover   1.000 in
Surface runoff       1.002 in
Final snow cover     0.000 in
Runoff continuity error -0.591 %
PASS: the run completed, the snow with no receiving subcatchment stayed on S1 (runoff continuity error -0.591%)
CRASH-09 5.3.0 patched: PASS
CRASH-09 6.0.0 patched: PASS
```

## The fix

```diff
             // --- send to another subcatchment
-            if ( Snowmelt[k].sfrac[4] > 0.0 )
+            if ( Snowmelt[k].sfrac[4] > 0.0 && Snowmelt[k].toSubcatch >= 0 )
```

The Fsub share then stays on the plowable area, which is what 5.3.0 already does for a receiving subcatchment without pervious snow area and what 6.0.0 does for a missing one, so the two engines agree. Rejecting the line (ERROR 182 in `snow_validateSnowmelt()`) would also prevent the crash, but 5.3.0 would then refuse input that 6.0.0 accepts; 6.0.0's object deleter also clears the receiving subcatchment when that subcatchment is deleted, so "Fsub without a subcatchment" is a state it deliberately supports. No input that ran before changes: the new condition only matters when `toSubcatch` is -1, which crashed or read memory outside the array.
