# IO-01: "nan" and "inf" are accepted as numbers in the input file

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A `nan` or `inf` typed (or written by a script) into a numeric field is read as a number, with no error. In 5.2.4 and 5.3.0 the run then completes with NaN results, and in three of the four test decks the flow routing continuity error is printed as `0.000` or `nan`, so the report looks clean or is plainly broken without pointing at the input line. 6.0.0 either stops at the first step with "routing solution diverged" or completes with NaN outflow and a 0.000 continuity error. |
| **Reached from** | Any numeric field read with `getDouble()` (nearly all of them: [CONDUITS], [JUNCTIONS], [TIMESERIES], [OPTIONS] steps, ...), and every clock time read with `datetime_strToTime()` |
| **5.3.0** | `getDouble()` in [`src/legacy/engine/input.c:857`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L857-L863) and `datetime_strToTime()` in [`src/legacy/engine/datetime.c:351`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/datetime.c#L351-L356); NaN then passes checks such as [`project.c:694`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L694) and [`link.c:1045`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1045) |
| **5.2.4** | Same code: [`src/solver/input.c:866`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/input.c#L866), [`src/solver/datetime.c:351`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/datetime.c#L351) |
| **6.0.0** | Reproduces: `from_chars_double()` in [`src/engine/core/charconv_compat.hpp:74`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/charconv_compat.hpp#L74) is `std::from_chars`, which also reads `nan` and `inf`, and checks such as `step <= 0.0` ([`OptionsHandler.cpp:271`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/OptionsHandler.cpp#L271)) let NaN through |
| **Since** | Every release: `getDouble()` has always been a bare `strtod()`, and C99 runtimes (glibc, macOS, the Windows UCRT since Visual Studio 2015) parse `nan`/`inf` |
| **Fix** | Treat a non-finite result as "not a number": [`IO-01_swmm530.patch`](IO-01_swmm530.patch), [`IO-01_swmm600.patch`](IO-01_swmm600.patch) |

## The problem

SWMM rejects a field that is not a number with ERROR 211 ("invalid number"): a roughness of `abc` stops the run at input time and the report names the line. A roughness of `nan`, a maximum depth of `inf` or a time-series value of `nan` are accepted instead, because the C library reads them as numbers. Nothing downstream catches them. The results are NaN, and the report often hides it.

The test runs one small network (J1 → C1 → J2 → C2 → outfall O1, 5 cfs inflow at J1) with one bad token per deck:

| Deck | Bad token | 5.2.4 and 5.3.0 | 6.0.0 |
|---|---|---|---|
| `IO-01_nan-roughness.inp` | C1 Manning n `nan` | Runs; flows and volumes are NaN, continuity error `nan` | Stops at step 1: ERROR 14 "routing solution diverged at node 'J1' (head nan)" |
| `IO-01_inf-depth.inp` | J2 max depth `inf` | Runs; initial and final stored volume `-nan`, continuity error printed `0.000` (true value -0.216 %) | Runs, results as for a deep junction |
| `IO-01_nan-series.inp` | inflow series value `nan` (kinematic wave) | Stops at step 1: ERROR 103 "cannot solve KW equations for Link C1" | Runs; external inflow 0.000, external outflow `nan`, continuity error printed `0.000` |
| `IO-01_nan-routing-step.inp` | `ROUTING_STEP nan` | Runs; every flow routing continuity term is `nan`, "Routing Time Step ... nan sec", continuity error printed `0.000` | Stops at step 1: ERROR 14 |

None of these reports names the input line, and none says the input was invalid. The `0.000` comes from `massbal_getFlowError()`, whose tests `fabs(in - out) < 1.0` and `fabs(in) > 0.0` are both false for NaN, so the percentage keeps its initial value of zero ([`massbal.c:850-858`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L850-L858)).

A wider sweep in the review replaced each numeric field of the first data row of every section with `nan` in six decks (Example1, Storage_Shape_Test, Type5_Pump_Test, extran1, bioretention, swc21; 197 fields). 5.2.4 and 5.3.0 rejected 39 of them, crashed on 7 (the cross-section look-up crash reported separately), wrote NaN to the .out file for 58 (5.2.4) or 60 (5.3.0), and gave finite results for the rest. 6.0.0 rejected `nan` in [XSECTIONS] but accepted it in [EVAPORATION], [SUBCATCHMENTS], [TIMESERIES], [LID_USAGE] and [POLLUTANTS] and wrote NaN results.

## Why it happens

Every numeric field goes through `getDouble()`, which only asks whether `strtod()` used the whole token:

```c
// src/legacy/engine/input.c, getDouble()
    *y = strtod(s, &endptr);
    if (*endptr > 0) return(0);
    return(1);
```

Since C99, `strtod()` reads `nan`, `inf` and `infinity` in any case, and returns `HUGE_VAL` (infinity) for an overflowing value such as `1e400`. All of them consume the whole token, so `getDouble()` reports success. The checks that follow are written as "reject if bad", and every ordered comparison with NaN is false:

```c
// src/legacy/engine/project.c, project_readOption(), ROUTE_STEP
            if ( tStep <= 0.0 ) return error_setInpError(ERR_NUMBER, s2);
// src/legacy/engine/link.c, conduit_validate()
    if ( Conduit[k].roughness <= 0.0 )
        report_writeErrorMsg(ERR_ROUGHNESS, Link[j].ID);
```

`datetime_strToTime()` reads a clock time as decimal hours first, with the same `strtod()` test, so `nan` is also a valid time. `ROUTING_STEP` and `LENGTHENING_STEP` fall back to it when `getDouble()` fails, so fixing `getDouble()` alone would send `ROUTING_STEP nan` there instead.

`getInt()` is `getDouble()` plus a cast, so `nan` in an integer field reaches `(int)NaN`, which is undefined behaviour.

6.0.0 parses every number through `openswmm::from_chars_double()`, a wrapper around `std::from_chars`. `std::from_chars` follows `strtod()` and also accepts `nan`, `inf` and `infinity`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-01_valid.inp`](IO-01_valid.inp) | Control: J1 → C1 → J2 → C2 → O1, 5 cfs constant inflow at J1, dynamic wave, every value finite |
| [`IO-01_nan-roughness.inp`](IO-01_nan-roughness.inp) | The control deck with C1's roughness `nan` |
| [`IO-01_inf-depth.inp`](IO-01_inf-depth.inp) | The control deck with J2's maximum depth `inf` |
| [`IO-01_nan-series.inp`](IO-01_nan-series.inp) | Kinematic wave; the inflow comes from series TS1 (5, `nan`, 5 cfs at 0, 2 and 4 h) |
| [`IO-01_nan-routing-step.inp`](IO-01_nan-routing-step.inp) | The control deck with `ROUTING_STEP nan` |
| [`IO-01_test.c`](IO-01_test.c) | Opens each deck with the legacy toolkit and, if it opens, runs it, counting non-finite node depths, node volumes and link flows. Correct: the control deck runs with finite results, every other deck is refused by `swmm_open` |
| [`IO-01_test6.c`](IO-01_test6.c) | The same check with the 6.0.0 API |

```sh
tools/run-test.sh IO-01            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-01 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same table as 5.3.0):

```
---- IO-01 on 5.3.0 (base) ----
Deck                          open   step non-finite routing CE %
IO-01_valid.inp                  0      0          0       -0.216
IO-01_nan-roughness.inp          0      0      17281          nan  <-- accepted
IO-01_inf-depth.inp              0      0       4320        0.000  <-- accepted
IO-01_nan-series.inp             0    103          4        0.000  <-- accepted
IO-01_nan-routing-step.inp       0      0          0        0.000  <-- accepted
FAIL: 4 of 5 decks handled wrongly; 'nan'/'inf' in a numeric field is accepted as a number instead of raising ERROR 211
IO-01 5.3.0 base: FAIL
---- IO-01 on 6.0.0 (base) ----
Deck                          open    run non-finite routing CE %
IO-01_valid.inp                  0      0          0       -0.227
IO-01_nan-roughness.inp          0     14          0            -  <-- accepted
IO-01_inf-depth.inp              0      0          0       -0.227  <-- accepted
IO-01_nan-series.inp             0      0       4320          nan  <-- accepted
IO-01_nan-routing-step.inp       0     14          0            -  <-- accepted
FAIL: 4 of 5 decks handled wrongly; 'nan'/'inf' in a numeric field is accepted as a number instead of raising ERROR 211
IO-01 6.0.0 base: FAIL
```

"non-finite" counts NaN or infinite node depths, node volumes and link flows over all routing steps. In the routing-step deck the state values stay finite but every term of the flow routing continuity table is `nan`:

```
  External Inflow ..........           nan           nan
  External Outflow .........           nan           nan
  ...
  Final Stored Volume ......           nan           nan
  Continuity Error (%) .....         0.000
```

**With the fix** each bad deck is refused when it is opened (5.3.0 returns 200, 6.0.0 returns 5), and the report names the token:

```
---- IO-01 on 5.3.0 (patched) ----
Deck                          open   step non-finite routing CE %
IO-01_valid.inp                  0      0          0       -0.216
IO-01_nan-roughness.inp        200      0          0            -
IO-01_inf-depth.inp            200      0          0            -
IO-01_nan-series.inp           200      0          0            -
IO-01_nan-routing-step.inp     200      0          0            -
PASS: 'nan' and 'inf' in numeric fields are refused at swmm_open (input error), and the finite control deck runs with finite results
IO-01 5.3.0 patched: PASS
---- IO-01 on 6.0.0 (patched) ----
...
IO-01_nan-routing-step.inp       5      0          0            -
PASS: 'nan' and 'inf' in numeric fields are refused at swmm_engine_open (input error), and the finite control deck runs with finite results
IO-01 6.0.0 patched: PASS
```

```
  ERROR 211: invalid number nan at line 28 of [CONDUIT] section:
  C1   J1   J2   400   nan     0   0   0   0
  ERROR 211: invalid number inf at line 21 of [JUNC] section:
  J2    99   inf  0   0   0
  ERROR 211: invalid number nan at line 42 of [TIMESERIES] section:
  TS1  2:00  nan
```

## The fix

5.3.0: `getDouble()` and the decimal-hours branch of `datetime_strToTime()` reject a non-finite result, so the token goes down the same path as any other unreadable number:

```diff
     *y = strtod(s, &endptr);
     if (*endptr > 0) return(0);
+    if (!isfinite(*y)) return(0);       // strtod also reads "nan" and "inf"
     return(1);
```

```diff
     *t = strtod(s, &endptr);
     if ( *endptr == 0 )
     {
+        if ( !isfinite(*t) ) return 0;     // strtod also reads "nan", "inf"
         *t /= 24.0;
```

The time check returns at once rather than falling through to the `hr:min:sec` parser, which would read `1e400` as 1 hour. `getFloat()` has the same code but no callers, so it is left alone.

6.0.0: `from_chars_double()` returns `invalid_argument` for a non-finite result, on both its `std::from_chars` and its `strtod_l` paths. That stops `nan` and `inf` everywhere, but it is not enough on its own: most 6.0.0 section handlers read numbers with `to_double()`, which turns a token it cannot read into 0.0 without a message, where legacy reports ERROR 211. With only the parser change the roughness deck runs with n = 0 and the depth deck with a maximum depth of 0. The patch therefore also checks the fields the test uses with `parse_double_strict()` and reports ERROR 211, as legacy does: junction maximum, initial and surcharge depth and ponded area ([`NodesHandler.cpp:123`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/NodesHandler.cpp#L123)), conduit length and roughness ([`LinksHandler.cpp:115`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/LinksHandler.cpp#L115-L116)) and time series values ([`TablesHandler.cpp:185`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/TablesHandler.cpp#L185), [`:235`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/TablesHandler.cpp#L235)). The other `to_double()` fields (about 250) need the same treatment; that is a separate 6.0.0 defect (an unreadable number becomes 0 silently, also for `abc`), and [GROUNDWATER] shows the intended pattern already.

**Effect on other models.** Only input that holds `nan`, `inf`, `infinity` or an overflowing number is affected; finite values are parsed exactly as before. None of the 73 regression decks contains such a token. With the patched 5.3.0 and 6.0.0 engines all 73 decks gave the same exit codes, error messages and runoff and routing continuity errors as the unpatched engines (69 decks on both engines; the 4 longest only on 6.0.0, because 5.3.0 under the sanitizers did not finish them within the time limit, patched or not).

## Notes

- `START_DRY_DAYS` is read with `atof()`, not `getDouble()`, so it still accepts `nan` (and reads `abc` as 0). External climate and interface files are read with `atof()`/`sscanf()` as well and are not covered here.
- Huge but finite times and steps (`END_TIME 1e30`, `REPORT_STEP -1e10`) are [IO-02](../IO-02-time-options-accept-nan-inf-huge/).
