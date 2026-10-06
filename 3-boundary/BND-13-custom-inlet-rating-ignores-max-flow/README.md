# BND-13: Custom inlets with a RATING curve ignore the Qmax capture limit

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | A capture limit set in `[INLET_USAGE]` (for example to represent a lead pipe of limited capacity) is silently ignored for a custom inlet whose capture is given as a function of depth. In the test, an inlet limited to 0.5 cfs captures 2.15 cfs of a 3 cfs street flow, 4.3 times the limit, and the sewer receives that flow. Every other inlet type, including custom inlets with a DIVERSION curve, honours the limit. No warning. |
| **Reached from** | `[INLETS] name CUSTOM curve` where the curve is of type RATING, used in `[INLET_USAGE]` with Qmax > 0 |
| **5.3.0** | `getCustomCapturedFlow()` in [`src/legacy/engine/inlet.c:1953`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inlet.c#L1953) |
| **5.2.4** | Same code, [`src/solver/inlet.c:1953`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/inlet.c#L1953) |
| **6.0.0** | Reproduces: `getCustomCapturedFlow()` in [`src/engine/hydraulics/Inlet.cpp:558`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Inlet.cpp#L558) is a direct port |
| **Since** | 5.2.0, when inlets were added |
| **Fix** | Limit the rating capture to the number of inlets times Qmax: [`BND-13_swmm530.patch`](BND-13_swmm530.patch), [`BND-13_swmm600.patch`](BND-13_swmm600.patch) |

## The problem

The input reference describes the `[INLET_USAGE]` parameter Qmax as the "maximum flow that the inlet can capture (flow units)" and adds that "a Qmax value of 0 indicates that the inlet has no flow restriction". It makes no exception for any inlet type.

A custom inlet takes its capture from a curve. A DIVERSION curve gives captured flow against approach flow; a RATING curve gives captured flow against the water depth at the inlet. SWMM applies Qmax to standard inlets on grade and on sag and to DIVERSION curves, but not to RATING curves.

The test deck has a one-sided street carrying 3 cfs to a custom inlet with a rating curve of 10 cfs per ft of depth and Qmax = 0.5 cfs. At the street depth of 0.215 ft the curve gives 2.15 cfs, and all three engines capture 2.15 cfs. The correct capture is 0.5 cfs.

## Why it happens

`getCustomCapturedFlow()` sets up the per-inlet limit at the top and applies it to each replicate inlet in the DIVERSION branch, but the RATING branch does not use it:

```c
// src/legacy/engine/inlet.c, getCustomCapturedFlow()
qMax = BIG;
if (inlet->flowLimit > 0.0) qMax = inlet->flowLimit;
...
if (Curve[c].curveType == DIVERSION_CURVE)
{
    for (j = 1; j <= inlet->numInlets; j++)
    {
        qIncrement = inlet->clogFactor *
            table_lookupEx(&Curve[c], qBypassed * UCF(FLOW)) / UCF(FLOW);
        qIncrement = MIN(qIncrement, qMax);           // limit applied
        ...
    }
}
else if (Curve[c].curveType == RATING_CURVE)
{
    qCaptured = inlet->numInlets * inlet->clogFactor *
        table_lookupEx(&Curve[c], d * UCF(LENGTH)) / UCF(FLOW);   // no limit
}
qCaptured *= sides;
```

The on-sag standard inlets, which are also driven by depth, apply the limit per inlet before multiplying by the number of inlets (`getOnSagCapturedFlow()`, inlet.c:1602).

## How to reproduce

| File | What it is |
|---|---|
| [`BND-13_rating-inlet-maxflow.inp`](BND-13_rating-inlet-maxflow.inp) | One-sided street with 3 cfs and a custom RATING inlet limited to Qmax = 0.5 cfs |
| [`BND-13_test.c`](BND-13_test.c) | Legacy toolkit (5.2.4, 5.3.0). At steady state, measures the capture as the drop in street flow across the inlet and checks that it equals Qmax |
| [`BND-13_test6.c`](BND-13_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh BND-13            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-13 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints 0.2146 ft and 2.146 cfs; 6.0.0 prints the 5.3.0 values):

```
End of run:
  depth at inlet node B       0.2145 ft (rating curve: 2.14 cfs)
  street flow S1 / S2          3.000 / 0.855 cfs
  captured flow                2.145 cfs
  Qmax in [INLET_USAGE]        0.500 cfs
FAIL: the rating-curve inlet captures 2.145 cfs, more than its Qmax of 0.500 cfs
BND-13 5.2.4 base: FAIL
BND-13 5.3.0 base: FAIL
BND-13 6.0.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same values):

```
End of run:
  depth at inlet node B       0.2924 ft (rating curve: 2.92 cfs)
  street flow S1 / S2          3.000 / 2.500 cfs
  captured flow                0.500 cfs
  Qmax in [INLET_USAGE]        0.500 cfs
PASS: the rating-curve inlet captures its Qmax of 0.500 cfs (0.500 cfs)
BND-13 5.3.0 patched: PASS
BND-13 6.0.0 patched: PASS
```

The street runs deeper with the fix (0.29 ft instead of 0.21 ft) because 2.5 cfs instead of 0.86 cfs now bypasses the inlet.

## The fix

```diff
         else if (Curve[c].curveType == RATING_CURVE)
         {
             qCaptured = inlet->numInlets * inlet->clogFactor *
                 table_lookupEx(&Curve[c], d * UCF(LENGTH)) / UCF(FLOW);
+            qCaptured = MIN(qCaptured, inlet->numInlets * qMax);
         }
```

Since every replicate inlet sees the same depth, limiting the total to `numInlets * qMax` is the same as limiting each inlet to `qMax`, as `getOnSagCapturedFlow()` does. The 6.0.0 patch adds the same line to `Inlet.cpp`.

**Effect on other models.** Only custom RATING inlets with Qmax > 0 change. With Qmax = 0, `qMax` is `BIG` and the added line returns its input unchanged. No deck in the regression suite sets Qmax, so none changes.

The review pass also noted that a RATING capture is not limited by the approach flow when the inlet is on grade. That is how the standard on-sag inlets behave too (capture driven by depth, the node drains), and this patch does not change it.
