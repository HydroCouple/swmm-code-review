# CON-17: A custom elliptical pipe gets the area of a standard ellipse, whatever its span

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | An elliptical pipe given by rise and span (no size code) gets its full area and hydraulic radius from the minor axis alone, as if it had the standard 1:1.56 proportions. Its top width, and so the surface area it adds to its nodes, uses the real span. A 2 ft x 6 ft pipe gets 5.08 ft² instead of about 9.4 ft². In the test the flow routing continuity error is +3.10 % (horizontal) and +1.90 % (vertical) where a consistent pipe gives −0.16 % and −0.32 %. No warning. The 5.3.0 change log says this was fixed. |
| **Reached from** | `[XSECTIONS]` HORIZ_ELLIPSE or VERT_ELLIPSE with Geom2 > 0 and no size code, with proportions other than 1:1.56 |
| **5.3.0** | `xsect_setParams()` in [`src/legacy/engine/xsect.c:576`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L576) and [`:604`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L604); change-log claim at [`:37`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L37) |
| **5.2.4** | Same code, [`src/solver/xsect.c:572`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/xsect.c#L572) and [`:600`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/xsect.c#L600) |
| **6.0.0** | Reproduces: `setParams()` in [`src/engine/hydraulics/XSection.cpp:489`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/XSection.cpp#L489) and [`:511`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/XSection.cpp#L511) |
| **Since** | Every SWMM 5 release (5.0.022 has the same formulas; the 5.3.0 change log says they come from SWMM 4.4). Fixed on EPA `develop` in 467e52a8 (issue #144, October 2023), reverted in 9bf4fb4e (October 2024), and re-applied on the `dev-elliptical_pipes` branch (0c0a5ca3, 8e27af6c), which has not been merged. The 5.3.0 copy (03ed283a) took the change-log line but not the code. |
| **Fix** | The formulas from 0c0a5ca3: area and radius from rise and span: [`CON-17_swmm530.patch`](CON-17_swmm530.patch), [`CON-17_swmm600.patch`](CON-17_swmm600.patch) |

## The problem

The manual lets an elliptical pipe be given either by a size code (a standard pipe from Appendix A12) or by its full height and maximum width. For the second kind the engine computes the full area and full hydraulic radius from one axis only:

| Shape | Area used | Radius used | Width used |
|---|---|---|---|
| HORIZ_ELLIPSE, rise *y*, span *w* | 1.2692 *y*² | 0.3061 *y* | *w* |
| VERT_ELLIPSE, rise *y*, span *w* | 1.2692 *w*² | 0.3061 *w* | *w* |

These are the full area and radius of a standard elliptical pipe whose span is 1.56 times its rise. For any other proportions the area table (scaled by `aFull`) and the width table (scaled by `wMax`) describe two different pipes. A horizontal 2 ft x 6 ft pipe has a full area of 5.08 ft², the same as a 2 ft x 3.13 ft pipe, while an ellipse with these axes has π/4 · 2 · 6 = 9.42 ft². The conduit holds about half the water it should and conveys about half the flow, but its top width is the full 6 ft.

Dynamic wave routing uses both tables: conduit volume and flow come from the area, and the surface area each conduit adds to its end nodes comes from the width. When the two disagree, the routing does not conserve volume. In the test, a 15 cfs hydrograph through three flat 2000 ft conduits gives a continuity error of +3.10 % with 2 ft x 6 ft horizontal ellipses and +1.90 % with 6 ft x 2 ft vertical ones.

The header of 5.3.0's `xsect.c` says, under "Build 5.3.0":

```c
//   - Addressing inconsistent results for custom sized elliptical pipes.
//     Stems from incorrect equations for pipe full area and hydraulic radius
//     for elliptical pipes from SWMM 4.4.
```

The code below it is unchanged from 5.2.4.

## Why it happens

```c
// src/legacy/engine/xsect.c, xsect_setParams()
    case HORIZ_ELLIPSE:
        ...
        else
        {
            // --- length of minor axis
            xsect->yFull = p[0]/ucf;

            // --- length of major axis
            if ( p[1] < 0.0 ) return FALSE;
            xsect->wMax = p[1]/ucf;
            xsect->aFull = 1.2692 * xsect->yFull * xsect->yFull;   // span not used
            xsect->rFull = 0.3061 * xsect->yFull;                  // span not used
        }

    case VERT_ELLIPSE:
        ...
            xsect->aFull = 1.2692 * xsect->wMax * xsect->wMax;     // rise not used
            xsect->rFull = 0.3061 * xsect->wMax;
```

The fix was written in 2023 for EPA issue #144 (467e52a8 on EPA `develop`), reverted a year later together with an unrelated change (9bf4fb4e), and re-applied on its own on the `dev-elliptical_pipes` branch (0c0a5ca3; 76c8891c briefly forced the span to 1.56 x the rise and 8e27af6c undid that). When the legacy engine was copied into `src/legacy/engine` (03ed283a), the change-log entry came along and the code did not.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-17_horiz-2x6.inp`](CON-17_horiz-2x6.inp) | Three flat 2000 ft HORIZ_ELLIPSE conduits, 2 ft high and 6 ft wide; 15 cfs hydrograph (1 h rise, 2 h plateau, 1 h fall), 8 h run, free outfall |
| [`CON-17_vert-6x2.inp`](CON-17_vert-6x2.inp) | The same with VERT_ELLIPSE 6 ft high and 2 ft wide |
| [`CON-17_test.c`](CON-17_test.c) | Runs both through the legacy toolkit (5.2.4, 5.3.0); reads C1's full area from the report and the flow routing continuity error from `swmm_getMassBalErr()` |
| [`CON-17_test6.c`](CON-17_test6.c) | The same through the 6.0.0 API (`swmm_get_routing_continuity_error()`) |

The test requires the full area to be within 5 % of a true ellipse with the same axes (9.42 ft²), and the continuity error to be within ±1 %. The 5 % allows for SWMM's standard elliptical shape being about 3 % fuller than a true ellipse: Table A12 gives 5.10 ft² for 24 in x 38 in, while π/4 · 2 · 3.17 = 4.97 ft². With a pipe of nearly standard proportions (HORIZ_ELLIPSE 2 3.2), the same model has a continuity error of −0.224 % in 5.3.0.

```sh
tools/run-test.sh CON-17            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh CON-17 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0 print the same; 6.0.0 gives 3.097 % through its API):

```
Deck                Full area  True ellipse  Ratio   HydRad  Width  Continuity
                    (ft2)      (ft2)                 (ft)    (ft)   error (%)
CON-17_horiz-2x6         5.08          9.42  0.539    0.61   6.00       3.098
CON-17_vert-6x2          5.08          9.42  0.539    0.61   2.00       1.896
FAIL: CON-17_horiz-2x6: full area 5.08 ft2 (54% of the ellipse), continuity error 3.098%; CON-17_vert-6x2: full area 5.08 ft2 (54% of the ellipse), continuity error 1.896%
CON-17 5.3.0 base: FAIL
```

**With the fix**:

```
Deck                Full area  True ellipse  Ratio   HydRad  Width  Continuity
                    (ft2)      (ft2)                 (ft)    (ft)   error (%)
CON-17_horiz-2x6         9.74          9.42  1.033    0.85   6.00      -0.164
CON-17_vert-6x2          9.74          9.42  1.033    0.85   2.00      -0.317
PASS: custom ellipses get the area of their own axes and conserve volume
CON-17 5.3.0 patched: PASS
```

The patched 6.0.0 writes the same report: continuity errors −0.164 % and −0.317 %, and the same node depths and link flows. Its API returns −0.165 % for the first deck. This is the same value rounded differently; the report shows −0.164 %.

## The fix

The formulas of 0c0a5ca3, for both orientations:

```diff
-            xsect->aFull = 1.2692 * xsect->yFull * xsect->yFull;
-            xsect->rFull = 0.3061 * xsect->yFull;
+            xsect->aFull = 0.8117 * xsect->yFull * xsect->wMax;
+            xsect->rFull = 0.2448 * sqrt(xsect->yFull * xsect->wMax);
```

For a span of 1.5636 times the rise, these give exactly the old values (1.2692 = 0.8117 x 1.5636 and 0.3061 = 0.2448 x √1.5636), so standard proportions are unchanged. A pipe entered with a span of 1.6 times the rise gets 2.3 % more area and 1.2 % more radius than before. **Effect on other models:** the regression decks have no custom elliptical pipes (the only elliptical conduits, in `user/user2.inp`, use size codes), so no regression result changes.

The new radius is an approximation, and it overshoots for flat pipes. For a true ellipse 2 ft x 6 ft the hydraulic radius is 0.705 ft (area over Ramanujan's perimeter). The fix gives 0.848 ft (+20 %) and the old formula 0.612 ft (−13 %). At 2 ft x 4 ft the three values are 0.649, 0.692 (+7 %) and 0.612 ft (−6 %). Full-flow capacity, which goes with A·R^(2/3), is 17 % high with the fix and 51 % low without it at 2 x 6. This patch reuses the vetted change as it stands. Computing `rFull` as `aFull` over the perimeter of the scaled shape would be closer.

The non-standard ARCH has a similar but smaller mismatch: its area uses both rise and span (`0.7879 * yFull * wMax`), but its radius uses the rise only (`0.2991 * yFull`). It is not part of the fix on `dev-elliptical_pipes` and is not changed here.
