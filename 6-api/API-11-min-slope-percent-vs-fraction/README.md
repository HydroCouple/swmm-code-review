# API-11: The minimum-slope option reads back as a fraction, is written as given, and has no effect when set before the run

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | 5.3.0: with `MIN_SLOPE 0.5` (percent) in the input file, `swmm_MINSLOPE` reads 0.005; a value written through the API is stored without the division by 100, so 0.5 becomes a 50 % minimum slope, and it reads back as 0.5. In both 5.3.0 and 6.0.0, a minimum slope set after the input is read is never applied to the conduits: in the test, C1 keeps slope 0.0025 instead of 0.005 and J1 floods 862 ft³ instead of 0. No warning. |
| **Reached from** | 5.3.0: `swmm_getValueExpanded(swmm_SYSTEM, swmm_MINSLOPE, ...)` and `swmm_setValueExpanded(swmm_SYSTEM, swmm_MINSLOPE, ...)` between `swmm_open()` and `swmm_start()`. 6.0.0: `swmm_options_set(e, "MIN_SLOPE", ...)` between `swmm_engine_open()` and `swmm_engine_initialize()` |
| **5.3.0** | `getSystemValue()` [`src/legacy/engine/swmm5.c:2941-2942`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2941-L2942) and `setSystemValue()` [`swmm5.c:3430-3432`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L3430-L3432); the option is converted to a fraction in [`project.c:737`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L737) and applied in `conduit_getSlope()` [`link.c:1292`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1292) during `swmm_open()` |
| **5.2.4** | Not affected: there is no minimum-slope property in the toolkit ([`src/solver/swmm5.c:867`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L867)) |
| **6.0.0** | The units are consistent (percent on get and set, divided by 100 where it is applied, [`PostParseResolver.cpp:3459`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/PostParseResolver.cpp#L3459)), but the "no effect" part reproduces: [`swmm_options_set()`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_model_impl.cpp#L1473) stores the option after the conduit slopes were resolved |
| **Since** | 5.3.0, when the property was added (fork commit 5b87a2b5, December 2024, #204) |
| **Fix** | Percent on both sides in 5.3.0, and recompute the conduit slopes after a change in both engines: [`API-11_swmm530.patch`](API-11_swmm530.patch), [`API-11_swmm600.patch`](API-11_swmm600.patch). Both need [API-10](../API-10-invert-offset-setters-stale-slope/)'s patch first. |

## The problem

`MIN_SLOPE` in `[OPTIONS]` is "minimum conduit slope in percent" (input reference, chapter 2); any conduit flatter than that is given that slope. 5.3.0 exposes it through `swmm_MINSLOPE` and 6.0.0 through `swmm_options_set/_get("MIN_SLOPE")`.

In 5.3.0 the two directions disagree. The getter returns the internal fraction, so a model with `MIN_SLOPE 0.5` reads 0.005. The setter accepts the percent range (0 to 100) but stores the number as the fraction, so a caller who writes 0.5 meaning 0.5 % asks for a minimum slope of 50 %, and reads 0.5 back. A caller who copies the getter's value into the setter, or follows the input file's unit, gets a value 100 times off in one of the two directions.

In both engines, the value set before the run does nothing. The test model has C1 at a geometric slope of 0.0025 (0.25 %), too flat to carry the 6.5 cfs inflow peak, so J1 floods 862 ft³. With `MIN_SLOPE 0.5` in the input file, C1 runs at 0.005 and nothing floods. With the same 0.5 set through the API after the input is read, C1 stays at 0.0025 and J1 floods 862 ft³. (In 5.3.0 the stored value is 50 % rather than 0.5 %, but that is never applied either.)

## Why it happens

The option is read in percent and converted once:

```c
// src/legacy/engine/project.c, project_readOption()
      case MIN_SLOPE:
        if ( !getDouble(s2, &MinSlope) )
            return error_setInpError(ERR_NUMBER, s2);
        if ( MinSlope < 0.0 || MinSlope >= 100 )
            return error_setInpError(ERR_NUMBER, s2);
        MinSlope /= 100.0;
```

The toolkit copies the fraction out and the caller's number in:

```c
// src/legacy/engine/swmm5.c, getSystemValue()
    case swmm_MINSLOPE:
        return MinSlope;
...
// setSystemValue()
    case swmm_MINSLOPE:
        if (value >= 0 && value < 100)
        {
            MinSlope = value;
            return 0;
        }
```

`MinSlope` is used in one place, `conduit_getSlope()`, called from `conduit_validate()` during `swmm_open()`. Nothing recomputes the conduit slopes when it changes. 6.0.0 keeps the option in percent and divides by 100 in the resolver's slope pass, which also runs only during `swmm_engine_open()`.

## How to reproduce

| File | What it is |
|---|---|
| [`API-11_base.inp`](API-11_base.inp) | J1 to J2 to O1 through C1 (slope 0.0025) and C2 (0.005), kinematic wave, inflow at J1 peaking at 6.5 cfs; no `MIN_SLOPE` |
| [`API-11_minslope.inp`](API-11_minslope.inp) | The same with `MIN_SLOPE 0.5` |
| [`API-11_test.c`](API-11_test.c) | Legacy API: reads `swmm_MINSLOPE` from the deck with `MIN_SLOPE 0.5`; sets 0.5 on the base deck before `swmm_start()` and reads it back; compares C1's `LINK_SLOPE` and J1's flooded volume with the deck that has the option (5.3.0 only, `#ifdef`; 5.2.4 prints PASS because the property does not exist) |
| [`API-11_test6.c`](API-11_test6.c) | The same through `swmm_options_get/_set("MIN_SLOPE")` and `swmm_link_get_slope()` before `swmm_engine_initialize()` |
| [`API-11_swmm530.patch`](API-11_swmm530.patch), [`API-11_swmm600.patch`](API-11_swmm600.patch) | The fixes (apply after API-10's) |

```sh
tools/run-test.sh API-11            # 5.2.4 PASS, 5.3.0 FAIL, 6.0.0 FAIL
tools/run-test.sh API-11 --patched  # 5.3.0 PASS, 6.0.0 PASS (API-10 is applied first)
```

**Without the fix**, 5.3.0 fails the getter and effect checks:

```
                                     read      C1 slope   J1 flooded (ft3)
MIN_SLOPE 0.5 in [OPTIONS]           0.005     0.005000          0
no MIN_SLOPE, API sets 0.5 (rc 0)       0.5     0.002500        862
  (read before the set: 0)
  getter: MIN_SLOPE 0.5 from the input file reads 0.005, not 0.5
  effect: set before swmm_start, C1 slope 0.002500 and J1 flooding 862 ft3; from the input file 0.005000 and 0 ft3
FAIL: swmm_MINSLOPE is wrong in 2 of 3 checks (read 0.005 for MIN_SLOPE 0.5; set 0.5 leaves C1 at slope 0.002500 instead of 0.005000)
API-11 5.3.0 base: FAIL
```

6.0.0 reads and writes percent but does not apply the value:

```
MIN_SLOPE 0.5 in [OPTIONS]             0.5     0.005000          0
no MIN_SLOPE, API sets 0.5 (rc 0)       0.5     0.002500        862
  (read before the set: 0)
  effect: set before initialize, C1 slope 0.002500 and J1 flooding 862 ft3; from the input file 0.005000 and 0 ft3
FAIL: MIN_SLOPE is wrong in 1 of 3 checks (set 0.5 leaves C1 at slope 0.002500 instead of 0.005000)
API-11 6.0.0 base: FAIL
```

**With the fix**, both engines print the same:

```
                                     read      C1 slope   J1 flooded (ft3)
MIN_SLOPE 0.5 in [OPTIONS]             0.5     0.005000          0
no MIN_SLOPE, API sets 0.5 (rc 0)       0.5     0.005000          0
  (read before the set: 0)
PASS: swmm_MINSLOPE reads and writes percent like [OPTIONS], and a value set before swmm_start() changes the conduit slopes as the input file does
API-11 5.3.0 patched: PASS
```

## The fix

5.3.0 reads and writes percent, the unit of `[OPTIONS]` and of the setter's own range check, and recomputes every conduit's slope with `link_updateSlope()`, the function [API-10](../API-10-invert-offset-setters-stale-slope/) splits out of `conduit_validate()`:

```diff
     case swmm_MINSLOPE:
-        return MinSlope;
+        return MinSlope * 100.0;
 ...
     case swmm_MINSLOPE:
         if (value >= 0 && value < 100)
         {
-            MinSlope = value;
+            int j;
+
+            // --- value is a percent, as in [OPTIONS]; update the conduit
+            //     slopes computed with the old value at swmm_open
+            MinSlope = value / 100.0;
+            for (j = 0; j < Nobjects[LINK]; j++)
+            {
+                if (Link[j].type == CONDUIT) link_updateSlope(j);
+            }
             return 0;
         }
```

6.0.0 calls API-10's `recompute_conduit_slope()` for every conduit when `MIN_SLOPE` is set in the OPENED state. In the BUILDING state the resolver has not run yet and applies the option itself. After `swmm_engine_initialize()` the slopes are not recomputed, because lengthening has already been applied from them; the option is accepted there but, as before, has no effect until the model is opened again.

A 5.3.0 caller that relied on the getter returning a fraction will now get 100 times the value it got before; that is the unit the setter and the input file already used. Input files run without the API are unaffected.
