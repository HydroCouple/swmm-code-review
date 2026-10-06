# CRASH-19: The output reader dereferences a NULL handle right after testing for it

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Any `SMO_*` getter called with a NULL handle crashes the host application (null-pointer dereference) instead of returning the documented -1. The handle is NULL after `SMO_close()`, which sets it to NULL on purpose, and stays NULL when `SMO_init()` fails. |
| **Reached from** | 26 getters and `SMO_clearError()` in 5.3.0 (18 getters and `SMO_clearError()` in 5.2.4); of the functions that take a handle, only `SMO_open`, `SMO_close`, `SMO_getVersion`, `SMO_getFlowUnits` and `SMO_checkError` handle NULL |
| **5.3.0** | e.g. `SMO_getStartDate()` in [`src/legacy/output/swmm_output.c:751-756`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L751-L756); `SMO_getUnits()` at [line 649](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L649); `SMO_clearError()` at [line 1734](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1734) |
| **5.2.4** | Same code, e.g. [`src/outfile/swmm_output.c:438`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/outfile/swmm_output.c#L438) |
| **6.0.0** | Not affected: every `swmm_output_*` function tests the handle before using it ([`openswmm_output_impl.cpp:41`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/output/openswmm_output_impl.cpp#L41)), and `swmm_output_open()` returns NULL on failure |
| **Since** | 5.1.14, the first release that ships the output library (`src/outfile`) |
| **Fix** | Return the error from the NULL test: [`CRASH-19_swmm530.patch`](CRASH-19_swmm530.patch) |

## The problem

The output-file reader API is used through a handle: `SMO_init(&h)`, `SMO_open(h, path)`, getters, `SMO_close(&h)`. `SMO_close()` takes the handle's address so that it can set it to NULL, and every getter begins with an explicit test for a NULL handle that is meant to return -1 ("Error code 0 on success, -1 on failure or error code", in the header).

The test does not stop anything. A program that asks a closed handle for its start date, period count or any result series dies with a null-pointer dereference. Only `SMO_getVersion()`, `SMO_getFlowUnits()` and `SMO_checkError()` return -1 as intended.

## Why it happens

The NULL test sets the error code, and the return statement then reaches through the NULL pointer to record it:

```c
// src/legacy/output/swmm_output.c, SMO_getStartDate()
    if (p_data == NULL)
        errorcode = -1;
    else
        *date = p_data->StartDate;

    return set_error(p_data->error_handle, errorcode);   // p_data is NULL here
```

All series, attribute, result, element-name, unit and size getters share this shape. `SMO_getVersion()` and `SMO_getFlowUnits()` write `return -1;` in the NULL branch instead, which is why they survive. `SMO_getUnits()` fails even earlier, sizing its output from `p_data->Npolluts` before the NULL test, and `SMO_clearError()` has no test:

```c
// SMO_getUnits()
    *unitFlag = NULL;
    if (p_data->Npolluts > 0)
        *length = 2 + p_data->Npolluts;
...
// SMO_clearError()
    p_data = (data_t *)p_handle;
    clear_error(p_data->error_handle);
```

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-19_reader.inp`](CRASH-19_reader.inp) | One subcatchment, three junctions, an outfall and three conduits; writes a 24-period output file |
| [`CRASH-19_test.c`](CRASH-19_test.c) | Runs the deck, opens the file, closes it (the handle becomes NULL), then calls every reader function with the handle (5.2.4 and 5.3.0; the eight 5.3.0-only functions under `#ifdef`) |
| [`CRASH-19_test6.c`](CRASH-19_test6.c) | Calls every `swmm_output_*` function with the NULL handle that `swmm_output_open()` returns for a missing file |

```sh
tools/run-test.sh CRASH-19            # 5.2.4 and 5.3.0: CRASH; 6.0.0: PASS
tools/run-test.sh CRASH-19 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix**, 5.3.0 (and 5.2.4, at `swmm_output.c:438`) stops at the first unguarded call:

```
SMO_open/getStartDate: error 0, start date 43831.0; after SMO_close the handle is NULL

  call with the NULL handle  return
  SMO_getVersion                -1
  SMO_getFlowUnits              -1
  SMO_checkError                -1
../src/src/legacy/output/swmm_output.c:756:30: runtime error: member access within null pointer of type 'data_t'
    #0 0x7f7f181ee9af in SMO_getStartDate .../src/legacy/output/swmm_output.c
    #1 0x5595a2200e04 in main .../530-base/CRASH-19_test.c:78:30
CRASH-19 5.3.0 base: CRASH
```

6.0.0 returns -1 or NULL from all 28 calls (excerpt):

```
swmm_output_open(missing file) returned NULL

  call with the NULL handle            return
  swmm_output_get_version                 -1
  swmm_output_get_start_date              -1
  swmm_output_get_node_id               NULL
  swmm_output_get_system_result           -1
  swmm_output_get_node_series             -1
  swmm_output_close                    (void)
PASS: every output reader call with a NULL handle returned -1 (or NULL) without crashing
CRASH-19 6.0.0 base: PASS
```

**With the fix**, every 5.3.0 call returns its error code (excerpt):

```
  SMO_checkError                -1
  SMO_getStartDate              -1
  SMO_getTimes                  -1
  SMO_getUnits                  -1
  SMO_getElementName           410
  SMO_getSystemResult           -1
  SMO_getPropertyValues         -1
  SMO_clearError             (void)
PASS: every reader call with a NULL handle returned an error code without crashing
CRASH-19 5.3.0 patched: PASS
```

`SMO_getElementName()` has always used 410 for a NULL handle; the patch keeps it.

## The fix

In each of the 26 getters the NULL branch returns instead of setting the code; `SMO_getUnits()` and `SMO_clearError()` test the pointer before using it:

```diff
     if (p_data == NULL)
-        errorcode = -1;
+        return -1;
     else
         *date = p_data->StartDate;
...
-    if (p_data->Npolluts > 0)
+    if (p_data != NULL && p_data->Npolluts > 0)
...
-    clear_error(p_data->error_handle);
+    if (p_data != NULL)
+        clear_error(p_data->error_handle);
```

Nothing changes for a valid handle. 6.0.0 needs no change.
