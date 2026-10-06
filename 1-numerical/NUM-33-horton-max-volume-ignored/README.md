# NUM-33: Horton maximum infiltration volume is ignored for a constant-rate curve

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A Horton or Modified Horton soil with `f0 = fmin` or `decay = 0` infiltrates without limit even when a maximum infiltration volume (Fmax) is given. With Fmax = 0.5 in, a 1 in storm infiltrates 1.00 in instead of 0.50 in, and the 0.50 in of runoff it should produce is lost. The input is accepted without a warning. |
| **Reached from** | `[INFILTRATION]` rows for HORTON or MODIFIED_HORTON with a 5th value Fmax > 0 and either f0 = fmin or decay = 0 |
| **5.3.0** | Constant-rate shortcut in `horton_getInfil()`, [`src/legacy/engine/infil.c:415`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/infil.c#L415), and `modHorton_getInfil()`, [`infil.c:524`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/infil.c#L524) |
| **5.2.4** | Same code, [`src/solver/infil.c:410`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/infil.c#L410) and [`infil.c:519`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/infil.c#L519) |
| **6.0.0** | Reproduces with the same numbers: `infil::horton_getInfil()` in [`src/engine/hydrology/Infiltration.cpp:78`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Infiltration.cpp#L78) and `infil::modHorton_getInfil()` at [`Infiltration.cpp:164`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Infiltration.cpp#L164) |
| **Since** | Every release (5.0.022 has the same early return ahead of the Fmax code) |
| **Fix** | Apply the Fmax cap, with dry-weather recovery, in the constant-rate branch: [`NUM-33_swmm530.patch`](NUM-33_swmm530.patch), [`NUM-33_swmm600.patch`](NUM-33_swmm600.patch) |

## The problem

Fmax is the "maximum infiltration volume possible" of a Horton soil (input reference, `[INFILTRATION]`; Vol I sec. 4.2.3 limits the cumulative infiltration to it). A soil with a constant infiltration capacity and a finite storage, for example a thin layer over rock or a clay liner, is a natural use of the parameters: set f0 = fmin, or decay = 0, and give Fmax.

SWMM ignores Fmax for exactly those soils. In the test deck six pervious subcatchments get 1 in of rain in one hour; all have Fmax = 0.5 in and a capacity of at least 0.5 in/hr:

| Subcatchment | Method | Curve | Infiltration (in) |
|---|---|---|---|
| H1 | HORTON | f0 3, fmin 0.5 in/hr, decay 4/hr | 0.50 |
| H2 | HORTON | f0 = fmin = 3 in/hr | **1.00** |
| H3 | HORTON | decay = 0 | **1.00** |
| M1 | MODIFIED_HORTON | f0 3, fmin 0.5 in/hr, decay 4/hr | 0.50 |
| M2 | MODIFIED_HORTON | f0 = fmin = 3 in/hr | **1.00** |
| M3 | MODIFIED_HORTON | decay = 0 | **1.00** |

The decaying curves stop at 0.50 in and send the rest to runoff (0.42 in after depression storage). The constant-rate soils take the whole storm and produce no runoff. Nothing in the report hints at it: runoff continuity is closed, because the extra water is booked as infiltration (and goes on to the groundwater module if one is attached).

## Why it happens

Both functions handle the constant-rate case first and return before the code that limits the cumulative infiltration:

```c
// src/legacy/engine/infil.c, horton_getInfil()
    // --- special cases of no infil. or constant infil
    if ( df < 0.0 || kd < 0.0 || kr < 0.0 ) return 0.0;
    if ( df == 0.0 || kd == 0.0 )
    {
        fp = f0;
        fa = irate + depth / tstep;
        if ( fp > fa ) fp = fa;
        return MAX(0.0, fp);              // Fmax never checked, Fe never accumulated
    }
    ...
        // --- limit cumulative infiltration to Fmax
        if ( Fmax > 0.0 )
        {
            if ( infil->Fe + fp * tstep > Fmax )
                fp = (Fmax - infil->Fe) / tstep;
            fp = MAX(fp, 0.0);
            infil->Fe += fp * tstep;
        }
```

`modHorton_getInfil()` has the same early return ahead of its Fmax block (which in 5.3.0 caps the accumulator `Fmh`). 6.0.0 ported both functions line by line.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-33_one-storm.inp`](NUM-33_one-storm.inp) | The six soils above, 1 in/hr of rain for one hour, 6-hour run |
| [`NUM-33_two-storms.inp`](NUM-33_two-storms.inp) | The same storm on 1 June and on 11 June; drying time 7 days, evaporation 0.2 in/day empties the 0.05 in depression storage between storms |
| [`NUM-33_test.c`](NUM-33_test.c) | Runs both decks through the legacy toolkit and reads each subcatchment's Total Infil from the Subcatchment Runoff Summary. A constant-rate soil must take at most Fmax + 0.01 in from one storm, and 0.95 to 1.01 in (Fmax per storm, recovered in between) from two |
| [`NUM-33_test6.c`](NUM-33_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-33            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-33 --patched  # 5.3.0, 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 6.0.0 prints the same table):

```
Sub  Soil                          Infil (in)   Infil (in)
                                    one storm   two storms
H1   HORTON decaying (ref.)            0.50         1.00
H2   HORTON f0 = fmin                  1.00         2.00   <-- more than Fmax in one storm
H3   HORTON decay = 0                  1.00         2.00   <-- more than Fmax in one storm
M1   MOD_HORTON decaying (ref.)        0.50         1.00
M2   MOD_HORTON f0 = fmin              1.00         2.00   <-- more than Fmax in one storm
M3   MOD_HORTON decay = 0              1.00         2.00   <-- more than Fmax in one storm

FAIL: 4 of 4 constant-rate soils infiltrate more than Fmax = 0.50 in in one storm (up to 1.00 in), 4 of 4 do not take Fmax per storm over two storms (2.00 in instead of 1.00 in)
NUM-33 5.3.0 base: FAIL
NUM-33 6.0.0 base: FAIL
```

5.2.4 prints the same rows for H2, H3, M2 and M3 (`NUM-33 5.2.4 base: FAIL`). Its M1 shows 0.08 / 0.17 in, which is a different 5.2.4 bug in the decaying Modified Horton curve, [NUM-56](../NUM-56-modified-horton-max-volume-524/README.md); the test does not check the reference rows.

**With the fix:**

```
H1   HORTON decaying (ref.)            0.50         1.00
H2   HORTON f0 = fmin                  0.50         1.00
H3   HORTON decay = 0                  0.50         1.00
M1   MOD_HORTON decaying (ref.)        0.50         1.00
M2   MOD_HORTON f0 = fmin              0.50         1.00
M3   MOD_HORTON decay = 0              0.50         1.00

PASS: every constant-rate soil stops at Fmax = 0.50 in per storm and recovers between storms
NUM-33 5.3.0 patched: PASS
NUM-33 6.0.0 patched: PASS
```

## The fix

Apply the same cap in the constant-rate branch, on the accumulator the decaying branch caps (`Fe` for Horton, `Fmh` for Modified Horton), and let it recover during dry steps:

```diff
         if ( fp > fa ) fp = fa;
-        return MAX(0.0, fp);
+        fp = MAX(0.0, fp);
+
+        // --- limit cumulative infiltration to Fmax (recovers when dry)
+        if ( Fmax > 0.0 )
+        {
+            if ( infil->Fe + fp * tstep > Fmax )
+                fp = MAX((Fmax - infil->Fe) / tstep, 0.0);
+            infil->Fe += fp * tstep;
+            if ( fa <= ZERO ) infil->Fe *= exp(-kr * tstep);
+        }
+        return fp;
```

The recovery is needed: without it a capped soil would stay saturated for the rest of the run (the two-storm deck would give 0.50 in instead of 1.00 in). A constant-rate curve has no Horton time `tp` to regenerate, so the stored volume decays with the drying-time constant, `exp(-kr*dt)`, which is what Modified Horton already does with its `Fe` and `Fmh` and what the drying time means (98 % recovery in that many days). The 6.0.0 patch makes the same change in `Infiltration.cpp`; both patched engines print the same values.

Only soils with Fmax > 0 and a constant-rate curve change. None of the 73 regression decks has one; `user/user2.inp` (17 decaying Horton curves with Fmax) and `examples/Example1.inp` give reports identical to the unpatched engines in both 5.3.0 and 6.0.0.
