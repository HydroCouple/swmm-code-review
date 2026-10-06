# CRASH-15: The land-use buildup setters accept any number as an external-buildup time-series index

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | 5.3.0: a buildup function made EXTERNAL through the API with a coefficient 3 that is not a time-series index reads `Tseries[]` out of bounds at the first runoff step: a heap overflow one past the end, a segmentation fault further out, or undefined `int` conversion for a large value. A smaller wrong index silently drives buildup from an unrelated series. 6.0.0 range-checks the index at run time but converts it to `int` first, which is undefined for a large value; an index past the tables is accepted and silently gives no loading. The setters return success in every case. |
| **Reached from** | 5.3.0: `swmm_setValueExpanded(swmm_LANDUSE, swmm_LANDUSE_BUILDUP_COEFF3, ...)` on an EXTERNAL function, or `swmm_LANDUSE_BUILDUP_FUNC` = 4 (EXTERNAL) on another function, before or during a run. 6.0.0: `swmm_buildup_set()` with `func_type` 4 |
| **5.3.0** | `setLanduseValue()` in [`src/legacy/engine/swmm5.c:1819-1838`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L1819-L1838); the index is used by `landuse_getExternalBuildup()`, [`src/legacy/engine/landuse.c:714-723`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/landuse.c#L714-L723) |
| **5.2.4** | Not affected: the toolkit has no land-use setters |
| **6.0.0** | `swmm_buildup_set()` in [`src/engine/core/openswmm_quality_impl.cpp:193-232`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_quality_impl.cpp#L193-L232) stores any value; `SWMMEngine::stepSurfaceQuality()` converts it at [`SWMMEngine.cpp:3773`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L3773) before checking the range |
| **Since** | 5.3.0, fork commit e061db0a (June 2026, runtime buildup/washoff coefficients); 6.0.0's `swmm_buildup_set()` from 4e29c886 (March 2026) |
| **Fix** | Reject an EXTERNAL function whose coefficient 3 is not the index of a time series: [`CRASH-15_swmm530.patch`](CRASH-15_swmm530.patch), [`CRASH-15_swmm600.patch`](CRASH-15_swmm600.patch) |

## The problem

An EXTERNAL buildup function (`[BUILDUP] ... EXT max scale series`) takes its buildup rate from a time series. Its third coefficient is not a number the user types: the input parser looks up the series name and stores the series' index there. The toolkit setters in 5.3.0 let a program edit that coefficient as a plain number (any value ≥ 0) and change the function code to EXTERNAL without looking at the coefficient. 6.0.0's `swmm_buildup_set()` takes the function code and all three coefficients and stores them without checks.

The test deck has two time series, so the valid indices are 0 and 1:

- 5.3.0, coefficient 3 set to 2 on an EXTERNAL function: the first runoff step reads one element past the end of `Tseries[]` (AddressSanitizer heap-buffer-overflow).
- 5.3.0, a power function given coefficient 3 = 1000 and then switched to EXTERNAL: the first runoff step reads `Tseries[1000]` (segmentation fault).
- Both engines, coefficient 3 = 3e9: the conversion to `int` is undefined behaviour (UndefinedBehaviorSanitizer). On x86-64 the result is `INT_MIN`, so the lookup is skipped and the run continues with no external loading.
- 6.0.0 with 2 or 1000: the run-time range check skips the lookup, so the function silently adds nothing.

Without a sanitizer, a 5.3.0 index a little past the end reads whatever memory follows the array, and a valid but wrong index (say, the rain gage's series) drives the buildup from an unrelated series without any message.

## Why it happens

```c
// src/legacy/engine/landuse.c, landuse_readBuildupParams(), EXTERNAL_BUILDUP
        n = project_findObject(TSERIES, tok[5]);           //time series
        if ( n < 0 ) return error_setInpError(ERR_NAME, tok[4]);
        Tseries[n].refersTo = EXTERNAL_BUILDUP;
        c[2] = n;
...
// landuse_getExternalBuildup()
    int    ts = (int)floor(Landuse[i].buildupFunc[p].coeff[2]);  // time series index
    ...
    if ( ts >= 0 )
    {
        rate = sf * table_tseriesLookup(&Tseries[ts],
               getDateTime(NewRunoffTime), FALSE);
    }
```

```c
// src/legacy/engine/swmm5.c, setLanduseValue()
    case swmm_LANDUSE_BUILDUP_FUNC:
        Landuse[index].buildupFunc[subIndex].funcType = (int)value;
        recomputeBuildupMaxDays(index, subIndex);
        return 0;
    ...
    case swmm_LANDUSE_BUILDUP_COEFF3:
        if (value < 0.0) return ERR_API_PROPERTY_VALUE;
        Landuse[index].buildupFunc[subIndex].coeff[2] = value;
```

6.0.0 has the bound but converts first:

```cpp
// src/engine/core/SWMMEngine.cpp, stepSurfaceQuality()
    int ts_idx    = static_cast<int>(bp.coeff[2]);
    double rate   = 0.0;
    if (ts_idx >= 0 &&
        ts_idx < static_cast<int>(ctx_.tables.tables.size())) {
```

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-15_buildup.inp`](CRASH-15_buildup.inp) | One subcatchment with land use RES: TSS builds up from series TSB (EXTERNAL), LEAD by a power function. Two time series: TSR (index 0, rain, all zero) and TSB (index 1). One day, no rain |
| [`CRASH-15_test.c`](CRASH-15_test.c) | Legacy API. Case 0 sets COEFF3 = 1 (TSB) on TSS and must be accepted; cases 1 to 3 set COEFF3 = 2 and 3e9 on TSS, and COEFF3 = 1000 followed by FUNC = 4 on LEAD, and must be rejected. Each case runs the whole day (5.3.0 only, `#ifdef`; 5.2.4 prints PASS because the setters do not exist) |
| [`CRASH-15_test6.c`](CRASH-15_test6.c) | The same four cases with `swmm_buildup_set()` between `swmm_engine_open()` and `swmm_engine_initialize()` |
| [`CRASH-15_swmm530.patch`](CRASH-15_swmm530.patch), [`CRASH-15_swmm600.patch`](CRASH-15_swmm600.patch) | The fixes |

```sh
tools/run-test.sh CRASH-15            # 5.2.4 PASS, 5.3.0 CRASH, 6.0.0 CRASH
tools/run-test.sh CRASH-15 --patched  # 5.3.0 PASS, 6.0.0 PASS
```

**Without the fix**, 5.3.0 stops in case 1:

```
0. TSS (EXTERNAL): BUILDUP_COEFF3 = 1 (TSB, a valid index; must be accepted)
  setter return codes: 0 
  run finished, error code 0
1. TSS (EXTERNAL): BUILDUP_COEFF3 = 2 (there are 2 time series)
  setter return codes: 0 
==22981==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x517000000a80 at pc 0x7f510e1d8d65 bp 0x7ffdefa99500 sp 0x7ffdefa994f8
READ of size 8 at 0x517000000a80 thread T0
    #0 0x7f510e1d8d64 in table_tseriesLookup table.c:765:17
    #1 0x7f510e1101a8 in landuse_getExternalBuildup landuse.c:723:21
    #2 0x7f510e1101a8 in landuse_getBuildup landuse.c:436:16
    #3 0x7f510e1bc900 in surfqual_getBuildup surfqual.c:146:26
CRASH-15 5.3.0 base: CRASH
```

Run on their own (the harness stops at the first report), cases 2 and 3 give:

```
2. TSS (EXTERNAL): BUILDUP_COEFF3 = 3e9
  setter return codes: 0 
landuse.c:714:17: runtime error: 3e+09 is outside the range of representable values of type 'int'
  run finished, error code 0
3. LEAD (POWER): BUILDUP_COEFF3 = 1000, then BUILDUP_FUNC = 4 (EXTERNAL)
  setter return codes: 0 0
==24437==ERROR: AddressSanitizer: SEGV on unknown address 0x517000059e20 (pc 0x7f9d59fd885e bp 0x7ffdc733ff30 sp 0x7ffdc733fed0 T0)
    #0 0x7f9d59fd885e in table_tseriesLookup table.c:765:17
    #1 0x7f9d59f101a8 in landuse_getExternalBuildup landuse.c:723:21
```

6.0.0 accepts all three invalid indices; case 2 is undefined behaviour:

```
1. TSS: EXTERNAL with c3 = 2 (there are 2 time series)
  swmm_buildup_set returned 0
  run finished, error code 0
2. TSS: EXTERNAL with c3 = 3e9
  swmm_buildup_set returned 0
SWMMEngine.cpp:3773:62: runtime error: 3e+09 is outside the range of representable values of type 'int'
  run finished, error code 0
3. LEAD: POWER changed to EXTERNAL with c3 = 1000
  swmm_buildup_set returned 0
  run finished, error code 0
FAIL: 3 of 4 cases went wrong (an invalid time-series index accepted, or a valid one rejected)
CRASH-15 6.0.0 base: CRASH
```

**With the fix**, the valid index is accepted and the invalid ones are rejected (5.3.0 returns `ERR_API_PROPERTY_VALUE`, -999908; 6.0.0 returns `SWMM_ERR_BADPARAM`, 9); every run completes with the deck's own functions:

```
0. TSS (EXTERNAL): BUILDUP_COEFF3 = 1 (TSB, a valid index; must be accepted)
  setter return codes: 0 
  run finished, error code 0
1. TSS (EXTERNAL): BUILDUP_COEFF3 = 2 (there are 2 time series)
  setter return codes: -999908 
  run finished, error code 0
2. TSS (EXTERNAL): BUILDUP_COEFF3 = 3e9
  setter return codes: -999908 
  run finished, error code 0
3. LEAD (POWER): BUILDUP_COEFF3 = 1000, then BUILDUP_FUNC = 4 (EXTERNAL)
  setter return codes: 0 -999908
  run finished, error code 0
PASS: the valid time-series index was accepted, every invalid one was rejected, and the runs completed
CRASH-15 5.3.0 patched: PASS
```

## The fix

5.3.0 refuses a change that would leave an EXTERNAL function with a coefficient 3 at or beyond the number of time series (coefficient 3 is already kept ≥ 0):

```diff
     case swmm_LANDUSE_BUILDUP_FUNC:
+        // --- coefficient 3 of an external function is a time series index
+        if ((int)value == EXTERNAL_BUILDUP &&
+            Landuse[index].buildupFunc[subIndex].coeff[2] >= Nobjects[TSERIES])
+            return ERR_API_PROPERTY_VALUE;
         Landuse[index].buildupFunc[subIndex].funcType = (int)value;
 ...
     case swmm_LANDUSE_BUILDUP_COEFF3:
         if (value < 0.0) return ERR_API_PROPERTY_VALUE;
+        if (Landuse[index].buildupFunc[subIndex].funcType == EXTERNAL_BUILDUP &&
+            value >= Nobjects[TSERIES])
+            return ERR_API_PROPERTY_VALUE;
```

To switch a function to EXTERNAL, a program sets coefficient 3 to the series index first (any value ≥ 0 is accepted while the function is not EXTERNAL), then the function code.

6.0.0's `swmm_buildup_set()` returns `SWMM_ERR_BADPARAM` when `func_type` is 4 and `c3` is not the index of a table of type TIMESERIES, before the pollutant and reactions-species branches. Input files and valid API calls are unaffected.

Not changed: the 5.3.0 setters still take any other function, normalizer or washoff code without a range check (`(int)value` is stored as is). Those codes are only compared, never used as an index, so they do not crash; the conversion itself is undefined for values outside the range of `int`.
