# NUM-57: 5.2.4 builds transect hydraulic radii with 1.49 instead of 1.486

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | In 5.2.4 every irregular (transect) and street cross-section has a hydraulic radius 0.40 % too small at every depth, so its conveyance is 0.27 % low in kinematic and dynamic wave. Small, but systematic, and it is part of any difference between 5.2.4 and 5.3.0 results for such models. No warning. |
| **Reached from** | Any `IRREGULAR` conduit (`[TRANSECTS]`) or `STREET` conduit (`[STREETS]`) |
| **5.3.0** | Not affected: `getGeometry()` uses `PHI` ([`src/legacy/engine/transect.c:492`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L492)) |
| **5.2.4** | `getGeometry()` in [`src/solver/transect.c:463`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/transect.c#L463) divides by 1.49; still so on EPA's `develop` branch |
| **6.0.0** | Not affected: `transect::buildTables()` uses `PHI` ([`src/engine/hydraulics/Transect.cpp:186`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Transect.cpp#L186)) |
| **Since** | 5.0.022 or earlier (in the first commit of the EPA repository, 2014); fixed for 5.3.0 by fork commit [e3309b63](https://github.com/HydroCouple/Stormwater-Management-Model/commit/e3309b639a870bb29bd1a9e253360e096ee0be03) (#70, January 2024) |
| **Fix** | None needed for 5.3.0 and 6.0.0; for 5.2.4, replace `1.49` by `PHI` |

## The problem

SWMM computes the conveyance of a transect by splitting it into sub-sections (overbanks, channel, and segments separated by high points), adding up their Manning flows `PHI / n * a * pow(a/wp, 2./3.)` with `PHI` = 1.486, and then back-calculating the single hydraulic radius that reproduces that total with the main channel's n. 5.2.4 back-calculates with 1.49:

```c
// src/solver/transect.c (5.2.4), getGeometry()
        transect->hradTbl[i] = pow(qSum * Nchannel / 1.49 / aSum, 1.5);
```

So every entry of the hydraulic-radius table is (1.486/1.49)^1.5 = 0.9960 of the value that reproduces the sub-section flows, and the conveyance A R^(2/3) / n that the routing uses is 0.27 % below their sum. Street cross-sections are built by the same function and have the same bias.

For a plain rectangle 20 ft wide and 5 ft deep entered as a transect with one n, the hydraulic radius is simply A/P = 100/30 = 3.333 ft and the full flow is that of Manning's equation with 1.486. 5.2.4 gives 3.320 ft and 0.27 % less flow; 5.3.0 and 6.0.0 give the exact values.

## Why it happens

1.49 is the rounded Manning constant, written out locally in a few 5.2.4 source files (`transect.c`, `lid.c` and, as `MCOEFF`, `subcatch.c`), while the transect's sub-section flows use `PHI` from `consts.h`. 5.3.0 replaced the local constants with the ones in `consts.h`, as its change log in `transect.c` notes ("Modified to use global constants defined in consts.h"):

```c
// src/legacy/engine/transect.c (5.3.0), getGeometry()
        transect->hradTbl[i] = pow(qSum * Nchannel / PHI / aSum, 1.5);
```

6.0.0 ported the 5.3.0 version.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-57_rect-transect.inp`](NUM-57_rect-transect.inp) | One 1000 ft conduit at slope 0.001 with a rectangular transect, 20 ft x 5 ft, n = 0.03 |
| [`NUM-57_test.c`](NUM-57_test.c) | Legacy toolkit (5.2.4, 5.3.0): compares the conduit's full flow with Manning's equation and backs out the full hydraulic radius (tolerance 0.05 %; the toolkit returns the full flow in double precision) |
| [`NUM-57_test6.c`](NUM-57_test6.c) | 6.0.0: reads Afull and Rfull of the link's section (`swmm_link_create_xsect()`, `swmm_xsect_full_properties()`) and compares Rfull with A/P |

```sh
tools/run-test.sh NUM-57   # 5.2.4: FAIL; 5.3.0, 6.0.0: PASS
```

5.2.4:

```
                          engine      exact
Full flow (cfs)          348.5909   349.5292   (-0.268 %)
Full hyd. radius (ft)      3.3199     3.3333   (-0.402 %)
FAIL: full flow 348.5909 cfs is -0.27 % from Manning's equation (349.5292 cfs); hydraulic radius 3.3199 ft instead of 3.3333 ft
NUM-57 5.2.4 base: FAIL
```

5.3.0 and 6.0.0:

```
Full flow (cfs)          349.5292   349.5292   (-0.000 %)
Full hyd. radius (ft)      3.3333     3.3333   (-0.000 %)
PASS: the transect's full flow and hydraulic radius follow Manning's equation with 1.486
NUM-57 5.3.0 base: PASS
...
Full hyd. radius (ft)      3.3333     3.3333   (-0.000 %)
PASS: the transect's full hydraulic radius is A/P
NUM-57 6.0.0 base: PASS
```

## The fix

5.3.0 and 6.0.0 already use `PHI`, so there are no patches. For 5.2.4 (or EPA's `develop` branch, which still has 1.49) the change is the one in fork commit e3309b63:

```diff
-        transect->hradTbl[i] = pow(qSum * Nchannel / 1.49 / aSum, 1.5);
+        transect->hradTbl[i] = pow(qSum * Nchannel / PHI / aSum, 1.5);
```

The same 0.27 % appears in the 5.2.4 numbers of the other transect issues: [NUM-22](../NUM-22-transect-nc-inherits-meander-adjusted-n/) (536.92 vs 538.37 cfs), [NUM-23](../NUM-23-transect-bank-stations-float-compare/) (109.55 vs 109.85 m³/s) and [NUM-24](../NUM-24-transect-right-wall-perimeter/) (126.51 vs 126.85 cfs).
