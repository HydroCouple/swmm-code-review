# IO-45: Evaporation read from a runoff interface file is 24 times too large

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A run that reads its runoff from a `USE RUNOFF` file reports every subcatchment's evaporation, and the system evaporation built from it, 24 times too large. In the test the subcatchment evaporates at 4.8 in/day under a potential rate of 0.2 in/day. Flows, depths and routing results are not affected; the binary output's evaporation series is. Nothing warns |
| **Reached from** | `[FILES] USE RUNOFF` with a file written by `SAVE RUNOFF`, any unit system |
| **5.3.0** | `runoff_readFromFile()` in [`src/legacy/engine/runoff.c:459`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L459); written in `subcatch_getResults()` at [`src/legacy/engine/subcatch.c:893`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L893) |
| **5.2.4** | Same code, [`src/solver/runoff.c:452`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/runoff.c#L452) and [`subcatch.c:860`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L860) |
| **6.0.0** | Reproduces with the same numbers, on purpose: `RunoffInterfaceFile::readResults()` ([`src/engine/hydrology/RunoffInterface.cpp:218`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/RunoffInterface.cpp#L218)) copies the legacy asymmetry and calls it "a deliberate legacy-parity quirk" |
| **Since** | Every release in the repository (5.1.000 has the same two lines) |
| **Fix** | Divide by `UCF(EVAPRATE)` on reading: [`IO-45_swmm530.patch`](IO-45_swmm530.patch), [`IO-45_swmm600.patch`](IO-45_swmm600.patch) |

## The problem

A runoff interface file lets a model's runoff be computed once and reused: one run saves the subcatchment results of every runoff step with `SAVE RUNOFF`, later runs read them with `USE RUNOFF` and only route. The run that reads the file should report the same subcatchment results as the run that wrote it.

For evaporation it does not. The test deck has one impervious 10-acre subcatchment with 0.2 in of depression storage, 0.5 in of rain in the first hour, and a constant potential evaporation of 0.2 in/day. The run that writes the file reports 0.2 in/day of evaporation for the first day, until the depression storage is dry. The run that reads the file reports 4.8 in/day for the same hours, 24 times as much and 24 times the potential rate.

The binary output's system evaporation series is the area-weighted sum of the subcatchment values ([`output.c:614`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L614)), so it is inflated the same way. Routing does not use subcatchment evaporation, so flows and depths are right.

## Why it happens

`runoff_saveToFile()` gets each subcatchment's results from `subcatch_getResults()`, which converts evaporation from ft/s to the user's evaporation units, in/day or mm/day:

```c
// src/legacy/engine/subcatch.c, subcatch_getResults()
    x[SUBCATCH_EVAP] = (float)(Subcatch[subcatchIndex].evapLoss * UCF(EVAPRATE));
    x[SUBCATCH_INFIL] = (float)(Subcatch[subcatchIndex].infilLoss * UCF(RAINFALL));
```

`runoff_readFromFile()` converts it back with the rainfall factor, in/hr or mm/hr, as it does for infiltration:

```c
// src/legacy/engine/runoff.c, runoff_readFromFile()
        Subcatch[j].evapLoss     = SubcatchResults[SUBCATCH_EVAP] /
                                   UCF(RAINFALL);
        Subcatch[j].infilLoss    = SubcatchResults[SUBCATCH_INFIL] /
                                   UCF(RAINFALL);
```

`UCF(EVAPRATE) / UCF(RAINFALL)` is 1036800 / 43200 = 24 in US units and 26334720 / 1097280 = 24 in SI units. When the reading run writes its own output, `subcatch_getResults()` multiplies by `UCF(EVAPRATE)` again, so the reported value is 24 times the saved one.

6.0.0 writes the same record and reads it the same way:

```cpp
// src/engine/hydrology/RunoffInterface.cpp, RunoffInterfaceFile::readResults()
        // runoff_readFromFile does — including its evap asymmetry (written
        // ×UCF(EVAPRATE) but read ÷UCF(RAINFALL), a deliberate legacy-parity
        // quirk). ...
        ctx.subcatches.evap_loss[uj]  = static_cast<double>(buf_[2])
                                        / ucf::Ucf[ucf::RAINFALL][us];
```

## How to reproduce

| File | What it is |
|---|---|
| [`IO-45_save.inp`](IO-45_save.inp) | One impervious subcatchment, 0.5 in of rain, 0.2 in of depression storage, potential evaporation 0.2 in/day, 2 days; `SAVE RUNOFF IO-45.rof` |
| [`IO-45_use.inp`](IO-45_use.inp) | The same model with `USE RUNOFF IO-45.rof` |
| [`IO-45_test.c`](IO-45_test.c) | Runs both and compares the subcatchment evaporation series in the two `.out` files (5.2.4 and 5.3.0, read with the output library) |
| [`IO-45_test6.c`](IO-45_test6.c) | The same with 6.0.0 and its output reader |

```sh
tools/run-test.sh IO-45            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-45 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 6.0.0 prints the same values, 5.2.4 the same series with totals 0.2244 and 5.3861):

```
hour   S1 evaporation (in/day)
       SAVE RUNOFF run   USE RUNOFF run
   1            0.2000           4.8000
   2            0.2000           4.8000
   3            0.2000           4.8000
   6            0.2000           4.8000
  12            0.2000           4.8000
  18            0.2000           4.8000
  24            0.2000           4.8000
  30            0.0000           0.0000
  36            0.0000           0.0000
  42            0.0000           0.0000
  48            0.0000           0.0000
maximum         0.2000           4.8000   (potential 0.2 in/day)
total           0.2245           5.3869   in (sum of the hourly values)
FAIL: the USE RUNOFF run reports evaporation up to 4.8000 in/day (24.0 times the run that wrote the file, potential 0.2 in/day)
IO-45 5.3.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same values):

```
  24            0.2000           0.2000
  30            0.0000           0.0000
maximum         0.2000           0.2000   (potential 0.2 in/day)
total           0.2245           0.2245   in (sum of the hourly values)
PASS: the USE RUNOFF run reports the evaporation of the run that wrote the file, within the potential rate
IO-45 5.3.0 patched: PASS
IO-45 6.0.0 patched: PASS
```

## The fix

Convert back with the factor the value was written with:

```diff
         Subcatch[j].evapLoss     = SubcatchResults[SUBCATCH_EVAP] /
-                                   UCF(RAINFALL);
+                                   UCF(EVAPRATE);
```

The 6.0.0 patch makes the same change in `RunoffInterfaceFile::readResults()` and corrects the two comments that describe the asymmetry as intended. The file format does not change, so existing runoff files read correctly with the fix. Only the evaporation reported by `USE RUNOFF` runs changes (by a factor of 1/24); no regression deck uses a runoff interface file.
