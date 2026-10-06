# BND-11: A hot start drops the groundwater outflow for the first runoff step

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | A run continued from a hot start file does not reproduce the uninterrupted run. The groundwater inflow to the drainage system restarts from 0 and ramps back up over the first runoff step, although the aquifer state is restored and drains at the full rate. A 100-acre aquifer discharging 7.8 cfs: J1 receives 1.81 cfs instead of 7.80 cfs 15 minutes after the restart, and 0.316 instead of 0.644 ac-ft in the first hour (-51 %). The missing water shows up only as a groundwater continuity error (0.071 %, small because it is taken relative to the aquifer's whole storage). Chained hot-start runs lose this volume at every restart. |
| **Reached from** | `[FILES] USE HOTSTART` with a file saved by `SAVE HOTSTART` (5.2.4, 5.3.0) or `swmm_hotstart_apply()` with a file from `swmm_hotstart_save()` (6.0.0), for any subcatchment with groundwater that is discharging |
| **5.3.0** | `gwater_setState()` restores the saved flow into `oldFlow` only, [`src/legacy/engine/gwater.c:459`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L459); the first `gwater_getGroundwater()` overwrites it with `newFlow` = 0, [`gwater.c:595`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L595) |
| **5.2.4** | Same code, [`src/solver/gwater.c:452`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gwater.c#L452) |
| **6.0.0** | Reproduces through the native hot start API: the restored flow goes to `subcatches.gw_flow` ([`src/engine/core/HotStartManager.cpp:1203`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/HotStartManager.cpp#L1203)), not to the GW solver's rate, and the first runoff step skips setting the old rate ([`src/engine/core/SWMMEngine.cpp:1845`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L1845)). `[FILES]` hot starts cannot continue this model at all (see below) |
| **Since** | 5.1.000, which added `gwater_setState()` (fork history commit fa32a734) |
| **Fix** | Restore the saved flow as both the old and the new flow: [`BND-11_swmm530.patch`](BND-11_swmm530.patch), [`BND-11_swmm600.patch`](BND-11_swmm600.patch) |

## The problem

A hot start file exists to let a run continue where another stopped, for example in operational forecasting or to skip a warm-up period. The test deck has a 100-acre aquifer draining steadily to junction J1, with no rain, so J1's lateral inflow is the groundwater flow only. `BND-11_full.inp` runs 0:00 to 12:00. `BND-11_save.inp` runs 0:00 to 6:00 and saves a hot start file, and `BND-11_use.inp` continues from it to 12:00. All three engines give:

| Time | Uninterrupted run (cfs) | Hot-started run (cfs) |
|---|---|---|
| 6:15 | 7.797 | 1.814 |
| 6:30 | 7.789 | 3.757 |
| 6:45 | 7.781 | 5.700 |
| 7:00 | 7.773 | 7.643 |
| GW volume 6:00-7:00 | 0.6437 ac-ft | 0.3158 ac-ft |

After the first runoff step (DRY_STEP 1 h) the two runs agree again. The aquifer is restored correctly and ends at the same storage, 452.260 ac-ft, in both. In the continuation run the aquifer releases 3.822 ac-ft, but the groundwater continuity table books 3.500 ac-ft sent to the node: 0.32 ac-ft, half an hour of the 7.8 cfs baseflow, is lost.

## Why it happens

```c
// src/legacy/engine/gwater.c
void gwater_getState(int subcatchIndex, double x[])
    ...
    x[2] = gw->newFlow;                 // saved: the latest flow

void gwater_setState(int subcatchIndex, double x[])
    ...
    gw->oldFlow = x[2];                 // restored into oldFlow only;
                                        // newFlow stays 0 from gwater_initState()
```

The first runoff step of the continuation then shifts the flows:

```c
// gwater_getGroundwater()
    GW->oldFlow = GW->newFlow;          // 0: the restored value is lost
    GW->newFlow = GWFlow;
```

and routing interpolates the inflow between them, `q = ((1-f)*gw->oldFlow + f*gw->newFlow) * area` (`routing.c:766`), which ramps from 0.

6.0.0's `[FILES]` hot start keeps routing state only. `SAVE HOTSTART` writes a legacy-format file whose header counts the subcatchments but which has no runoff section, and `USE HOTSTART` of that file stops at initialisation with an empty message (`USE HOTSTART: `). This is a broader defect than BND-11, reported separately. Its native hot start (`swmm_hotstart_save()` / `swmm_hotstart_apply()`) does carry the aquifer state and the groundwater flow, but `HotStartManager::apply()` puts the flow into `subcatches.gw_flow` (cfs). The engine takes the old rate for the routing interpolation from the GW solver's own rate, which is still 0. On the first runoff step it does not even copy that rate, because the array it copies into is only sized later, in the routing step:

```cpp
// src/engine/core/SWMMEngine.cpp, stepRunoff()
            if (ui < old_gw_rate_.size())          // false on the first runoff step
                old_gw_rate_[ui] = ... groundwater_.state().gw_flow[ui] ...;
```

## How to reproduce

| File | What it is |
|---|---|
| [`BND-11_full.inp`](BND-11_full.inp) | 100-acre aquifer draining to J1, 0:00-12:00 in one run |
| [`BND-11_save.inp`](BND-11_save.inp) | The same, 0:00-6:00, `SAVE HOTSTART "BND-11.hsf"` |
| [`BND-11_use.inp`](BND-11_use.inp) | The same, 6:00-12:00, `USE HOTSTART "BND-11.hsf"` |
| [`BND-11_test.c`](BND-11_test.c) | Runs the three decks through the legacy toolkit, records J1's lateral inflow (`swmm_getValue`) at every routing step, and compares the 6:00-7:00 groundwater volume of the continuation with the uninterrupted run. Tolerance 1 % |
| [`BND-11_test6.c`](BND-11_test6.c) | The same check through the 6.0.0 C API, saving with `swmm_hotstart_save()` and continuing with `swmm_hotstart_apply()` (on `BND-11_use.inp` without its `[FILES]` section) |

The outfall is named `OUT01` so that 6.0.0's native hot start file is 260 bytes long. `swmm_hotstart_open()` reads the file's trailing CRC with a misaligned `uint32_t` load (`HotStartManager.cpp:336`), which UBSan reports whenever the file size is not a multiple of 4. That is a separate defect, reported separately. The trailing `*` in `[GROUNDWATER]` works around IO-15.

```sh
tools/run-test.sh BND-11            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh BND-11 --patched  # 5.3.0, 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0:

```
J1 lateral (groundwater) inflow, cfs
Time    Uninterrupted   Hot-started
 6:15           7.797         1.814
 6:30           7.789         3.757
 6:45           7.781         5.700
 7:00           7.773         7.643
GW volume to J1, ac-ft
6:00-7:00        0.6437        0.3158
6:00-12:00       3.8120        3.4841
FAIL: after the hot start J1 receives 0.3158 ac-ft of groundwater in the first hour instead of 0.6437 ac-ft (-50.9 %)
```

6.0.0 prints the same lines except the 6:00-12:00 totals, `3.8225        3.4946` (the difference from legacy, 0.0105 ac-ft, is one 60-s routing step at 7.6 cfs: the 6.0.0 step loop integrates one step more), and ends `BND-11 6.0.0 base: FAIL`.

**With the fix**, 5.3.0:

```
 6:15           7.797         7.797
 6:30           7.789         7.789
 6:45           7.781         7.781
 7:00           7.773         7.773
GW volume to J1, ac-ft
6:00-7:00        0.6437        0.6437
6:00-12:00       3.8120        3.8120
PASS: the hot-started run delivers the same groundwater inflow as the uninterrupted run (6:00-7:00 -0.00 %)
BND-11 5.3.0 patched: PASS
```

6.0.0 prints the same flows and 6:00-7:00 volumes (6:00-12:00 3.8225 in both columns) and `BND-11 6.0.0 patched: PASS`. In the 5.3.0 continuation run the groundwater continuity now books 3.822 ac-ft of groundwater flow, equal to the storage drop, and its continuity error falls from 0.071 % to 0.000 %.

## The fix

5.3.0:

```diff
     gw->oldFlow = x[2];
+    gw->newFlow = x[2];
```

`gwater_getGroundwater()` shifts `newFlow` into `oldFlow` before computing the new flow, so the first runoff step now interpolates from the saved flow, as the uninterrupted run does.

6.0.0: the `set_gw_state` accessor that `HotStartManager::apply()` calls after copying the record's flow also sets the GW solver's rate from that flow, and `stepRunoff()` sizes the array of old rates before its first copy:

```diff
+                const double a_ft2 = ctx_.subcatches.area[ui]
+                                   / ucf::UCF(ucf::LANDAREA, ctx_.options);
+                if (ui < gwa.gw_flow.size() && a_ft2 > 0.0)
+                    gwa.gw_flow[ui] = ctx_.subcatches.gw_flow[ui] / a_ft2;
 ...
+            if (old_gw_rate_.size() <= ui)   // not yet sized on the first runoff step
+                old_gw_rate_.resize(static_cast<std::size_t>(ctx_.n_subcatches()), 0.0);
```

A cold start is unchanged: the solver's rate is 0 on the first step, as before. Only continuations from a hot start change. `examples/Example5.inp` (groundwater, cold start) and `extran/extran8a.inp` (the regression deck that uses a hot start file, no groundwater) give identical reports with both patched engines.

With NUM-36's patch the ramp disappears as well, because the first step then sends the step-average outflow and no longer uses the restored rate; the two patches apply independently and agree.
