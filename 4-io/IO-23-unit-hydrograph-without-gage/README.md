# IO-23: A unit hydrograph group without a rain gage line is accepted and silently uses gage 0

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | No message. In 5.2.4 and 5.3.0 the group takes its rain from the first gage in the project; if nothing else uses that gage, its first rainfall value is held for the whole run (46.5 times the right RDII in the test). With no gages in the project both read past the end of the gage array. 6.0.0 gives the group no rain and no RDII. |
| **Reached from** | A [HYDROGRAPHS] group that has UH lines but no `Name Gage` line, used by an [RDII] node |
| **5.3.0** | `rdii_initUnitHyd()` in [`src/legacy/engine/rdii.c:201`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rdii.c#L201) leaves the gage at 0 from `calloc` ([`project.c:1062`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L1062)); overflow in `initGageData()` at [`rdii.c:1099`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rdii.c#L1099) |
| **5.2.4** | Same code; overflow at [`src/solver/rdii.c:1062`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/rdii.c#L1062) |
| **6.0.0** | Reproduces silently in a different form: a group without a gage line gets gage index -1 ([`src/engine/hydrology/RDII.cpp:317`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/RDII.cpp#L317)) and a rainfall of 0 ([`RDII.cpp:541`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/RDII.cpp#L541)) |
| **Since** | Every release in the repository history (initial commit, 2014) |
| **Fix** | New ERROR 152 for a group without a rain gage: [`IO-23_swmm530.patch`](IO-23_swmm530.patch), [`IO-23_swmm600.patch`](IO-23_swmm600.patch) |

## The problem

The input reference says: "For each group of unit hydrographs, use one line to specify its rain gage followed by as many lines as are needed to define each unit hydrograph used by the group". Nothing checks that the gage line is there. A file edited by hand, or written by a tool that dropped the line, runs without a message:

- **5.2.4 and 5.3.0** use the project's first rain gage. In the test that gage, G1, is used by nothing else. Its first value (1.0 in over the first hour, 1.0 in/hr) is loaded at start-up and never updated, so RDII sees 1.0 in/hr for all 48 hours: 48 in of rain instead of 1, and 1,688,919 ft3 of RDII at node J1, 46.5 times the 36,300 ft3 that R x P x A gives on G1. If the first gage is in use, the group silently uses its rainfall, right or wrong.
- **With no rain gages in the project**, 5.2.4 and 5.3.0 read `Gage[0]` from a zero-length array: AddressSanitizer reports a heap-buffer-overflow in `initGageData()`; a normal build reads whatever memory follows.
- **6.0.0** gives the group no rainfall, so the node gets no RDII, also without a message.

## Why it happens

The group's gage is set only when its two-token line is read, and `rdii_initUnitHyd()` does not give it a "none" value, so it stays at the 0 that `calloc` wrote:

```c
// src/legacy/engine/rdii.c, rdii_readUnitHydParams()
    // --- line has 2 tokens; assign rain gage to UH object
    if ( ntoks == 2 )
    {
        g = project_findObject(GAGE, tok[1]);
        if ( g < 0 ) return error_setInpError(ERR_NAME, tok[1]);
        UnitHyd[j].rainGage = g;
        Gage[g].isUsed = TRUE;
        return 0;
    }
```

`validateRdii()` checks the hydrograph times and ratios but not the gage. `initGageData()` then dereferences it (`g >= 0` is always true), which is the overflow when there are no gages:

```c
// src/legacy/engine/rdii.c, initGageData()
        g = UnitHyd[i].rainGage;
        if ( g >= 0 )
        {
            if ( Gage[g].coGage >= 0 )          // Gage[0] of an empty array
```

Gage 0 is not marked as used, because only a gage line sets `isUsed`. `gage_initState()` still loads its first rainfall record, but every later `gage_setState()` returns at once:

```c
// src/legacy/engine/gage.c, gage_setState()
    // --- return if gage not used by any subcatchment
    if ( Gage[j].isUsed == FALSE ) return;
```

so the RDII pass reads the first record's intensity at every step.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-23_no-gage-line.inp`](IO-23_no-gage-line.inp) | UH1 (R = 0.1, T = 1 h, K = 2) on 100 ac at node J1, without the `UH1 G1` line; gage G1 (1.0 in in the first hour) exists but nothing uses it; 48-hour run |
| [`IO-23_no-gages.inp`](IO-23_no-gages.inp) | The same without any rain gage |
| [`IO-23_test.c`](IO-23_test.c) | Runs both decks through the legacy toolkit. Each must stop with an input error; for a run that is accepted it prints the RDII volume at J1 |
| [`IO-23_test6.c`](IO-23_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh IO-23            # 5.2.4, 5.3.0: CRASH; 6.0.0: FAIL
tools/run-test.sh IO-23 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.3.0 (5.2.4 is the same, at `rdii.c:1062`):

```
deck                     error  RDII volume (ft3)
IO-23_no-gage-line.inp       0     1688919.4  (46.5 x R*P*A for gage G1)
=================================================================
==22370==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x502000000dbc at pc 0x7f860157bc03 bp 0x7ffdcc27d4b0 sp 0x7ffdcc27d4a8
READ of size 4 at 0x502000000dbc thread T0
    #0 0x7f860157bc02 in initGageData .../src/legacy/engine/rdii.c:1099:26
    #1 0x7f860157bc02 in createRdiiFile .../src/legacy/engine/rdii.c:787:5
    #2 0x7f86015764f2 in rdii_openRdii .../src/legacy/engine/rdii.c:470:35
IO-23 5.3.0 base: CRASH
```

6.0.0:

```
deck                     error  RDII volume (ft3)
IO-23_no-gage-line.inp       0           0.0  (0.0 x R*P*A for gage G1)
IO-23_no-gages.inp           0           0.0  (0.0 x R*P*A for gage G1)
FAIL: 2 of 2 decks with a gageless unit hydrograph group ran without an error
IO-23 6.0.0 base: FAIL
```

**With the fix** both engines stop with `ERROR 152: no rain gage assigned to Unit Hydrograph set UH1.` (6.0.0's API returns its parse-error code 5):

```
deck                     error  RDII volume (ft3)
IO-23_no-gage-line.inp     152  (rejected)
IO-23_no-gages.inp         152  (rejected)
PASS: a unit hydrograph group without a rain gage is an input error
IO-23 5.3.0 patched: PASS
IO-23 6.0.0 patched: PASS
```

## The fix

`rdii_initUnitHyd()` sets the group's gage to -1, and `validateRdii()` reports a new error for a group that still has none. `createRdiiFile()` returns when validation fails, and `initGageData()` already skips a negative gage index, so nothing reads the gage array:

```diff
     int i;                             // individual UH index
     int m;                             // month index
 
+    UnitHyd[unitHydIndex].rainGage = -1;
     for ( m=0; m<12; m++)
 ...
     for (j=0; j<Nobjects[UNITHYD]; j++)
     {
+        // --- UH group must have a rain gage
+        if ( UnitHyd[j].rainGage < 0 )
+        {
+            report_writeErrorMsg(ERR_UNITHYD_GAGE, UnitHyd[j].ID);
+        }
+
```

```
ERR(152,"\n  ERROR 152: no rain gage assigned to Unit Hydrograph set %s.")
```

152 is unused in both engines and sits with the other unit hydrograph errors (151, 153). As in legacy, unit hydrographs are validated only when some node has [RDII] inflow, so a group that no node uses is not reported. The 6.0.0 patch adds the same code and message and checks, in its [HYDROGRAPHS] validation, that each group has a gage line naming an existing gage.

**Effect on other models.** No regression-suite deck uses RDII, and every group in the Richmond CSO model in 6.0.0's unit-test data has its gage line. Only input that is invalid by the input reference is affected.
