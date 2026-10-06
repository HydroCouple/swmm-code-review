# CON-23: A rain gage that only a control rule reads is never updated

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | A rule premise on a gage that no subcatchment uses (`GAGE G2 INTENSITY`, `GAGE G2 6` for a 6-hour total), such as a forecast or radar gage that drives real-time control, reads 0 in 5.2.4 and 5.3.0 and the first record's value in 6.0.0, for the whole run. The rule never acts on the rain and nothing warns the user. In models without subcatchments the gage is not updated even when flagged. |
| **Reached from** | `[CONTROLS]` premises or `VARIABLE`s on `GAGE <id> INTENSITY` or `GAGE <id> <hours>`, where no subcatchment (or unit hydrograph) uses the gage |
| **5.3.0** | `getRainValue()` in [`src/legacy/engine/controls.c:1966`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1966) returns 0 for an unused gage, and `gage_setState()` skips it, [`gage.c:346`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L346) |
| **5.2.4** | Same code, [`src/solver/controls.c:1397`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/controls.c#L1397) and [`gage.c:332`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gage.c#L332) |
| **6.0.0** | Reproduces: `gageIsUsed()` in [`src/engine/hydrology/Gage.cpp:182`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Gage.cpp#L182) does not count rules, and `updateAllGages()` leaves such a gage at its seeded first record ([`Gage.cpp:328`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Gage.cpp#L328)) |
| **Since** | 5.2.0, when gage premises were added |
| **Fix** | Flag gages named in rules as used and update them when no runoff is computed: [`CON-23_swmm530.patch`](CON-23_swmm530.patch), [`CON-23_swmm600.patch`](CON-23_swmm600.patch) Apply after NUM-19 (its `Requires:` line). |

## The problem

Since 5.2.0 a rule can test a rain gage: `IF GAGE G2 INTENSITY > 0.1` (current intensity) or `IF GAGE G2 6 > 1.0` (rain over the past 6 hours). A natural use is a gage that feeds only the controls, for example a forecast or radar series, or a gage in a model that has no subcatchments because inflows come from elsewhere.

Such a gage is never updated. In the test, gage G2 records 0.5 in/hr from 01:00 to 01:15 and rule GR throttles orifice OR1 to 0.2 while it rains:

```
RULE GR
IF GAGE G2 INTENSITY > 0.1
THEN ORIFICE OR1 SETTING = 0.2
ELSE ORIFICE OR1 SETTING = 1.0
```

OR1 stays at 1.0 for the whole run in 5.2.4, 5.3.0 and 6.0.0, both in a model with no subcatchments and in one whose only subcatchment uses another gage. Attach any subcatchment to G2 and the rule acts at 01:00 and 01:15.

## Why it happens

A gage carries an `isUsed` flag, set only by `subcatch_validate()` (a subcatchment's gage) and by the unit-hydrograph reader. An unused gage is never advanced through its record:

```c
// src/legacy/engine/gage.c, gage_setState()
    // --- return if gage not used by any subcatchment
    if ( Gage[j].isUsed == FALSE ) return;
```

and the rule code knows it, returning 0 instead of a stale value:

```c
// src/legacy/engine/controls.c, getRainValue()
    if (v.index < 0)
        return MISSING;
    else if (Gage[v.index].isUsed == FALSE)
        return 0.0;
```

The rule parser does not set the flag. Setting it in the parser alone would not be enough, for two reasons:

- `gage_readParams()` clears the flag when it reads the `[RAINGAGES]` line (`Gage[gageIndex].isUsed = FALSE;`), so a mark made by a `[CONTROLS]` (or `[HYDROGRAPHS]`) section that comes earlier in the file is lost. Both test decks put `[CONTROLS]` first.
- Gages are updated only by `runoff_execute()`, which `swmm_step()` calls only when the model has subcatchments (`DoRunoff`, `swmm5.c:744`). Without subcatchments no gage is ever updated.

6.0.0 computes the flag in `gageIsUsed()` from subcatchments, unit hydrographs and the 2D mesh. `updateAllGages()` runs at every runoff step, with or without subcatchments, but for a gage that is not used it keeps the rainfall of the gage's first record, so a rule reads that value (0 in the test) for the whole run.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-23_no-subcatch.inp`](CON-23_no-subcatch.inp) | Orifice OR1 under rule GR on gage G2 (0.5 in/hr from 01:00 to 01:15); no subcatchments; `[CONTROLS]` before `[RAINGAGES]` |
| [`CON-23_with-subcatch.inp`](CON-23_with-subcatch.inp) | The same plus subcatchment S1 on a dry gage G1, so runoff is computed |
| [`CON-23_test.c`](CON-23_test.c) | Steps both decks through the legacy toolkit; OR1 must be 1.0, 0.2, 1.0 at 0:30, 1:07 and 2:00, and at 0.2 for 15 min |
| [`CON-23_test6.c`](CON-23_test6.c) | The same through the 6.0.0 API |

```sh
tools/run-test.sh CON-23            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-23 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (all three engines print the same):

```
OR1 setting (expected)        0:30        1:07        2:00   minutes at 0.2 (expected 15)
  no subcatchments        1.00 (1.0)  1.00 (0.2)  1.00 (1.0)     0.0
  subcatchment on G1      1.00 (1.0)  1.00 (0.2)  1.00 (1.0)     0.0
FAIL: rule 'IF GAGE G2 INTENSITY > 0.1' did not act while G2 recorded 0.5 in/hr (OR1 at 0.2 for 0.0 and 0.0 min instead of 15)
CON-23 5.2.4 base: FAIL
CON-23 5.3.0 base: FAIL
CON-23 6.0.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
OR1 setting (expected)        0:30        1:07        2:00   minutes at 0.2 (expected 15)
  no subcatchments        1.00 (1.0)  0.20 (0.2)  1.00 (1.0)    15.0
  subcatchment on G1      1.00 (1.0)  0.20 (0.2)  1.00 (1.0)    15.0
PASS: the rule reads G2's rainfall and throttles OR1 from 01:00 to 01:15 in both decks
CON-23 5.3.0 patched: PASS
CON-23 6.0.0 patched: PASS
```

The test checks the intensity premise only. With the 5.3.0 patch the past-rain premise works as well (checked with a fixed 30 s step; with variable dynamic-wave steps the patch carries fractions of a second forward): the no-subcatchment deck with `IF GAGE G2 1 > 0.1` logs OR1 changes at 02:00 and 03:00, the same as 5.3.0 unpatched when a subcatchment uses G2. 6.0.0's past-rain premise does not fire in either case, patched or not, because of a separate defect in its past-rain totals (reported separately; not part of this fix).

## The fix

5.3.0, three parts:

```diff
 // controls.c, getPremiseVariable()
     case r_GAGE:
         ...
         object = r_GAGE;
+        Gage[index].isUsed = TRUE;   // so that the gage gets updated
         break;

 // gage.c, gage_readParams()
     Gage[gageIndex].coGage = -1;
-    Gage[gageIndex].isUsed = FALSE;
     return 0;

 // routing.c, routing_execute()
     currentDate = getDateTime(NewRoutingTime);
+    // --- with no runoff computed, update gages read by control rules here
+    //     (past rain takes whole seconds, so carry the fraction of a
+    //     dynamic wave step over to the next step)
+    if ( Nobjects[SUBCATCH] == 0 && Nobjects[GAGE] > 0 )
+    {
+        int pastRainStep;
+        PastRainTime += routingStep;
+        pastRainStep = (int)PastRainTime;
+        PastRainTime -= pastRainStep;
+        for (j = 0; j < Nobjects[GAGE]; j++)
+        {
+            gage_setState(j, currentDate);
+            gage_updatePastRain(j, pastRainStep);
+        }
+    }
     actionCount = evaluateControlRules(currentDate, routingStep);
```

`Gage[]` is allocated with `calloc`, so the flag starts out clear without the line removed from `gage_readParams()`. With no subcatchments nothing else calls `gage_setState()` during the run (the RDII interface file is built before `project_init()` re-initialises the gages), so the routing loop does what `runoff_execute()` does for gages: set the state at the start of the step and add the step's rain to the past-rain totals. `gage_updatePastRain()` counts whole seconds, while a dynamic-wave step can be a fraction of a second, so the patch carries the unused fraction (`PastRainTime`, reset in `routing_open()`) to the next step instead of truncating every step.

6.0.0 needs one change: `gageIsUsed()` also returns true for a gage that a rule names (`GAGE <id>` in the stored rule text). That makes `updateAllGages()`, which already runs without subcatchments, advance the gage, and keeps 6.0.0's runoff-step limits and gage validation in step with the patched 5.3.0, which now treats these gages as used too.

**Effect on other models.** 30 regression decks (`examples`, `update_v52`, `extran`) give byte-identical `.out` files with the patch in 5.3.0 and 6.0.0; none has a rule on a gage. The change to `gage_readParams()` also fixes a separate silent error in 5.2.4 and 5.3.0: when `[HYDROGRAPHS]` comes before `[RAINGAGES]`, the unit hydrograph's gage loses its flag and RDII inflow is 0. A small RDII deck (one junction, one unit hydrograph on a 2-hour, 0.5 in/hr storm) gives 0.000 acre-ft of RDII inflow in 5.2.4 and 5.3.0 in that section order and 0.417 acre-ft in the usual order; with the patch 5.3.0 gives 0.417 in both. 6.0.0 gives 0.417 in both orders with or without the patch.
