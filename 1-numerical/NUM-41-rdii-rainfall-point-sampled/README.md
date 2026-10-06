# NUM-41: RDII uses a point sample of the rain gage instead of the rain that fell

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | RDII volume off by -33% to +20% for a single 15-minute burst, depending only on WET_STEP, and 58% low when a UH limb is shorter than WET_STEP. Subcatchments on the same gage get the right rain. No warning; the RDII continuity table is consistent with the wrong rainfall. |
| **Reached from** | Any [HYDROGRAPHS]/[RDII] model where WET_STEP does not divide the gage's recording interval (15-min gage with a 2, 4, 6-14 min step; 1-hour gage with a 7, 8, 9, 11, 13, 14 or 25 min step ...), or where a UH's time to peak or falling limb (T, K x T) is shorter than WET_STEP |
| **5.3.0** | `getRainfall()` in [`src/legacy/engine/rdii.c:1234`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rdii.c#L1234) |
| **5.2.4** | Same code, [`src/solver/rdii.c:1197`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/rdii.c#L1197) |
| **6.0.0** | Reproduces: `RDIISolver::advance()` and `emitTick()` in [`src/engine/hydrology/RDII.cpp:544`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/RDII.cpp#L544) copy the legacy sampling on purpose, and give the same numbers |
| **Since** | Every release in the repository history (initial commit, 2014) |
| **Fix** | Average the gage over each RDII time step and split that rain over the step's UH intervals: [`NUM-41_swmm530.patch`](NUM-41_swmm530.patch), [`NUM-41_swmm600.patch`](NUM-41_swmm600.patch) |

## The problem

RDII is computed before the run (in 6.0.0, alongside it) on a grid of RDII time steps equal to WET_STEP. In each step, the rainfall that a unit hydrograph group receives is the gage's intensity at one instant times the length of the group's rain interval. That is only the rain that fell if the intensity is constant over the interval. Rain gage records are not aligned with that grid in general, so RDII gets too much or too little rain.

A single 1.0 in rain record between 0:15 and 0:30 on a 15-minute gage, an RTK unit hydrograph with R = 0.1, and 100 ac of sewershed must give R x P x A = 10 ac-in = 36,300 ft3 of RDII, whatever the time step. With WET_STEP = 10 min SWMM samples the gage at 0:00, 0:10, 0:20 and 0:30. Only the 0:20 sample sees the rain (4 in/hr), and it counts it for 10 minutes: 0.667 in of rain, 24,201 ft3 of RDII, a third less than the gage recorded. With WET_STEP = 9 min the samples at 0:18 and 0:27 both see rain and RDII gets 1.2 in. Over a long storm of steady intensity the errors largely cancel; short, intense bursts, which drive peak inflow and infiltration, are the ones mis-measured.

The subcatchments on the same gage get the full 1.0 in, because the runoff time step is cut at every change of the gage's rainfall.

A second path makes it worse. When a UH limb is shorter than WET_STEP, the RDII rain interval is that limb (6 min in the test), several intervals fall in one RDII step, and all of them reuse the one sample taken at the start of the step. In the test's short-limb case RDII gets 0.4 in of the 1.0 in.

SWMM reduces WET_STEP to the gage interval when it is longer (WARNING 01), so a WET_STEP larger than the gage interval cannot happen. The defect needs a step that is shorter than the gage interval and does not divide it, or a UH limb shorter than the step.

## Why it happens

```c
// src/legacy/engine/rdii.c, getRainInterval(): a UH group's rain interval
    ri = WetStep;
    ...  ri = MIN(ri, tLimb);           // shorter if a UH limb is shorter

// src/legacy/engine/rdii.c, getRainfall(), called once per RDII step (WetStep)
    for (g = 0; g < Nobjects[GAGE]; g++) Gage[g].isCurrent = FALSE;
    for (j = 0; j < Nobjects[UNITHYD]; j++)
    {
        g = UnitHyd[j].rainGage;
        rainInterval = UHGroup[j].rainInterval;
        while ( UHGroup[j].gageDate < currentDate )
        {
            gageDate = UHGroup[j].gageDate;
            ...
            if (!Gage[g].isCurrent)
            {
                gage_setState(g, gageDate);      // intensity AT gageDate
                Gage[g].isCurrent = TRUE;        // ... reused for the rest of this step
            }
            rainDepth = Gage[g].rainfall * (double)rainInterval / 3600.0;
            ...
            UHGroup[j].gageDate = datetime_addSeconds(gageDate, rainInterval);
        }
    }
```

`gage_setState()` returns the intensity of the record that covers `gageDate`; nothing accounts for a record that starts or ends inside the interval. `isCurrent` makes the gage be read once per RDII step, because `gage_setState()` can only move forward in time and several UH groups may share a gage. The intervals after the first one in a step therefore repeat the first one's intensity.

6.0.0 runs the same algorithm during the simulation. `advance()` records the gage rate at the start of every rain interval, and `emitTick()` uses the first recorded rate of each step for all intervals of that step ("Legacy samples the gage ONCE per driver tick (Gage.isCurrent memo) — all chunks of one tick use the FIRST chunk's rate. Replicated below.").

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-41_burst.inp`](NUM-41_burst.inp) | 1.0 in in one 15-minute record (0:15-0:30), UH R = 0.1, T = 1 h, K = 2, 100 ac at node J1, 6-hour run |
| [`NUM-41_short-limb.inp`](NUM-41_short-limb.inp) | The same with K = 0.1, so the UH falling limb (6 min) is shorter than WET_STEP (10 min) |
| [`NUM-41_test.c`](NUM-41_test.c) | Runs the burst deck with WET_STEP = 1 ... 15 min (rewriting the WET_STEP line) and the short-limb deck through the legacy toolkit, integrates J1's lateral inflow (RDII only) and compares it with R x P x A |
| [`NUM-41_test6.c`](NUM-41_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-41            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-41 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** all three engines print the same table. Only the steps that divide 15 min (1, 3, 5, 15) are right:

```
Expected RDII volume R x P x A = 0.1 x 1 in x 100 ac = 36300 ft3

deck        WET_STEP  RDII volume (ft3)  ratio
burst        1 min       36301.3         1.000
burst        2 min       33881.2         0.933  <-- wrong
burst        3 min       36301.3         1.000
burst        4 min       38721.4         1.067  <-- wrong
burst        5 min       36301.3         1.000
burst        6 min       29041.1         0.800  <-- wrong
burst        7 min       33884.4         0.933  <-- wrong
burst        8 min       38778.8         1.068  <-- wrong
burst        9 min       43616.0         1.202  <-- wrong
burst       10 min       24200.9         0.667  <-- wrong
burst       11 min       26693.7         0.735  <-- wrong
burst       12 min       29041.1         0.800  <-- wrong
burst       13 min       31394.2         0.865  <-- wrong
burst       14 min       33730.7         0.929  <-- wrong
burst       15 min       36301.3         1.000
short-limb  10 min       15400.6         0.424  <-- wrong

FAIL: 12 of 16 runs put the wrong rain volume into RDII (worst: 0.424 x R*P*A)
NUM-41 5.2.4 base: FAIL
NUM-41 5.3.0 base: FAIL
NUM-41 6.0.0 base: FAIL
```

**With the fix** both engines print identical numbers:

```
deck        WET_STEP  RDII volume (ft3)  ratio
burst        1 min       36301.3         1.000
burst        2 min       36301.3         1.000
burst        3 min       36301.3         1.000
burst        4 min       36301.3         1.000
burst        5 min       36301.3         1.000
burst        6 min       36301.3         1.000
burst        7 min       36304.7         1.000
burst        8 min       36355.1         1.002
burst        9 min       36346.7         1.001
burst       10 min       36301.3         1.000
burst       11 min       36400.5         1.003
burst       12 min       36301.3         1.000
burst       13 min       36224.0         0.998
burst       14 min       36140.0         0.996
burst       15 min       36301.3         1.000
short-limb  10 min       37328.0         1.028

PASS: RDII volume is R x P x A for every WET_STEP (worst: 1.028 x R*P*A)
NUM-41 5.3.0 patched: PASS
NUM-41 6.0.0 patched: PASS
```

The remaining deviations are the UH convolution's own discretisation. In the short-limb case RDII is computed once per 10-minute step and held for the step, which is longer than the 6-minute falling limb; that adds 2.8% on its own and is not part of this defect.

## The fix

A new `getGageRainfall()` walks the gage's state machine over one RDII step with `gage_setState()` and `gage_getNextRainDate()` (the same calls the runoff time step uses) and returns the time-weighted average intensity. It runs once per gage per step, where `gage_setState()` ran before, so the gage is still only moved forward. Each UH rain interval gets that average times the part of the step it covers, from the end of the previous interval to its own end but not past the current date, so the depths assigned in a step add up to the rain that fell in it:

```diff
             if (!Gage[g].isCurrent)
             {
-                gage_setState(g, gageDate);
+                GageRain[g] = getGageRainfall(g, prevDate, currentDate);
                 Gage[g].isCurrent = TRUE;
             }
-            rainDepth = Gage[g].rainfall * (double)rainInterval / 3600.0;
+
+            // --- rain falling between end of previous UH interval and
+            //     end of this one (but not beyond current date)
+            date2 = MIN(datetime_addSeconds(gageDate, rainInterval),
+                        currentDate);
+            rainDepth = GageRain[g] * (double)datetime_timeDiff(date2, date1)
+                        / 3600.0;
+            date1 = date2;
```

When a UH limb is shorter than WET_STEP, the rain within one step is spread evenly over the step's intervals; the legacy code already assumed one intensity per step. The rain interval, the number of intervals and the IA and dry-period bookkeeping are unchanged. Consecutive periods with the same intensity are merged before averaging, and where one intensity covers the whole step the function returns it unchanged, so a model whose gage records line up with WET_STEP gives bit-identical results.

The 6.0.0 patch makes `advance()` add each runoff substep's rain to the RDII step it falls in (a substep never crosses a gage record boundary) and queue the step's average rate with the same arithmetic, and `emitTick()` splits it as above. Both patched engines print the same numbers in the test.

**Effect on other models.** None of the regression-suite decks uses RDII. On the 2,764-line Richmond CSO model in the 6.0.0 unit-test data (`tests/unit/engine/data/legacy_small.inp`, 5-minute gage, WET_STEP 5 min) run for two days with a synthetic 3.79 in storm in place of its all-zero placeholder rain file, the binary output files of 5.3.0 and 6.0.0 are byte-identical before and after the fix. With the same storm given as 15-minute records and WET_STEP 10 min, the sewershed rainfall changes from 5384.264 to 5383.814 ac-ft (the value of the 5-minute version, i.e. the rain the gage recorded) and RDII from 18.836 to 18.834 ac-ft: a long, smooth storm averages out, a short burst does not.
