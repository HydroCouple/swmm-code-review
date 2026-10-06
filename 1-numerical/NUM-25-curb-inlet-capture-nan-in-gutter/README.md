# NUM-25: Curb opening inlets capture all of the flow while the spread is inside a depressed gutter

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | On-grade curb opening, slotted and combination (sweeper) inlets in a street with a depressed gutter capture 100% of any flow narrow enough to stay inside the gutter. HEC-22 gives 44-56% for the test's 1-ft opening at 0.05-0.10 cfs. Bypass flow to downstream inlets is under-predicted on every rising and falling limb. No warning. |
| **Reached from** | `[STREETS]` with a gutter depression `a > 0` (or `[INLET_USAGE]` local depression), an on-grade CURB or SLOTTED inlet, or a GRATE + CURB inlet whose curb opening is longer than the grate |
| **5.3.0** | `getCurbInletCapture()` in [`src/legacy/engine/inlet.c:1497`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inlet.c#L1497) |
| **5.2.4** | Same code, [`src/solver/inlet.c:1497`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/inlet.c#L1497) |
| **6.0.0** | Reproduces: `getCurbInletCapture()` in [`src/engine/hydraulics/Inlet.cpp:300`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Inlet.cpp#L300) is a direct port |
| **Since** | 5.2.0, when inlets were added |
| **Fix** | Use Eo = 1 when the spread is within the gutter: [`NUM-25_swmm530.patch`](NUM-25_swmm530.patch), [`NUM-25_swmm600.patch`](NUM-25_swmm600.patch) |

## The problem

For an on-grade curb opening, SWMM follows HEC-22: the opening length needed to capture all of the flow is

    Lt = 0.6 Q^0.42 SL^0.3 (1 / (n Se))^0.6          (HEC-22 Eq. 4-22a)

and an opening of length L < Lt captures E = 1 - (1 - L/Lt)^1.8 of the flow. In a depressed gutter the equivalent slope is Se = Sx + (a/W) Eo, where Eo is the fraction of the flow carried in the gutter. When the spread T is no wider than the gutter, all of the flow is in the gutter: Eo = 1 and Se = Sw = Sx + a/W.

SWMM evaluates Eo with a formula that only holds for T > W. For T < W it returns NaN, `Lt` becomes NaN, the test `L < Lt` is false, and the efficiency stays at its initial value of 1. The inlet captures everything.

The test uses the HEC-22 example street (Sx = 2%, a = 0.0833 ft, W = 2 ft, n = 0.016) on a 1% grade with a 1-ft curb opening. At 0.05, 0.08 and 0.10 cfs the spread is 1.2 to 1.5 ft, inside the 2-ft gutter. All three engines capture 100%, where HEC-22 gives 56.1%, 47.6% and 43.9%. At 0.50 cfs the spread is 4.2 ft and SWMM agrees with HEC-22 (23.0%).

## Why it happens

```c
// src/legacy/engine/inlet.c, getCurbInletCapture()
double Se = Sx, Lt, Sr, Eo = 0.0, E = 1.0;
...
if (a > 0.0)
{
    Sr = Sw / Sx;
    Eo = getEo(Sr, T-W, W);                // T-W < 0 when the spread is inside the gutter
    Se = Sx + (a/W) * Eo;
}
Lt = 0.6 * pow(Q, 0.42) * pow(SL, 0.3) * pow(1.0/(n*Se), 0.6);
if (L < Lt) { ... E = 1 - pow(E, 1.8); }  // NaN comparison is false: E stays 1.0
```

`getEo()` solves HEC-22 Eq. 4-4 with Ts = T - W, the part of the spread outside the gutter:

```c
// src/legacy/engine/inlet.c, getEo()
x = Sr / (Ts / w);
x = pow((1.0 + x), 2.67) - 1.0;
```

For 0 < T < W, `Ts` is negative and `x = Sr W / (T - W)` is below -1 (since Sr > 1), so `pow()` gets a negative base with a fractional exponent and returns NaN. `getGutterFlowRatio()`, which the grate code uses, has the guard (`if (T <= w) return 1.0;`); `getCurbInletCapture()` does not. Slotted inlets and the sweeper part of combination inlets call the same function.

A related inconsistency, not changed here: `getFlowSpread()` decides whether the spread is within the gutter with the slope a/W instead of Sw = Sx + a/W (`f1 = f * pow((a / W) / Sx, 1.67)`, inlet.c:1221). For the test street it switches to the beyond-the-gutter branch at 0.110 cfs instead of 0.212 cfs. Near T = W, Eo is close to 1 in both branches, so the curb capture barely changes (the review pass measured 37.73% at 0.15 cfs, the HEC-22 value), but the spread reported for such flows is a little too wide.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-25_curb-inlet-low-flows.inp`](NUM-25_curb-inlet-low-flows.inp) | Four identical streets with a 1-ft on-grade curb opening, carrying 0.05, 0.08, 0.10 and 0.50 cfs |
| [`NUM-25_test.c`](NUM-25_test.c) | Legacy toolkit (5.2.4, 5.3.0). Measures each inlet's capture as the drop in street flow across it and compares it with HEC-22, computed in the test |
| [`NUM-25_test6.c`](NUM-25_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-25            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-25 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** all three engines print:

```
Street  Q (cfs)  bypass (cfs)  capture %  HEC-22 %
S1a       0.050        0.0000     100.00     56.10
S2a       0.080        0.0000     100.00     47.55
S3a       0.100        0.0000     100.00     43.85
S4a       0.500        0.3852      22.96     22.98
FAIL: 3 of 4 curb inlets differ from HEC-22 by more than 2 points (worst: 0.100 cfs captured at 100.00%, HEC-22 43.85%)
NUM-25 5.2.4 base: FAIL
NUM-25 5.3.0 base: FAIL
NUM-25 6.0.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same values):

```
Street  Q (cfs)  bypass (cfs)  capture %  HEC-22 %
S1a       0.050        0.0220      56.10     56.10
S2a       0.080        0.0420      47.55     47.55
S3a       0.100        0.0561      43.85     43.85
S4a       0.500        0.3852      22.96     22.98
PASS: curb inlet capture follows HEC-22 inside and beyond the depressed gutter (largest difference 0.02 points)
NUM-25 5.3.0 patched: PASS
NUM-25 6.0.0 patched: PASS
```

## The fix

```diff
     if (a > 0.0)
     {
         Sr = Sw / Sx;
-        Eo = getEo(Sr, T-W, W);
+        if (T <= W) Eo = 1.0;    // spread is within the depressed gutter
+        else        Eo = getEo(Sr, T-W, W);
         Se = Sx + (a/W) * Eo;                              //HEC-22 Eq(4-24)
     }
```

The 6.0.0 patch makes the same change in `Inlet.cpp`. Only flows whose spread stays inside the gutter change. The binary output files of all eleven inlet decks in the regression suite (`update_v52/*inlet*`, `Example7-Inlets`, `CoS-Reduced-*`) are identical before and after the patch in both engines: their constant inflows keep the spread wider than the gutter.
