# API-01: Forcing values set before swmm_start are accepted, then wiped by swmm_start

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | A script that sets a gage rainfall, a node lateral inflow or a link setting (in 5.3.0 also a subcatchment's API rainfall, snowfall or PET) between `swmm_open` and `swmm_start` gets return code 0 and a run that ignores the value: 1 in/hr of rain on a dry gage gives 0 in/hr, a 2 cfs inflow gives 0 cfs, an orifice set to 0.25 runs fully open. Nothing warns. |
| **Reached from** | `swmm_setValue(swmm_GAGE_RAINFALL / swmm_NODE_LATFLOW / swmm_LINK_SETTING, ...)` and, in 5.3.0, `swmm_setValueExpanded` with those properties or `swmm_SUBCATCH_API_RAINFALL / _API_SNOWFALL / _API_PET`, called before `swmm_start` |
| **5.3.0** | `swmm_setValue()` in [`src/legacy/engine/swmm5.c:1396`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L1396), and the pre-start branches of `setGageValue()`, `setSubcatchValue()`, `setNodeValue()` and `setLinkValue()` ([`swmm5.c:1991`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L1991), [`:2202`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2202), [`:2328`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2328), [`:2407`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2407)); the resets are in [`gage.c:293`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L293), [`node.c:264`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L264), [`link.c:527`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L527) and [`subcatch.c:450`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L450) |
| **5.2.4** | Same for the three `swmm_setValue` properties: [`src/solver/swmm5.c:879`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L879), reset in [`gage.c:279`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gage.c#L279), [`node.c:265`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L265), [`link.c:524`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L524). `swmm_setValue` returns `void` there, so the caller cannot be told. |
| **6.0.0** | Not affected: every runtime setter and forcing function (`swmm_gage_set_rainfall`, `swmm_node_set_lateral_inflow`, `swmm_link_set_target_setting`, `swmm_forcing_*`, ...) checks `CHECK_RUNNING` and returns `SWMM_ERR_LIFECYCLE` before `swmm_engine_start` ([`openswmm_forcing_impl.cpp:391`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/openswmm_forcing_impl.cpp#L391)) |
| **Since** | 5.2.0, which added `swmm_setValue`; 5.3.0's expanded setters added the subcatchment properties with the same flaw |
| **Fix** | Return `ERR_API_NOT_STARTED` for these properties until the run has started: [`API-01_swmm530.patch`](API-01_swmm530.patch) |

## The problem

A coupling script or a scenario driver that wants the run to start with a given rainfall, a given inflow or a gate position naturally sets the value after opening the project and before starting it:

```c
swmm_open("model.inp", "model.rpt", "model.out");
swmm_setValue(swmm_GAGE_RAINFALL, g1, 1.0);      /* 5.3.0 returns 0 */
swmm_setValue(swmm_LINK_SETTING, or1, 0.25);     /* 5.3.0 returns 0 */
swmm_start(1);
```

Both calls succeed, and the run is identical to one without them. In the test deck below (a dry gage, one subcatchment, an orifice to a free outfall), each value is set on its own before `swmm_start` and the run is then watched step by step: S1 sees 0 in/hr of rain instead of 1, J2 receives 0 cfs instead of 2, OR1 stays at setting 1.0 instead of 0.25, and in 5.3.0 the subcatchment API PET reads back as -1 ("not set") instead of 0.2 in/day. The same calls made right after `swmm_start` (before the first `swmm_step`) are used by the run, which is what makes the pre-start case easy to miss.

5.3.0 makes this look deliberate: `setSubcatchValue()`, `setNodeValue()` and `setLinkValue()` each have a branch "Set values that can only be configured before simulation starts", and that branch accepts `swmm_SUBCATCH_API_RAINFALL`, `_API_SNOWFALL`, `_API_PET`, `swmm_NODE_LATFLOW` and `swmm_LINK_SETTING` and returns 0.

## Why it happens

The setters write straight into the object's state, with no check of `IsStartedFlag`:

```c
// src/legacy/engine/swmm5.c, swmm_setValue()
case swmm_GAGE_RAINFALL:
    if (index < 0 || index >= Nobjects[GAGE])
        return 0;
    if (value >= 0.0)
        Gage[index].apiRainfall = value;
    return 0;
...
case swmm_LINK_SETTING:
    setLinkSetting(index, value);
    return 0;
```

`swmm_start()` then calls `project_init()`, which runs every object's `*_initState()`, and those put the API fields back to their "not set" values:

```c
// src/legacy/engine/gage.c, gage_initState()
Gage[j].apiRainfall = MISSING;
// src/legacy/engine/node.c, node_initState()
Node[nodeIndex].apiExtInflow = 0.0;
// src/legacy/engine/link.c, link_initState()
Link[j].setting   = 1.0;
Link[j].targetSetting = 1.0;
// src/legacy/engine/subcatch.c, subcatch_initState()
Subcatch[subcatchIndex].apiRainfall = 0.0;
Subcatch[subcatchIndex].apiSnowfall = 0.0;
Subcatch[subcatchIndex].apiEvapRate = MISSING;
```

The resets are needed (they clear values left over from a previous `swmm_start`/`swmm_end` cycle in the same session), so the pre-start write can never survive. The node and link pollutant mass fluxes (`swmm_NODE_POLLUTANT_LATMASS_FLUX`, `swmm_LINK_POLLUTANT_LATMASS_FLUX`) are zeroed the same way by `qualrout_init()` ([`qualrout.c:82`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L82), [`:96`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L96)); their pre-start branch also crashes or writes out of bounds, which is [CRASH-13](../../5-crashes/CRASH-13-pollutant-flux-before-start/), so they are not repeated here.

## How to reproduce

| File | What it is |
|---|---|
| [`API-01_forcing.inp`](API-01_forcing.inp) | Gage G1 with an all-zero series, subcatchment S1 to J1, conduit C1 to J2, side orifice OR1 to free outfall O1; DYNWAVE, 2 hours |
| [`API-01_test.c`](API-01_test.c) | For each property: open, set the value, start, record the largest value the engine reports during the run. 3 cases on 5.2.4, 9 on 5.3.0 |
| [`API-01_test6.c`](API-01_test6.c) | The same through 6.0.0's runtime setters and forcing API |

A case passes if the call is refused with an error code, or if the run reports the value (within 10%; the reset values are 0, 1 and -1).

```sh
tools/run-test.sh API-01            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh API-01 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0:

```
value set before swmm_start                     set        rc  what the run reports                   verdict
swmm_setValue GAGE_RAINFALL(G1)                1.00         0  max S1 rainfall (in/hr)         0.000  ACCEPTED BUT IGNORED
swmm_setValue NODE_LATFLOW(J2)                 2.00         0  max J2 lateral inflow (cfs)     0.000  ACCEPTED BUT IGNORED
swmm_setValue LINK_SETTING(OR1)                0.25         0  max OR1 setting                 1.000  ACCEPTED BUT IGNORED
setValueExpanded GAGE_RAINFALL(G1)             1.00         0  max S1 rainfall (in/hr)         0.000  ACCEPTED BUT IGNORED
setValueExpanded NODE_LATFLOW(J2)              2.00         0  max J2 lateral inflow (cfs)     0.000  ACCEPTED BUT IGNORED
setValueExpanded LINK_SETTING(OR1)             0.25         0  max OR1 setting                 1.000  ACCEPTED BUT IGNORED
setValueExpanded SUBCATCH_API_RAINFALL(S1)     1.00         0  max S1 rainfall (in/hr)         0.000  ACCEPTED BUT IGNORED
setValueExpanded SUBCATCH_API_SNOWFALL(S1)     0.50         0  max S1 API snowfall (in/hr)     0.000  ACCEPTED BUT IGNORED
setValueExpanded SUBCATCH_API_PET(S1)          0.20         0  max S1 API PET (in/day)        -1.000  ACCEPTED BUT IGNORED
FAIL: 9 of 9 values set before swmm_start were accepted (rc 0) and then reset by swmm_start
API-01 5.3.0 base: FAIL
```

5.2.4 gives the same numbers for its three `swmm_setValue` cases (its `swmm_setValue` has no return value; the test prints 0):

```
swmm_setValue GAGE_RAINFALL(G1)                1.00         0  max S1 rainfall (in/hr)         0.000  ACCEPTED BUT IGNORED
swmm_setValue NODE_LATFLOW(J2)                 2.00         0  max J2 lateral inflow (cfs)     0.000  ACCEPTED BUT IGNORED
swmm_setValue LINK_SETTING(OR1)                0.25         0  max OR1 setting                 1.000  ACCEPTED BUT IGNORED
FAIL: 3 of 3 values set before swmm_start were accepted (rc 0) and then reset by swmm_start
API-01 5.2.4 base: FAIL
```

6.0.0 refuses every one of them with `SWMM_ERR_LIFECYCLE` (6):

```
swmm_gage_set_rainfall(G1)             1.00    6  max S1 rainfall (in/hr)         0.000  refused
swmm_forcing_gage_rainfall(G1)         1.00    6  max S1 rainfall (in/hr)         0.000  refused
swmm_node_set_lateral_inflow(J2)       2.00    6  max J2 lateral inflow (cfs)     0.000  refused
...
swmm_forcing_subcatch_rainfall(S1)     1.00    6  max S1 rainfall (in/hr)         0.000  refused
PASS: every value set before swmm_engine_start is either used by the run or refused with an error code
API-01 6.0.0 base: PASS
```

**With the fix**, 5.3.0 refuses the calls with `ERR_API_NOT_STARTED` (-999902):

```
swmm_setValue GAGE_RAINFALL(G1)                1.00   -999902  max S1 rainfall (in/hr)         0.000  refused
swmm_setValue NODE_LATFLOW(J2)                 2.00   -999902  max J2 lateral inflow (cfs)     0.000  refused
swmm_setValue LINK_SETTING(OR1)                0.25   -999902  max OR1 setting                 1.000  refused
...
setValueExpanded SUBCATCH_API_PET(S1)          0.20   -999902  max S1 API PET (in/day)        -1.000  refused
PASS: every value set before swmm_start is either used by the run or refused with an error code
API-01 5.3.0 patched: PASS
```

## The fix

The reset at `swmm_start` is correct, so the setter has to stop pretending. Each of these properties now returns `ERR_API_NOT_STARTED` before the run starts, the same rule 6.0.0 applies:

```diff
     case swmm_GAGE_RAINFALL:
+        if (!IsStartedFlag)
+            return ERR_API_NOT_STARTED;
         if (index < 0 || index >= Nobjects[GAGE])
             return 0;
```

and in the pre-start branches of the expanded setters:

```diff
         case swmm_SUBCATCH_API_RAINFALL:
-            if (value >= 0.0)
-            {
-                Subcatch[index].apiRainfall = value / UCF(RAINFALL);
-                return 0;
-            }
-            else
-                return ERR_API_PROPERTY_VALUE;
         case swmm_SUBCATCH_API_SNOWFALL:
 ...
         case swmm_SUBCATCH_API_PET:
-            ...
-            return 0;
+            // --- swmm_start resets these (subcatch_initState)
+            return ERR_API_NOT_STARTED;
```

Callers that want a value from the first time step set it after `swmm_start` and before the first `swmm_step`; that worked before and is unchanged (checked with the same test, calls moved after `swmm_start`: every case reports the value set). Runs driven only by an input file never call these setters, so no model result changes. The alternative, keeping pre-start values through `swmm_start`, would need the `*_initState()` resets moved to `swmm_end` and new initialisation at `swmm_open` (the fields are zero, not `MISSING`, until the first `swmm_start`), in five source files.
