# CON-15: KINWAVE books water that never entered when an inflow starts abruptly in an empty conduit

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | A sudden inflow into an empty kinematic-wave conduit fills the conduit's inlet end in one step with water that did not enter. For a 15 cfs step into a 2,000-ft empty pipe the conduit holds 2,593 ft³ after the first step although at most 75 ft³ has entered, and the run ends with a flow continuity error of -1.24 %. A smaller time step does not help (-0.95 % at 60 s, -1.24 % at 5 s). The continuity error is the only sign. |
| **Reached from** | KINWAVE routing whenever a conduit's inflow rises faster than one step can fill the corresponding inlet area: a step or steep rise into an empty or nearly empty long conduit, first flush after a dry period |
| **5.3.0** | `kinwave_execute()` / `solveContinuity()` in [`src/legacy/engine/kinwave.c:139`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/kinwave.c#L139) and [`kinwave.c:248`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/kinwave.c#L248); the volume is booked in `setNewLinkState()`, [`flowrout.c:697`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/flowrout.c#L697) |
| **5.2.4** | Same code: [`src/solver/kinwave.c:250`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/kinwave.c#L250) |
| **6.0.0** | Reproduces: `KWSolver::solveConduit()` copies the clamp, [`KinematicWave.cpp:336`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/KinematicWave.cpp#L336) |
| **Since** | Every 5.x release (the kinematic-wave scheme of SWMM 5.0) |
| **Fix** | When the clamp is hit on a rising inlet, raise the inlet area only by what the inflow fills: [`CON-15_swmm530.patch`](CON-15_swmm530.patch), [`CON-15_swmm600.patch`](CON-15_swmm600.patch) |

## The problem

The test conduit is a 2-ft circular pipe, 2,000 ft long, with a full-flow capacity of 16 cfs. It is empty at the start, and a constant 15 cfs enters its upstream junction from t = 0. After one routing step of any length the conduit is booked with 2,593 ft³, half its length at the normal-flow area of 15 cfs, while 15 cfs × Δt has entered (75 ft³ for Δt = 5 s). The surplus stays in the books and shows up as a negative continuity error at the end of the 3-hour run:

| Routing step | 60 s | 30 s | 15 s | 5 s |
|---|---|---|---|---|
| Continuity error, 5.2.4 / 5.3.0 / 6.0.0 | -0.947 % | -1.114 % | -1.197 % | -1.237 % |

The error does not go away with smaller steps, because it is created in one step regardless of the step's length. A falling inflow can do the opposite (see "Limits of the fix" below).

## Why it happens

The scheme (Hydraulics Reference Manual, Eq. 4-7) balances the change in the areas at both ends of the conduit against the flows in and out over the step:

```
[(1-θ)(A1' - A1) + θ(A2' - A2)] / Δt + [(1-φ)(Q2 - Q1) + φ(Q2' - Q1')] / L = 0      θ = φ = 0.6
```

`kinwave_execute()` sets the new inlet area A1' straight from the new inflow, `ain = xsect_getAofS(pXsect, qin/Beta1) / Afull`. For a long empty conduit, (1-θ)·A1'·L is far more water than φ·Q1'·Δt, so the equation needs a negative outlet area. `solveContinuity()` finds f(A) > 0 at both ends of its bracket and clamps:

```c
// src/legacy/engine/kinwave.c, solveContinuity()
// --- if lower/upper bound functions both positive then use no flow
else if ( fLo > 0 )
{
    *aout = 0.0;
    n = -3;
}
```

The manual describes this clamp ("set to 0 if both f(A_LOW) and f(A_HIGH) are positive"). With A2' = 0 the equation is not satisfied, but A1' is stored as the new inlet area anyway, and `setNewLinkState()` books `0.5 * (a1 + a2) * length` of water. In the test, a1 is the normal-flow area of 15 cfs (2.593 ft²) and the booked volume is 0.5 × 2.593 × 2,000 = 2,593 ft³. That water never entered; when the conduit reaches steady state it is part of the steady-state volume, so it is never given back.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-15_step-inflow.inp`](CON-15_step-inflow.inp) | J1 - C1 (2 ft circular, 2,000 ft, slope 0.5 %) - O1, KINWAVE, constant 15 cfs inflow at J1 from t = 0, 3 hours |
| [`CON-15_test.c`](CON-15_test.c) | Legacy toolkit (5.2.4 and 5.3.0): runs the deck at routing steps of 60, 30, 15 and 5 s and reads the flow continuity error; with 5.3.0 also the conduit volume after the first step (`swmm_LINK_VOLUME` does not exist in 5.2.4) |
| [`CON-15_test6.c`](CON-15_test6.c) | The same for 6.0.0 |

The test requires |continuity error| < 0.1 % and a first-step conduit volume no larger than the 15 cfs × Δt that has entered.

```sh
tools/run-test.sh CON-15            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-15 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 5.2.4 prints the same errors without the volume column, 6.0.0 the same volumes):

```
Step (s)  Inflow in 1st step (ft3)  C1 volume after it (ft3)  Continuity error (%)
      60                     900.0                    2593.2                -0.947
      30                     450.0                    2593.2                -1.114
      15                     225.0                    2593.2                -1.197
       5                      75.0                    2593.2                -1.237
FAIL: 4 of 4 runs do not conserve volume (worst continuity error -1.237 %)
```

**With the fix**:

```
---- 5.3.0 ----
Step (s)  Inflow in 1st step (ft3)  C1 volume after it (ft3)  Continuity error (%)
      60                     900.0                     675.0                 0.003
      30                     450.0                     337.5                 0.003
      15                     225.0                     168.7                 0.001
       5                      75.0                      56.2                 0.013
PASS: the conduit holds only the water that entered; continuity error below 0.1 % at every step size

---- 6.0.0 ----
      60                     900.0                     675.0                 0.007
      30                     450.0                     337.5                 0.004
      15                     225.0                     168.7                 0.001
       5                      75.0                      56.2                 0.013
```

The first-step volume is now 0.75 × the inflow: the scheme stores φ·Q·Δt = 0.6 × 15 × Δt at the inlet end with weight (1-θ) = 0.4, and the volume is booked with weight 0.5. The two weightings agree again once the conduit is at uniform flow.

The patched engines differ in the third decimal at 60 and 30 s. The cause is not the patch: 6.0.0's engine is compiled with `-mfma`, the compiler fuses `C2 + (1-WX)/WX*dq` into one FMA, and C2 comes out one ulp off legacy's. On the first step that uses Newton's method after the inlet has filled, the two engines then stop at different points within the 0.001 tolerance (outlet areas 0.06591 and 0.06540 of full). The unpatched engines differ for the same reason at other step sizes (10 s: -1.224 % and -1.222 %; 1 s: -1.037 % and -1.057 % in their reports).

## The fix

When the clamp is hit while the inlet area rises, use the inlet area for which the equation holds with A2' = 0 instead of the normal-flow area. Since f(0) = C2 and C2 is linear in A1', that area is `ain - C2*WX/((1-WT)*dxdt)`. It is never set below the previous inlet area:

```diff
+        // --- if the inflow cannot fill a rise in inlet area within this
+        //     step (no outflow solution), raise the inlet area only by what
+        //     the inflow fills, so that continuity still holds
+        if ( result == -3 && ain > a1 )
+            ain = MAX(a1, ain - C2 * WX / ((1.0 - WT) * dxdt));
         if ( result <= 0 ) result = 1;
```

The inlet area then grows step by step as the inflow fills it, and the normal solution, with outflow, resumes as soon as the inflow can fill the remaining rise within one step (the third 60-s step in the test). The inflow the conduit accepts, and so the upstream node's balance, is unchanged. The 6.0.0 patch makes the same change in the no-flow branch of `KWSolver::solveConduit()`.

**Limits of the fix.** Two related effects remain and are separate defects:

- *Newton tolerance.* The KW root finder stops within 0.001 of the full area (documented in the manual), and the error is biased towards losing water while a conduit fills. At steps of 1-2 s it adds +0.1 to +0.2 %: with the fix the 1-s and 2-s runs of the test deck end at +0.203 % and +0.079 % (5.3.0). With the tolerance set to 1e-9 in a private build, the same patched runs give 0.000 % at 5 s and 1 s. In the unpatched code this drift partly cancels the defect fixed here: the review's ramp deck (0 to 15 cfs over 30 min into the same pipe, `review/R2/kw_cap/v_rampup.inp`) shows +0.001 % unpatched (-0.161 % with the tight tolerance) and +0.147 % patched (0.000 % with the tight tolerance). So a model with gradual rises can show a larger continuity error after this patch until the tolerance is also fixed.
- *Abrupt recession.* The mirror case, an inflow that drops faster than the outlet can respond, hits the other clamp (`*aout = 1.0`, full flow) and destroys water. The review's step-down deck (`v_stepdown.inp`: 15 cfs to 0 within one minute after 2 hours) ends at -0.925 % unpatched, where the two errors partly cancel, and +0.941 % patched. Limiting the inlet-area drop in the same way is possible but would keep the outlet at full flow; it needs its own fix.

**Effect on other models.** Of the 31 KINWAVE decks in the regression suite, 30 ran (ncdc_format could not find its rain file in the scratch copy); only Example1 changed. Its flow continuity error goes from 0.106 % to 0.110 %, link peak flows change by at most 0.024 cfs (0.13 %), and the outfall volume from 1.914 to 1.913 10⁶ gal.
