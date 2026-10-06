# NUM-03: RECT_ROUND hydraulic radius in the round bottom has a misplaced parenthesis

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Wrong dynamic-wave friction in every RECT_ROUND conduit whose water surface is inside the round bottom. The hydraulic radius is too large near the invert, zero at 0.29 r and up to 100x too small above that. In the test, steady uniform flow runs 62 % deeper and 35 % slower than the normal depth. No warning; continuity is unaffected. |
| **Reached from** | `[XSECTIONS]` shape `RECT_ROUND` with `FLOW_ROUTING DYNWAVE`, at depths below the top of the round bottom (half the width for the default bottom radius) |
| **5.3.0** | `rect_round_getRofY()` in [`src/legacy/engine/xsect.c:2066`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L2066), called by `xsect_getRofY()` ([`xsect.c:1088`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L1088)) from `getHydRad()` in [`dwflow.c:741`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dwflow.c#L741) |
| **5.2.4** | Same code, [`src/solver/xsect.c:2062`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/xsect.c#L2062) |
| **6.0.0** | Reproduces: `XsectEval::rect_round_getRofY()` in [`src/engine/hydraulics/XSectKernels.hpp:788`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/XSectKernels.hpp#L788) is a direct port, with the same numbers |
| **Since** | 5.0.022 or earlier: the line is in the first commit of the EPA repository (2014) |
| **Fix** | Move one parenthesis: [`NUM-03_swmm530.patch`](NUM-03_swmm530.patch), [`NUM-03_swmm600.patch`](NUM-03_swmm600.patch) |

## The problem

A RECT_ROUND section is a rectangle on a circular-arc bottom of radius r. With the default bottom radius (Geom3 = 0, so r = W/2) the round part is the lower half of a circle, and a channel carrying normal flows spends most of its time inside it.

Dynamic wave computes friction from the hydraulic radius at the conduit's upstream end and midpoint, `getHydRad()` → `xsect_getRofY()`. For RECT_ROUND inside the round bottom that function has a misplaced parenthesis, so the hydraulic radius is not A/P. For the 3 x 4 ft section of the test (r = 2 ft), 6.0.0's cross-section API returns:

```
  y (ft)   engine R   exact A/P   ratio
    0.25     0.1512      0.1618   0.934
    0.50     0.0054      0.3136   0.017
    1.00     0.0640      0.5865   0.109
    1.50     0.1957      0.8164   0.240
    1.90     0.2959      0.9672   0.306
```

The value falls to zero where sin t = 1 (y = 0.29 r) and grows without bound as y goes to zero. Over most of the range it is far too small, so friction is overstated by a factor (R_exact / R)^(4/3) of 5 to 230 between 0.5 and 1.9 ft and the conduit backs up until the water reaches a depth where the error is smaller.

In the test, 6.24 cfs flows through two RECT_ROUND 3 x 4 ft conduits at slope 0.001 into a NORMAL outfall. That is uniform flow: every depth should be the normal depth, 1.001 ft. The engines hold J1 at 1.625 ft and J2 at 1.739 ft, and the velocity in C2 is 1.64 ft/s instead of 2.54 ft/s.

Kinematic wave, the normal-flow limit, the full-flow capacity and the NORMAL outfall all take the section factor from `rect_round_getRofA()`, which divides A by P correctly. The same conduit therefore gives the right answer under KINWAVE and the wrong one under DYNWAVE.

## Why it happens

For a circular segment with central angle t, A = r²(t − sin t)/2 and P = r t, so R = A/P = r(1 − sin(t)/t)/2. The code divides the whole bracket by t:

```c
// src/legacy/engine/xsect.c, rect_round_getRofY()
    // --- find hyd. radius of circular section
    theta1 = 2.0*acos(1.0 - y/xsect->rBot);
    return 0.5 * xsect->rBot * (1.0 - sin(theta1)) / theta1;
```

`rect_round_getRofA()`, a few lines above, gets the same quantity right by computing `a / p`. 6.0.0 ported the line as it was:

```cpp
// src/engine/hydraulics/XSectKernels.hpp, XsectEval::rect_round_getRofY()
        double theta1 = 2.0 * std::acos(1.0 - y / xs.r_bot);
        return 0.5 * xs.r_bot * (1.0 - std::sin(theta1)) / theta1;
```

Above the round bottom (y > yBot) both versions call `rect_round_getRofA()` and are correct.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-03_rect-round.inp`](NUM-03_rect-round.inp) | Two 500 ft RECT_ROUND 3 x 4 ft conduits (r = 2 ft), n = 0.013, slope 0.001, constant 6.24 cfs into a NORMAL outfall, dynamic wave, 2 h |
| [`NUM-03_test.c`](NUM-03_test.c) | Legacy toolkit (5.2.4, 5.3.0): runs the deck, compares the steady depths at J1 and J2 with the normal depth from Manning's equation on the exact segment geometry (tolerance 5 %) |
| [`NUM-03_test6.c`](NUM-03_test6.c) | 6.0.0: the same run, plus `swmm_xsect_hydrad_of_depth()` against the exact A/P (tolerance 1 %) |

```sh
tools/run-test.sh NUM-03            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-03 --patched  # 5.3.0, 6.0.0: PASS
```

**Without the fix** (5.2.4, 5.3.0 and 6.0.0 print the same numbers):

```
Steady state after 2 h (Q = 6.24 cfs, n = 0.013, S = 0.001, r = 2 ft)
                       engine    exact
Depth at J1 (ft)        1.625    1.001
Depth at J2 (ft)        1.739    1.001
C2 flow (cfs)           6.240    6.240
C2 velocity (ft/s)      1.640    2.535
FAIL: uniform-flow depth is 1.625 ft at J1 and 1.739 ft at J2, normal depth is 1.001 ft (+62 %)
NUM-03 5.2.4 base: FAIL
NUM-03 5.3.0 base: FAIL
```

6.0.0 adds the R(y) table shown above and ends with:

```
FAIL: R(y) off by up to 98 %; uniform-flow depth 1.625 ft at J1, normal depth 1.001 ft (+62 %)
NUM-03 6.0.0 base: FAIL
```

**With the fix**, both patched engines give the normal depth:

```
  y (ft)   engine R   exact A/P   ratio
    0.25     0.1618      0.1618   1.000
    0.50     0.3136      0.3136   1.000
    1.00     0.5865      0.5865   1.000
    1.50     0.8164      0.8164   1.000
    1.90     0.9672      0.9672   1.000

Steady state after 2 h (Q = 6.24 cfs, n = 0.013, S = 0.001, r = 2 ft)
                       engine    exact
Depth at J1 (ft)        1.001    1.001
Depth at J2 (ft)        1.001    1.001
C2 flow (cfs)           6.240    6.240
C2 velocity (ft/s)      2.535    2.535
NUM-03 5.3.0 patched: PASS
NUM-03 6.0.0 patched: PASS
```

## The fix

```diff
     theta1 = 2.0*acos(1.0 - y/xsect->rBot);
-    return 0.5 * xsect->rBot * (1.0 - sin(theta1)) / theta1;
+    return 0.5 * xsect->rBot * (1.0 - sin(theta1) / theta1);
```

The 6.0.0 patch makes the same change in `XSectKernels.hpp`, and both patched engines print identical numbers. Dynamic-wave results change for every RECT_ROUND conduit that flows inside its round bottom: depths drop and velocities rise to the values that the section's geometry and Manning's equation give. Other shapes are untouched.

**Effect on other models.** None of the 73 regression decks in `regsuite` uses RECT_ROUND, so their results are unchanged.
