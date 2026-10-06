# NUM-06: Mitered culvert inlets get ten times the HDS-5 slope correction

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | The inlet-control headwater of a culvert with a mitered inlet (codes 5, 37, 46) is too high by 6.3·S·D when the inlet is submerged: 0.95 ft for a 3 ft pipe on a 5% slope (6.64 ft instead of 5.69 ft at 60 cfs). At low heads (below 7·S·D) inlet control is lost instead and the headwater comes out too low. No warning. |
| **Reached from** | Any conduit with culvert code 5 (circular CMP, mitered to slope), 37 (CMP pipe arch, mitered) or 46 (CMP arch, mitered) in [XSECTIONS], under dynamic wave |
| **5.3.0** | `culvert_getInflow()` in [`src/legacy/engine/culvert.c:209`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/culvert.c#L209) |
| **5.2.4** | Same code, [`src/solver/culvert.c:209`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/culvert.c#L209) |
| **6.0.0** | Reproduces with the same numbers: `culvert::getInflow()` in [`src/engine/hydraulics/Culvert.cpp:239`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Culvert.cpp#L239); the finite-volume kernel `culvertInflow()` in [`src/engine/hydraulics/HydClosureKernels.hpp:141`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/HydClosureKernels.hpp#L141) has the same constant |
| **Since** | 5.1.001 (March 2014), when culvert inlet control was added (commit fa32a734) |
| **Fix** | `-7.0` → `-0.7`: [`NUM-06_swmm530.patch`](NUM-06_swmm530.patch), [`NUM-06_swmm600.patch`](NUM-06_swmm600.patch) |

## The problem

FHWA HDS-5 and SWMM's own Hydraulics Reference Manual (section 7.4, eqs. 7-39 and 7-46) add a slope correction to the inlet-control headwater:

```
HW/D = (inlet-control curve) + Scf·S      Scf = -0.5 for most inlets, +0.7 for mitered inlets
```

SWMM applies `+7·S` to mitered inlets instead of `+0.7·S`. The error grows with slope: on a 5% slope it adds 0.315 to HW/D, which for a 3 ft culvert is almost a foot of headwater. Mitered inlets are common on corrugated metal pipe culverts under roads, and steep slopes are where they are used most.

In the test, a 3 ft CMP culvert with a mitered inlet (code 5) on a 5% slope carries 60 cfs to a free outfall. Q/(A·√D) = 4.90, so the inlet is submerged and HDS-5 gives the headwater in closed form:

| | HW (ft) | HW/D |
|---|---|---|
| HDS-5, `+0.7·S` | 5.691 | 1.897 |
| SWMM 5.2.4, 5.3.0, 6.0.0 | 6.637 | 2.212 |
| with the fix | 5.691 | 1.897 |

For a road crossing, 0.95 ft of extra headwater can turn a crossing that passes the design flow into one that overtops, or make a culvert look undersized when it is not.

The same factor also shifts the submerged-flow threshold (`y2 = D·(16c + Y - scf)`), and it breaks the unsubmerged Form 1 equation at low heads: `getForm1Flow()` solves for critical depth with `hPlus = h/D + scf`, and when `h/D < 7·S` that term is negative, the equation has no root in the bracket, and `getForm1Flow()` returns the critical flow at depth `h` from the last evaluation, which is normally larger than the actual flow, so inlet control does not apply. So below `7·S·D` (1.05 ft here) the mitered culvert loses inlet control altogether. With the deck's inflow changed to 5 cfs, the unpatched engines give a headwater of 0.70 ft and the patched ones 0.85 ft (a headwall inlet, code 4, gives 0.77 ft: the mitered inlet should be the less efficient one). At 15 and 40 cfs the unpatched headwater is 0.91 and 1.08 ft too high (2.80 vs 1.89 ft, 4.80 vs 3.72 ft). (These are variants of the test deck with the inflow and culvert code changed; they are not part of the test.)

## Why it happens

```c
// src/legacy/engine/culvert.c, culvert_getInflow()
    // --- slope correction factor (-7 for mitered inlets, 0.5 for others)
    switch (code)
    {
    case 5:
    case 37:
    case 46: culvert.scf = -7.0 * Conduit[k].slope; break;
    default: culvert.scf = 0.5 * Conduit[k].slope;
    }
```

The code stores the correction with the opposite sign to HDS-5: the submerged equation is evaluated as `(h/D - Y + scf)/c`, i.e. `HW/D = c·(Q/AD)² + Y - scf`, and Form 1 as `hPlus = h/D + scf`. So `scf = 0.5·S` reproduces HDS-5's `-0.5·S` for ordinary inlets, and mitered inlets need `scf = -0.7·S`. The constant is `-7.0`, a factor of ten off; the comment repeats it.

6.0.0 copied the constant into `culvert::getInflow()` (dynamic wave) and into the finite-volume kernel `culvertInflow()`.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-06_mitered.inp`](NUM-06_mitered.inp) | C1: 3 ft CMP, n = 0.024, code 5, 100 ft on a 5% slope, 60 cfs into J1, free outlet; 2 hours |
| [`NUM-06_test.c`](NUM-06_test.c) | Legacy toolkit (5.2.4, 5.3.0): runs the deck and compares J1's steady depth with the HDS-5 submerged inlet-control headwater (`+0.7·S`), tolerance 0.15 ft |
| [`NUM-06_test6.c`](NUM-06_test6.c) | The same through the 6.0.0 C API |

The barrel's full-flow capacity (about 80 cfs) is above 60 cfs and the outlet is free, so the culvert is under inlet control.

```sh
tools/run-test.sh NUM-06            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-06 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 5.2.4 and 6.0.0 print the same):

```
Q/(A*sqrt(D))                            4.901 (> 4: submerged inlet)
HDS-5 headwater, Scf*S = +0.7*S          5.691 ft
same with +7*S                           6.637 ft
SWMM: C1 flow                            60.00 cfs
SWMM: J1 headwater (steady)              6.637 ft
FAIL: the mitered culvert's headwater is 6.637 ft, HDS-5 gives 5.691 ft (+0.946 ft; a 7*S slope correction gives 6.637 ft)
NUM-06 5.3.0 base: FAIL
```

The engine's headwater is exactly the HDS-5 value with `+7·S`.

**With the fix** (5.3.0 and 6.0.0 print the same):

```
SWMM: C1 flow                            60.00 cfs
SWMM: J1 headwater (steady)              5.691 ft
PASS: the mitered culvert's headwater 5.691 ft matches HDS-5 (5.691 ft)
NUM-06 5.3.0 patched: PASS
```

## The fix

```diff
-    // --- slope correction factor (-7 for mitered inlets, 0.5 for others)
+    // --- slope correction factor (-0.7 for mitered inlets, 0.5 for others)
     switch (code)
     {
     case 5:
     case 37:
-    case 46: culvert.scf = -7.0 * Conduit[k].slope; break;
+    case 46: culvert.scf = -0.7 * Conduit[k].slope; break;
```

The 6.0.0 patch makes the same change in `Culvert.cpp` and in the finite-volume kernel in `HydClosureKernels.hpp`. The test only exercises dynamic wave; on this deck the finite-volume solver does not reach inlet control at all (J1 2.64 ft), so its line is changed for consistency but not verified here.

Effect on other models: only culverts with codes 5, 37 or 46 change. None of the 73 decks of the SWMM regression test suite uses a culvert code. Headwall culverts (code 4) on the same pipe give identical results with and without the patch (0.77, 1.66 and 3.19 ft average depth at 5, 15 and 40 cfs).

This patch and [BND-07](../../3-boundary/BND-07-culvert-inlet-control-adverse-slope/) both edit `culvert_getInflow()`; they apply together in either order.
