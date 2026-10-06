# IO-12: Ellipse and arch pipes: a zero width turns the full height into the size code

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A standard elliptical or arch pipe given by its full height and size code, with the width left at 0, is built as a different standard pipe: `ARCH 5.17 0 12` (62 x 102 in, 34.6 ft²) becomes code 5 (22.5 x 36.25 in, 4.4 ft²). A custom height with the width left out (`HORIZ_ELLIPSE 1.2 0 0 0` in metres) becomes a 0.36 m pipe. A size code that is not a whole number (`12.5`) is truncated. No message in any of these cases. |
| **Reached from** | `[XSECTIONS]` lines for HORIZ_ELLIPSE, VERT_ELLIPSE and ARCH with Geom2 = 0, or with a fractional Geom3; in 6.0.0 also `swmm_link_set_xsect()` |
| **5.3.0** | `xsect_setParams()` in [`src/legacy/engine/xsect.c:557`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L557), [`:585`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L585), [`:613`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L613) |
| **5.2.4** | Same code, [`src/solver/xsect.c:553`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/xsect.c#L553), `:581`, `:609` |
| **6.0.0** | Reproduces: `setParams()` in [`src/engine/hydraulics/XSection.cpp:477`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/XSection.cpp#L477), `:499`, `:521` |
| **Since** | 5.1.008, which moved the size code to Geom3 and kept Geom1 as the code when Geom2 is 0 (the SWMM 5.0 form). Truncation of the code since 5.0. |
| **Fix** | Use the 5.0 form only when Geom3 is 0; reject a size code that is not a whole number: [`IO-12_swmm530.patch`](IO-12_swmm530.patch), [`IO-12_swmm600.patch`](IO-12_swmm600.patch) |

## The problem

The manual ([XSECTIONS], Table D-2) gives the parameters of the three shapes as

| Shape | Geom1 | Geom2 | Geom3 |
|---|---|---|---|
| HORIZ_ELLIPSE, VERT_ELLIPSE | Full Height | Max. Width | Size Code |
| ARCH | Full Height | Max. Width | Size Code |

where the size code is "the size code of a standard shaped elliptical [or arch] pipe as listed in Appendix A12 [A13]. Leave blank (or 0) if the pipe has custom dimensions". A size code fixes the height and the width, so the documented rule is: with a code, use the standard pipe; without one, use Geom1 and Geom2.

The engine has a third, undocumented rule from SWMM 5.0, where the size code was entered as Geom1 with Geom2 = 0: **whenever Geom2 is 0, Geom1 becomes the size code**, whatever Geom3 holds. It is then truncated to a whole number. So:

- `C1 HORIZ_ELLIPSE 2 0 4` (height 2 ft and code 4; the width is left to the code) is built as code 2, a 19 x 30 in pipe of 3.30 ft², instead of code 4, 24 x 38 in and 5.10 ft². The Geom3 code is ignored.
- `C3 ARCH 5.17 0 12` is built as code 5, an arch of 4.4 ft² instead of 34.6 ft².
- `C1 HORIZ_ELLIPSE 1.2 0 0 0` in an SI model (a 1.2 m custom pipe whose width was left out) is built as code 1, 14 x 23 in, i.e. 0.36 m x 0.58 m. A RECT_CLOSED with no width is rejected with ERROR 211; the ellipse is not.
- `C1 ARCH 4.5 7.0 12.5 0` has no valid size code, but is built as code 12.

None of these give a message. The 5.0 form itself is still in use: `user/user2.inp` in the regression suite has five conduits such as `HORIZ_ELLIPSE 17.0 0 0 0`. The fix keeps it.

## Why it happens

```c
// src/legacy/engine/xsect.c, xsect_setParams() (VERT_ELLIPSE and ARCH are the same)
    case HORIZ_ELLIPSE:
        if ( p[1] == 0.0 ) p[2] = p[0];          // Geom1 replaces any Geom3 code
        if ( p[2] > 0.0 )                        // std. ellipse pipe
        {
            index = (int)floor(p[2]) - 1;        // size code: 12.5 -> 12
            if ( index < 0 ||
                 index >= NumCodesEllipse ) return FALSE;
            xsect->yFull = MinorAxis_Ellipse[index]/12.;
            ...
```

The first line was added in 5.1.008, when the size code moved from Geom1 to Geom3, so that 5.0 files still read. It does not check whether Geom3 already holds a code. Geom1 must be positive for every shape (`p[0] <= 0.0` is rejected at the top of the function), so writing `0 0 4` to avoid the rule is rejected with ERROR 211.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-12_size-codes.inp`](IO-12_size-codes.inp) | C1 `HORIZ_ELLIPSE 2 0 4`, C2 `VERT_ELLIPSE 3.17 0 4`, C3 `ARCH 5.17 0 12` (height and code), C4 `HORIZ_ELLIPSE 4 0 0` (5.0 form) |
| [`IO-12_height-as-code.inp`](IO-12_height-as-code.inp) | SI model, C1 `HORIZ_ELLIPSE 1.2 0 0 0` |
| [`IO-12_fractional-code.inp`](IO-12_fractional-code.inp) | C1 `ARCH 4.5 7.0 12.5 0` |
| [`IO-12_test.c`](IO-12_test.c) | Runs the three decks through the legacy toolkit (5.2.4, 5.3.0) and reads the Cross Section Summary |
| [`IO-12_test6.c`](IO-12_test6.c) | The same through `swmm_engine_run()` (6.0.0) |

The expected sizes are the table values (Appendix A12/A13, `xsect.dat`): ellipse code 4 is 24 x 38 in with 5.10 ft², arch code 12 is 62 x 102 in with 34.60 ft². The first deck must run with those sizes, and the other two must be rejected.

```sh
tools/run-test.sh IO-12            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-12 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (all three engines print the same):

```
IO-12_size-codes.inp: error 0
Link  Xsection                Depth    Area   Width   expected
C1    HORIZ_ELLIPSE 2 0 4       1.58    3.30    2.50   2.00 5.10 3.17   <-- wrong
C2    VERT_ELLIPSE 3.17 0 4     2.83    4.10    1.83   3.17 5.10 2.00   <-- wrong
C3    ARCH 5.17 0 12            1.88    4.40    3.02   5.17 34.60 8.50   <-- wrong
C4    HORIZ_ELLIPSE 4 0 0       2.00    5.10    3.17   2.00 5.10 3.17
IO-12_height-as-code.inp: HORIZ_ELLIPSE 1.2 0 0 0 (m) accepted, C1 built as depth 0.36 m, width 0.58 m
IO-12_fractional-code.inp: ARCH 4.5 7.0 12.5 0 accepted, C1 built as depth 5.17 ft, width 8.50 ft
FAIL: C1 HORIZ_ELLIPSE 2 0 4 has the wrong size, C2 VERT_ELLIPSE 3.17 0 4 has the wrong size, C3 ARCH 5.17 0 12 has the wrong size, HORIZ_ELLIPSE 1.2 0 0 0 (m) accepted as a 0.36 m high pipe, ARCH 4.5 7.0 12.5 0 accepted as a 5.17 ft high pipe
IO-12 5.3.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same Cross Section Summary, including full flows of 13.30 cfs for C1, C2 and C4 and 168.95 cfs for C3):

```
IO-12_size-codes.inp: error 0
Link  Xsection                Depth    Area   Width   expected
C1    HORIZ_ELLIPSE 2 0 4       2.00    5.10    3.17   2.00 5.10 3.17
C2    VERT_ELLIPSE 3.17 0 4     3.17    5.10    2.00   3.17 5.10 2.00
C3    ARCH 5.17 0 12            5.17   34.60    8.50   5.17 34.60 8.50
C4    HORIZ_ELLIPSE 4 0 0       2.00    5.10    3.17   2.00 5.10 3.17
IO-12_height-as-code.inp: HORIZ_ELLIPSE 1.2 0 0 0 (m) rejected with error 200
IO-12_fractional-code.inp: ARCH 4.5 7.0 12.5 0 rejected with error 200
PASS: size codes in Geom3 and in the 5.0 form give the standard sizes, and invalid size codes are rejected
IO-12 5.3.0 patched: PASS
```

(6.0.0 returns its own error code, 5, for the two rejected decks.) The rejected decks report `ERROR 211: invalid number` with an empty token; that message is [IO-13](../IO-13-xsection-error-empty-token/).

## The fix

In each of the three cases:

```diff
-        if ( p[1] == 0.0 ) p[2] = p[0];
+        if ( p[1] == 0.0 && p[2] == 0.0 ) p[2] = p[0];  // SWMM 5.0 format
         if ( p[2] > 0.0 )                        // std. ellipse pipe
         {
             index = (int)floor(p[2]) - 1;        // size code
-            if ( index < 0 ||
+            if ( index < 0 || index + 1 != p[2] ||
                  index >= NumCodesEllipse ) return FALSE;
```

and the same two changes in `XSection.cpp`. Lines written by the GUI (height, width and code, or height, width and 0) and 5.0-form lines with a whole code read exactly as before. **Effect on other models:** `user/user2.inp`, the only regression deck with these shapes (five conduits in the 5.0 form), gives byte-identical `.rpt` files (apart from the run timestamps) and identical `.out` files in both patched engines.

A 5.0-form line whose height happens to be a whole number (`HORIZ_ELLIPSE 2 0 0 0` meant as a 2 ft custom pipe with the width left out) is still read as size code 2. The engine cannot tell the two meanings apart. The manual should document the 5.0 form, which it does not mention at all.
