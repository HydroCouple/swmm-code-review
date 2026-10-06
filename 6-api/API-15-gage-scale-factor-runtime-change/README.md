# API-15: A gage scale factor changed during a run applies late to its gage and rescales the gages that share its series

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | After `swmm_setValue(swmm_GAGE_SCALEFACTOR, ...)` the gage keeps its old factor until the current record and the next one have passed, while every other gage on the same time series immediately gets the wrong rainfall. In the test (hourly records of 1.0 in/hr, factor of G1 set to 2.0 at 2:30), S1 receives 6.00 in instead of 7.50 in and S2, whose gage was not touched, falls to 0.5 in/hr for 1.5 hours (4.25 in instead of 5.00 in). 6.0.0 is off for the rest of the current record (7.00 and 4.75 in). Nothing reports it. |
| **Reached from** | `swmm_setValue(swmm_GAGE_SCALEFACTOR, ...)` or `swmm_setValueExpanded(swmm_GAGE, swmm_GAGE_SCALEFACTOR, ...)` during a run (5.3.0); `swmm_gage_set_scale_factor()` during a run (6.0.0) |
| **5.3.0** | The setters in [`src/legacy/engine/swmm5.c:1402`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L1402) and [`swmm5.c:1999`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L1999); the factor is applied when a record is read, in `convertRainfall()` ([`gage.c:710`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L710)) |
| **5.2.4** | Not affected: 5.2.4 has no rain gage scale factor |
| **6.0.0** | Reproduces for the current record: `recordRate()` applies the factor when a record becomes current ([`src/engine/hydrology/Gage.cpp:231`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Gage.cpp#L231)), and `swmm_gage_set_scale_factor()` ([`src/engine/core/openswmm_gages_impl.cpp:227`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_gages_impl.cpp#L227)) only stores the factor, although its header says the new value "takes effect on the next timestep" |
| **Since** | 5.3.0, fork commits e791d7ed and 3ab67ce3 (May 2026, #46), which added the gage scale factor and its setters |
| **Fix** | Rescale the rainfall already read when the factor changes: [`API-15_swmm530.patch`](API-15_swmm530.patch), [`API-15_swmm600.patch`](API-15_swmm600.patch) |

## The problem

5.3.0 added a rainfall scale factor to rain gages (the optional last field of a `[RAINGAGES]` line) and made it settable through the API, for calibration and parameter sweeps that change it while a run is in progress. The setter only stores the new value:

```c
// src/legacy/engine/swmm5.c, swmm_setValue()
    case swmm_GAGE_SCALEFACTOR:
        ...
        if (value > 0.0)
            Gage[index].scaleFactor = value;
        return 0;
```

The rainfall the gage is applying at that moment, and the next record's rainfall that it has already read ahead, were both converted with the old factor. The gage changes over only when it reads the record after those two.

Gages that share the time series behave worse. A gage whose series is also used by a gage listed earlier copies that gage's rainfall and corrects for the difference between the two factors:

```c
// src/legacy/engine/gage.c, gage_setState()
        Gage[j].rainfall = Gage[cg].rainfall * Gage[j].scaleFactor / Gage[cg].scaleFactor;
```

Right after the change, `Gage[cg].rainfall` still carries the old factor but is divided by the new one, so the other gage's rainfall changes by old/new although nothing was set on it.

In the test, G1 and G2 read the same series of hourly 1.0 in/hr records and G1's factor goes from 1.0 to 2.0 at 2:30:

| Period | S1 (G1, factor 2.0 from 2:30) | S2 (G2, factor 1.0) | Expected S1 / S2 |
|---|---|---|---|
| 2:30 - 3:00 | 1.0 in/hr | 0.5 in/hr | 2.0 / 1.0 |
| 3:00 - 4:00 | 1.0 in/hr | 0.5 in/hr | 2.0 / 1.0 |
| 4:00 - 5:00 | 2.0 in/hr | 1.0 in/hr | 2.0 / 1.0 |

The 2:00 record was current and the 3:00 record had been read at 2:00, both with factor 1.0. Only the 4:00 record, read at 3:00, gets 2.0.

## Why it happens

`convertRainfall()` returns `r1 * Gage[j].unitsFactor * Gage[j].scaleFactor * Adjust.rainFactor`, and its result is stored in `Gage[j].rainfall` (first record) and `Gage[j].nextRainfall` (every later record, read when the record before it becomes current, [gage.c:415](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L415)). The factor is therefore part of two stored rates, and a new factor only reaches rates read after it was set. The co-gage formula ([gage.c:361](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L361)) assumes that `Gage[cg].rainfall` always carries `Gage[cg].scaleFactor`, which is false until those rates have passed.

6.0.0 keeps the next record as an index and computes its rate when it becomes current, so only the current record's rate (`st_rain`) carries the old factor. The co-gage copy in `updateAllGages()` ([Gage.cpp:357](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Gage.cpp#L357)) has the same formula, so S2 is wrong for the rest of that record.

## How to reproduce

| File | What it is |
|---|---|
| [`API-15_shared-series.inp`](API-15_shared-series.inp) | G1 and G2 on series TS1 (hourly records of 1.0 in/hr, 0:00 to 5:00); identical impervious subcatchments S1 and S2 |
| [`API-15_test.c`](API-15_test.c) | Sets G1's factor to 2.0 at 2:30 with `swmm_setValue`, reads both subcatchments' rainfall after every step and Total Precip from the Subcatchment Runoff Summary (5.3.0; prints PASS on 5.2.4, which has no such property) |
| [`API-15_test6.c`](API-15_test6.c) | The same with `swmm_gage_set_scale_factor()` |

```sh
tools/run-test.sh API-15            # 5.2.4: PASS (not affected); 5.3.0 and 6.0.0: FAIL
tools/run-test.sh API-15 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.3.0:

```
time    S1 rain  expected   S2 rain  expected   (in/hr)
 2.5 h    1.000    1.000      1.000    1.000
-- scale factor of G1 set to 2.0 at 2.50 h
 3.0 h    1.000    2.000      0.500    1.000
 3.5 h    1.000    2.000      0.500    1.000
 4.0 h    1.000    2.000      0.500    1.000
 4.5 h    2.000    2.000      1.000    1.000
 5.0 h    2.000    2.000      1.000    1.000
Total Precip (in): S1 6.00 (expected 7.50), S2 4.25 (expected 5.00)
steps with a wrong rate: 86
FAIL: the new scale factor of G1 is applied late (S1 6.00 in instead of 7.50) and changes G2 (S2 4.25 in instead of 5.00); 86 steps wrong
API-15 5.3.0 base: FAIL
```

6.0.0 is wrong until the end of the current record:

```
 3.0 h    1.000    2.000      0.500    1.000
 3.5 h    2.000    2.000      1.000    1.000
Total Precip (in): S1 7.00 (expected 7.50), S2 4.75 (expected 5.00)
steps with a wrong rate: 26
API-15 6.0.0 base: FAIL
```

(The rate printed at a time is the one of the runoff step that ended then, so the 3.0 h line shows the 2:00 record and the 4.0 h line the 3:00 record.)

**With the fix**, both engines print:

```
-- scale factor of G1 set to 2.0 at 2.50 h
 3.0 h    2.000    2.000      1.000    1.000
 3.5 h    2.000    2.000      1.000    1.000
 4.0 h    2.000    2.000      1.000    1.000
 4.5 h    2.000    2.000      1.000    1.000
 5.0 h    2.000    2.000      1.000    1.000
Total Precip (in): S1 7.50 (expected 7.50), S2 5.00 (expected 5.00)
steps with a wrong rate: 0
PASS: G1's new scale factor applies from the next step and G2 is unchanged (7.50 and 5.00 in)
API-15 5.3.0 patched: PASS
API-15 6.0.0 patched: PASS
```

## The fix

When the factor changes, multiply the rates that were converted with the old factor by new/old. In 5.3.0 these are the current and the read-ahead rainfall, in both setters:

```diff
         if (value > 0.0)
-            Gage[index].scaleFactor = value;
+        {
+            // --- rescale the current and read-ahead rainfall, which were
+            //     converted with the old factor
+            Gage[index].rainfall *= value / Gage[index].scaleFactor;
+            Gage[index].nextRainfall *= value / Gage[index].scaleFactor;
+            Gage[index].scaleFactor = value;
+        }
```

In 6.0.0 it is the current record's rate `st_rain`. The co-gage formula then holds again at every step, and the new factor applies from the next runoff step, on the changed gage only. The monthly `[ADJUSTMENTS]` factor and the units factor in the stored rates are kept, since only the ratio of the scale factors is applied. Rainfall that has already fallen (the gage's past-rain totals) is not changed.

Storing unscaled rates and applying the factor where the rainfall is used would also work, but `Gage[j].rainfall` is read in `gage.c`, `runoff.c`, `rdii.c` and `controls.c`, so that change would be much larger. Runs that do not change a scale factor through the API are unchanged.
