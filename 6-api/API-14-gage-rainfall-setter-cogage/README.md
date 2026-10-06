# API-14: Gage rainfall set through the API is ignored for a gage that shares its time series with another gage

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | `swmm_setValue(swmm_GAGE_RAINFALL, ...)` returns without error but has no effect on a gage whose time series is also used by an earlier gage. Its subcatchments get the series' rainfall instead, and the .out reports the series rainfall too. In the test, S2 receives 1.50 in instead of 3.00 in and peaks at 5.042 cfs instead of 10.084 cfs. A real-time or coupled application that gives all its gages the same placeholder series and drives them through the API gets rain only on the first one. In 6.0.0, `swmm_gage_set_rainfall()` is ignored for every gage. |
| **Reached from** | `swmm_setValue(swmm_GAGE_RAINFALL, ...)` (5.2.4, 5.3.0) or `swmm_gage_set_rainfall()` (6.0.0, also behind the Python `Gage.rainfall` property) during a run, on a gage whose `TIMESERIES` is also used by a gage listed earlier in `[RAINGAGES]` |
| **5.3.0** | `gage_setState()` in [`src/legacy/engine/gage.c:357`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L357) and `gage_setReportRainfall()` in [`gage.c:540`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L540) |
| **5.2.4** | Same code, [`src/solver/gage.c:343`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gage.c#L343) and [`gage.c:535`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gage.c#L535) |
| **6.0.0** | Reproduces, for every gage: `swmm_gage_set_rainfall()` ([`src/engine/core/openswmm_gages_impl.cpp:352`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_gages_impl.cpp#L352)) writes the gage's current rainfall, which the next runoff step recomputes from the series. The runoff step checks the API override before the co-gage ([`src/engine/hydrology/Gage.cpp:344`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Gage.cpp#L344)), but nothing sets that override, and the report path checks the co-gage first ([`Gage.cpp:409`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Gage.cpp#L409)) |
| **Since** | 5.2.0, which added API rainfall after the existing co-gage shortcut; 6.0.0's setter since fork commit 4e29c886 (March 2026) |
| **Fix** | Check the API rainfall before the co-gage; in 6.0.0, also make the setter store the override: [`API-14_swmm530.patch`](API-14_swmm530.patch), [`API-14_swmm600.patch`](API-14_swmm600.patch) |

## The problem

A gage that reads the same time series as a gage listed before it is a "co-gage": instead of reading the series a second time, it copies the earlier gage's rainfall at each step. `gage_validate()` sets this up for every used gage that names an already used series ([gage.c:247](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L247)). It is invisible in the input file and to the API.

When an application sets a gage's rainfall with `swmm_setValue(swmm_GAGE_RAINFALL, index, value)`, the value is stored and the call succeeds. For a co-gage it is never used. Applications that drive rainfall from outside, such as real-time control or coupling with a weather model, typically give every gage the same dummy series and then set each one through the API. All gages after the first are co-gages of the first, so only the first gage's rainfall takes effect.

The test deck has gages G1 and G2 on the series TS1 (0.5 in/hr for 3 hours) and G3 on TS3, an identical series under a different name. Three identical subcatchments drain them. The test sets 1.0 in/hr on G2 and G3 before every step. S3 gets 3.00 in and S2, whose gage is a co-gage of G1, gets G1's 1.50 in, with half the peak runoff of S3.

## Why it happens

`gage_setState()` copies the co-gage's rainfall before it looks at the API value:

```c
// src/legacy/engine/gage.c, gage_setState()
    // --- use rainfall from co-gage (gage with lower index that uses
    //     same rainfall time series or file) if it exists
    if ( Gage[j].coGage >= 0)
    {   
        int cg = Gage[j].coGage;
        // Apply rainfall scaling factor to co-gage's rainfall
        Gage[j].rainfall = Gage[cg].rainfall * Gage[j].scaleFactor / Gage[cg].scaleFactor;
        return;
    }

    // --- use rainfall supplied by API function call
    //     (where constant ZERO (1.e-10) is used for 0 rainfall)
    if (Gage[j].apiRainfall != MISSING)
    {
        Gage[j].rainfall = Gage[j].apiRainfall;
        return;
    }
```

`gage_setReportRainfall()`, which supplies the rainfall written to the .out, has the same order. The API branch was added in 5.2.0 below the co-gage branch, which predates it.

6.0.0's runoff step (`updateAllGages()`) has the right order, but its setter does not use it:

```cpp
// src/engine/core/openswmm_gages_impl.cpp, swmm_gage_set_rainfall()
    ctx.gages.rainfall[static_cast<std::size_t>(idx)] = rainfall;
```

The next runoff step's `updateAllGages()` overwrites `gages.rainfall` from the series (or with zero when the gage has no data). The field that `updateAllGages()` checks first, `api_rainfall`, is never set by any API function. So in 6.0.0 the setter has no effect on any gage, and S3 gets 1.50 in as well.

## How to reproduce

| File | What it is |
|---|---|
| [`API-14_shared-series.inp`](API-14_shared-series.inp) | G1 and G2 on series TS1, G3 on its copy TS3 (0.5 in/hr for 3 h); three identical impervious subcatchments S1 to S3 |
| [`API-14_test.c`](API-14_test.c) | Sets 1.0 in/hr on G2 and G3 with `swmm_setValue` before every step (5.2.4 and 5.3.0); reads Total Precip from the Subcatchment Runoff Summary, peak runoff with `swmm_getValue`, and the subcatchment rainfall written to the .out |
| [`API-14_test6.c`](API-14_test6.c) | The same through `swmm_gage_set_rainfall()` and the 6.0.0 output reader |

```sh
tools/run-test.sh API-14            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh API-14 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print the same:

```
Subcatch  gage  API rain   Total Precip  expected   peak runoff   .out rainfall
                (in/hr)    (in)          (in)       (cfs)         min - max (in/hr)
S1        G1    -              1.50        1.50        5.042     0.500 - 0.500
S2        G2    1.0            1.50        3.00        5.042     0.500 - 0.500
S3        G3    1.0            3.00        3.00       10.084     1.000 - 1.000
FAIL: the API rainfall is not used: S2 got 1.50 in (peak 5.042 cfs, reported 0.500 in/hr), S3 3.00 in (peak 10.084 cfs, reported 1.000 in/hr); both should get 3.00 in at 1.000 in/hr
API-14 5.2.4 base: FAIL
API-14 5.3.0 base: FAIL
```

6.0.0 ignores the setter on G3 as well:

```
S2        G2    1.0            1.50        3.00        5.042     0.500 - 0.500
S3        G3    1.0            1.50        3.00        5.042     0.500 - 0.500
API-14 6.0.0 base: FAIL
```

**With the fix**, both engines print:

```
S1        G1    -              1.50        1.50        5.042     0.500 - 0.500
S2        G2    1.0            3.00        3.00       10.084     1.000 - 1.000
S3        G3    1.0            3.00        3.00       10.084     1.000 - 1.000
PASS: G2 and G3 both use their API rainfall (3.00 in, same runoff); G1 keeps its series (1.50 in)
API-14 5.3.0 patched: PASS
API-14 6.0.0 patched: PASS
```

## The fix

In 5.3.0, move the API check above the co-gage branch in `gage_setState()` and likewise in `gage_setReportRainfall()`:

```diff
+    // --- use rainfall supplied by API function call
+    //     (where constant ZERO (1.e-10) is used for 0 rainfall)
+    //     (checked before the co-gage so that it also applies to a gage
+    //     that shares its time series with another gage)
+    if (Gage[j].apiRainfall != MISSING)
+    {
+        Gage[j].rainfall = Gage[j].apiRainfall;
+        return;
+    }
+
     // --- use rainfall from co-gage (gage with lower index that uses
     //     same rainfall time series or file) if it exists
     if ( Gage[j].coGage >= 0)
     ...
-    // --- use rainfall supplied by API function call
-    ...
```

In 6.0.0, `swmm_gage_set_rainfall()` also stores the value as the gage's `api_rainfall`, and `reportRainfall()` checks that override before the co-gage. The override then lasts until it is set again, as in legacy; the header comment, which said "applied for one timestep only", now says so, and that a negative value returns the gage to its own data.

Runs that do not set gage rainfall through the API are unchanged.

The co-gage shortcut has a second effect that this fix leaves in place: an API rainfall set on the first gage of a shared series is copied to the later gages on that series that have no API value of their own. With the test deck, setting only G1 gives S2 1.0 in/hr as well (peak 10.084 cfs instead of 5.042 cfs in 5.2.4 and 5.3.0); 6.0.0's `updateAllGages()` copies the first gage's rainfall the same way once its setter works. Fixing that needs the series rate kept separately from the override, because a co-gage cannot read a shared series on its own (the series has a single read position).
