# IO-47: The groundwater table read from a runoff interface file is mirrored below the aquifer bottom

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A `USE RUNOFF` run reports each subcatchment's groundwater table at (aquifer bottom − depth) instead of (subcatchment bottom + depth). In the test the water table falls from 7.99 ft to 3.35 ft in the run that wrote the file and from −6.99 ft to −2.35 ft in the run that reads it, below an aquifer whose bottom is at 1 ft. The groundwater storage at the end of the run is wrong with it (20.292 instead of 31.708 ac-ft). Groundwater flow to the drainage system is not affected (it is read separately, see IO-46). 6.0.0 writes 0 for the water table and soil moisture and ignores them on reading, so its `USE RUNOFF` run reports the initial values throughout |
| **Reached from** | `[FILES] USE RUNOFF` for a model with `[GROUNDWATER]` |
| **5.3.0** | `runoff_readFromFile()` in [`src/legacy/engine/runoff.c:469`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L469); written in `subcatch_getResults()` at [`subcatch.c:914`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L914) |
| **5.2.4** | Same code, [`src/solver/runoff.c:462`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/runoff.c#L462) and [`subcatch.c:881`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L881); same numbers |
| **6.0.0** | Reproduces differently: `RunoffInterfaceFile::saveResults()` writes 0 for both fields ([`src/engine/hydrology/RunoffInterface.cpp:173`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/RunoffInterface.cpp#L173)) and `readResults()` skips them, a documented omission; the reported water table stays at 8.000 ft and the soil moisture at 0.4000 |
| **Since** | Every release in the repository (5.1.000 has the same read line) |
| **Fix** | Subtract the subcatchment's bottom elevation from the saved elevation: [`IO-47_swmm530.patch`](IO-47_swmm530.patch); write and restore both fields from the groundwater state: [`IO-47_swmm600.patch`](IO-47_swmm600.patch). Both need IO-46's patch first (same lines in 5.3.0; the record-timing fix in 6.0.0), and the 6.0.0 patch also IO-45's (same comment) |

## The problem

A `USE RUNOFF` run reads each subcatchment's results from the runoff interface file instead of computing them, including the groundwater table elevation and upper-zone moisture, which it reports in the binary output and uses for the groundwater continuity table.

The test deck is a two-day groundwater recession with no rain. The aquifer's bottom is at 0 ft; subcatchment S1 overrides it to 1 ft (the optional `Ebot` column of `[GROUNDWATER]`). The water table starts at 8 ft, and the upper zone starts above field capacity so that its moisture changes too:

| Hour | Water table, run that writes the file | Water table, run that reads it |
|---|---|---|
| 1 | 7.993 ft | −6.993 ft |
| 24 | 4.586 ft | −3.586 ft |
| 48 | 3.354 ft | −2.354 ft |

The reading run puts the water table 3 to 8 ft below the bottom of the aquifer. The groundwater continuity table, which computes the final storage from the same state, ends with 20.292 ac-ft instead of 31.708 ac-ft. The soil moisture is restored correctly.

6.0.0 does not carry either field: the reading run reports the initial 8.000 ft and 0.4000 for all 48 hours, while the writing run goes from 7.993 ft and 0.3699 down to 3.354 ft and 0.3000. A file written by 6.0.0 also gives a 5.3.0 run that reads it an elevation of 0 ft for every subcatchment.

## Why it happens

`subcatch_getResults()` saves the water table elevation, measured from the subcatchment's own aquifer bottom:

```c
// src/legacy/engine/subcatch.c, subcatch_getResults()
        z = (gw->bottomElev + gw->lowerDepth) * UCF(LENGTH);
        x[SUBCATCH_GW_ELEV] = (float)z;
```

`runoff_readFromFile()` turns it back into a depth the wrong way round, and from the aquifer's bottom instead of the subcatchment's:

```c
// src/legacy/engine/runoff.c, runoff_readFromFile()
            gw->lowerDepth = Aquifer[gw->aquifer].bottomElev -
                             (SubcatchResults[SUBCATCH_GW_ELEV] / UCF(LENGTH));
```

With S1's bottom at 1 ft and the aquifer's at 0 ft, a saved elevation of 7.993 ft gives `lowerDepth` = 0 − 7.993 = −7.993 ft, and the reported elevation becomes 1 + (−7.993) = −6.993 ft. The hot start code does the same conversion correctly (`gwater_setState()`: `gw->lowerDepth = x[1] - gw->bottomElev`).

6.0.0's runoff interface writer has no access to the groundwater solver's state, so it writes 0 for both fields and the reader skips them (the header notes "Fields with no refactored per-subcatchment state are written as 0 and skipped on read: snow_depth, gw_elev, soil_moist"). The reported values then come from the groundwater state, which nothing updates in a `USE RUNOFF` run.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-47_save.inp`](IO-47_save.inp) | 10-acre subcatchment, no rain, water table at 8 ft, aquifer bottom 0 ft overridden to 1 ft for S1, upper-zone moisture 0.40 (field capacity 0.30), 2 days; `SAVE RUNOFF IO-47.rof` |
| [`IO-47_use.inp`](IO-47_use.inp) | The same model with `USE RUNOFF IO-47.rof` |
| [`IO-47_test.c`](IO-47_test.c) | Runs both; compares S1's groundwater elevation and soil moisture series in the two `.out` files (5.2.4 and 5.3.0) |
| [`IO-47_test6.c`](IO-47_test6.c) | The same with 6.0.0 and its output reader |

```sh
tools/run-test.sh IO-47            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-47 --patched  # 5.3.0 and 6.0.0 with IO-45, IO-46 and IO-47: PASS
```

**Without the fix**, 5.2.4 and 5.3.0:

```
hour   S1 groundwater elevation (ft)   S1 soil moisture
         SAVE run      USE run          SAVE run   USE run
   1        7.993       -6.993           0.3699    0.3699
  12        5.971       -4.971           0.3025    0.3025
  24        4.586       -3.586           0.3000    0.3000
  36        3.798       -2.798           0.3000    0.3000
  48        3.354       -2.354           0.3000    0.3000
USE run water table between -6.993 and -2.354 ft (aquifer bottom 1 ft, surface 10 ft)
FAIL: the USE RUNOFF run's groundwater state differs from the run that wrote the file by up to 14.987 ft (elevation) and 0.0000 (moisture)
IO-47 5.3.0 base: FAIL
```

6.0.0:

```
   1        7.993        8.000           0.3699    0.4000
  12        5.971        8.000           0.3025    0.4000
  24        4.586        8.000           0.3000    0.4000
  36        3.798        8.000           0.3000    0.4000
  48        3.354        8.000           0.3000    0.4000
USE run water table between 8.000 and 8.000 ft (aquifer bottom 1 ft, surface 10 ft)
FAIL: the USE RUNOFF run's groundwater state differs from the run that wrote the file by up to 4.646 ft (elevation) and 0.1000 (moisture)
IO-47 6.0.0 base: FAIL
```

**With the fix**, both engines print the same table, and the runoff interface files they write are byte-identical:

```
   1        7.993        7.993           0.3699    0.3699
  12        5.971        5.971           0.3025    0.3025
  24        4.586        4.586           0.3000    0.3000
  36        3.798        3.798           0.3000    0.3000
  48        3.354        3.354           0.3000    0.3000
USE run water table between 3.354 and 7.993 ft (aquifer bottom 1 ft, surface 10 ft)
PASS: the USE RUNOFF run reports the groundwater state of the run that wrote the file
IO-47 5.3.0 patched: PASS
IO-47 6.0.0 patched: PASS
```

## The fix

5.3.0, the inverse of what `subcatch_getResults()` writes:

```diff
-            gw->lowerDepth = Aquifer[gw->aquifer].bottomElev -
-                             (SubcatchResults[SUBCATCH_GW_ELEV] / UCF(LENGTH));
+            gw->lowerDepth = SubcatchResults[SUBCATCH_GW_ELEV] / UCF(LENGTH) -
+                             gw->bottomElev;
```

6.0.0: `saveResults()` and `readResults()` take an optional pointer to the groundwater state, which `SWMMEngine` passes. For a subcatchment with groundwater, the writer stores `(bottom_elev + lower_depth) * UCF(LENGTH)` and `theta`, as legacy does, and the reader restores `lower_depth` and `theta` from them. The 6.0.0 patch is written on top of IO-46's, which moves the write after the groundwater step (otherwise the record holds the previous step's water table), and of IO-45's, which edits the same comment.

Only `USE RUNOFF` runs of models with groundwater report different values, and in 6.0.0 the content of `SAVE RUNOFF` files for such models. The groundwater continuity table of a `USE RUNOFF` run still shows a large error after the fix (26.3 % here) because that run computes no groundwater fluxes; the final storage in it is now the right one (31.708 ac-ft).
