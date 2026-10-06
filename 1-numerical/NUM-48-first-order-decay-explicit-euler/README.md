# NUM-48: First-order decay uses c(1 − K1Δt) instead of the manual's c·exp(−K1Δt), so results depend on the routing step

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Under KINWAVE and DYNWAVE, concentrations in conduits and storage units with a decay coefficient depend on the routing step and are always too low. A pond decaying at K1 = 24/day for 6 h should hold 0.247875 mg/L; 5.2.4, 5.3.0 and 6.0.0 give 0.245814 with a 10 s step (−0.8 %), 0.235654 with 60 s (−4.9 %) and 0.190206 with 300 s (−23 %). Once K1Δt ≥ 1 everything is removed in one step. Under STEADY routing a negative K1 (growth, which the input reader accepts) is ignored: a conduit that should carry 10.869 mg/L carries 10.000. No warning. |
| **Reached from** | Any `[POLLUTANTS]` entry with a non-zero `Kdecay`; the error grows with K1 × routing step (fast-reacting constituents, long KINWAVE steps) |
| **5.3.0** | `getReactedQual()` in [`src/legacy/engine/qualrout.c:613`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L613); the K1 > 0 gate of `findSFLinkQual()` at [`qualrout.c:436`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L436) |
| **5.2.4** | Same code: [`src/solver/qualrout.c:513`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/qualrout.c#L513) and [`qualrout.c:385`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/qualrout.c#L385) |
| **6.0.0** | Reproduces: the node reactor and the link kernel copy the Euler factor ([`src/engine/quality/QualityRouting.cpp:1191`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1191), [`:1375`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1375)) and the STEADY branch the `k > 0` gate ([`:1361`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1361)) |
| **Since** | 5.0 (both are in the repository's first commit, 2014) |
| **Fix** | Use `exp(-K1*dt)` and apply it for any non-zero K1: [`NUM-48_swmm530.patch`](NUM-48_swmm530.patch), [`NUM-48_swmm600.patch`](NUM-48_swmm600.patch) |

## The problem

The SWMM reference manual (Vol. III, Water Quality, section 5.2) gives the mixing equation SWMM 5 uses for conduits and storage nodes:

> c(t + Δt) = [c(t) V(t) e<sup>−K<sub>1</sub>Δt</sup> + C<sub>in</sub> Q<sub>in</sub> Δt] / (V(t) + Q<sub>in</sub> Δt)   (5-6)

and section 5.3 says: "Using Equation 5-6 as its mixing equation for both conduit links and storage nodes, SWMM 5 carries out the following three step process...". For a closed pond (no inflow, constant volume) eq. 5-6 reduces to c(t + Δt) = c(t) e<sup>−K<sub>1</sub>Δt</sup>, so after any number of steps c = c<sub>0</sub> e<sup>−K<sub>1</sub>t</sup>, whatever the step size.

A closed 4,000 ft³ pond starts at 100 mg/L with K1 = 24/day (1/h). After 6 h the answer is 100 e<sup>−6</sup> = 0.247875 mg/L. All three engines give:

| Routing step | SWMM | Error |
|---|---|---|
| 10 s | 0.245814 mg/L | −0.83 % |
| 60 s | 0.235654 mg/L | −4.93 % |
| 300 s | 0.190206 mg/L | −23.27 % |

KINWAVE models commonly use routing steps of 30 s to 5 min. For bacteria die-off, BOD or volatile constituents in ponds and long conduits, the predicted concentration then depends on a numerical setting, and refining the step changes the answer. For typical rates the error is small (Example1 converted to DYNWAVE with a 15 s step and TSS K1 = 10/day: Mass Reacted 13.584 lb, 13.579 lb with the fix), but nothing tells the user when it is not.

STEADY routing is inconsistent with the other two. It does use the exponential, but only for K1 > 0. The `[POLLUTANTS]` reader explicitly accepts a negative K1 "for growth" ([`landuse.c:118`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/landuse.c#L118)), and KINWAVE and DYNWAVE apply it, but STEADY skips it. A conduit receiving 10 mg/L with K1 = −24/day and a 300 s step should carry 10 e<sup>+24/86400×300</sup> = 10.869 mg/L; all three engines report 10.000.

## Why it happens

`getReactedQual()`, called for every conduit (`findLinkQual()`) and storage unit (`findStorageQual()`) under KINWAVE and DYNWAVE, takes one explicit Euler step of dc/dt = −K1c:

```c
// src/legacy/engine/qualrout.c, getReactedQual()
    if (kDecay == 0.0)
        return c;
    c2 = c * (1.0 - kDecay * tStep);
    c2 = MAX(0.0, c2);
    lossRate = (c - c2) * v1 / tStep;
    massbal_addReactedMass(p, lossRate);
    return c2;
```

Since 1 − x < e<sup>−x</sup> for every x ≠ 0, each step removes more than eq. 5-6 does, and the per-step excess, about (K1Δt)²/2, accumulates to a relative error of roughly K1t × K1Δt / 2 over a time t. With K1Δt ≥ 1 the factor is ≤ 0 and the clamp removes everything in one step.

`findSFLinkQual()` (STEADY) has the exponential, behind a sign test:

```c
// src/legacy/engine/qualrout.c, findSFLinkQual()
        // --- apply first-order decay over travel time
        c2 = c1;
        if (Pollut[p].kDecay > 0.0)
        {
            c2 = c1 * exp(-Pollut[p].kDecay * tStep);
```

6.0.0 reproduces both on purpose (its comments cite `getReactedQual` and `findSFLinkQual`).

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-48_pond-decay-10s.inp`](NUM-48_pond-decay-10s.inp), [`-60s`](NUM-48_pond-decay-60s.inp), [`-300s`](NUM-48_pond-decay-300s.inp) | Closed pond SU1 (1000 ft², 4 ft), P1 100 mg/L, K1 = 24/day, KINWAVE, 6 h, routing step 10, 60 and 300 s |
| [`NUM-48_steady-growth.inp`](NUM-48_steady-growth.inp) | STEADY routing, 1 cfs at 10 mg/L into J1 and through conduit C1, K1 = −24/day, 300 s step |
| [`NUM-48_test.c`](NUM-48_test.c) | Runs the four decks through the legacy toolkit (5.2.4 and 5.3.0) and reads P1 at SU1 / C1 for the last period of the `.out` file |
| [`NUM-48_test6.c`](NUM-48_test6.c) | The same through the 6.0.0 C API (`swmm_node_get_quality()`, `swmm_link_get_quality()` after the last step) |

The test compares with eq. 5-6: 100 e<sup>−6</sup> for the pond and 10 e<sup>K×300 s</sup> for the STEADY conduit. With the exponential the per-step factors multiply to e<sup>−K1t</sup> exactly, so it allows 0.1 %.

```sh
tools/run-test.sh NUM-48            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-48 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix** all three engines print the same:

```
Deck                          Object  P1 at end   Eq. 5-6    Difference
                                      (mg/L)      (mg/L)     (%)
NUM-48_pond-decay-10s.inp     SU1      0.245814    0.247875     -0.83  <-- wrong
NUM-48_pond-decay-60s.inp     SU1      0.235654    0.247875     -4.93  <-- wrong
NUM-48_pond-decay-300s.inp    SU1      0.190206    0.247875    -23.27  <-- wrong
NUM-48_steady-growth.inp      C1      10.000000   10.869040     -8.00  <-- wrong
FAIL: in 4 of 4 decks first-order reaction does not follow exp(-K1 dt): the pond result depends on the routing step and STEADY routing ignores growth
NUM-48 5.2.4 base: FAIL
NUM-48 5.3.0 base: FAIL
NUM-48 6.0.0 base: FAIL
```

**With the fix** both engines give the same values, independent of the step:

```
NUM-48_pond-decay-10s.inp     SU1      0.247875    0.247875     -0.00
NUM-48_pond-decay-60s.inp     SU1      0.247875    0.247875     -0.00
NUM-48_pond-decay-300s.inp    SU1      0.247875    0.247875     -0.00
NUM-48_steady-growth.inp      C1      10.869040   10.869040     -0.00
PASS: first-order decay and growth follow exp(-K1 dt) for every step size and routing method
NUM-48 5.3.0 patched: PASS
...
PASS: first-order decay and growth follow exp(-K1 dt) for every step size and routing method
NUM-48 6.0.0 patched: PASS
```

## The fix

5.3.0:

```diff
-        if (Pollut[p].kDecay > 0.0)
+        if (Pollut[p].kDecay != 0.0)
         {
             c2 = c1 * exp(-Pollut[p].kDecay * tStep);
 ...
-    c2 = c * (1.0 - kDecay * tStep);
-    c2 = MAX(0.0, c2);
+    c2 = c * exp(-kDecay * tStep);
     lossRate = (c - c2) * v1 / tStep;
```

The exponential is never negative, so the clamp goes; a negative K1 gives exact growth, booked as negative Mass Reacted as before. 6.0.0 gets the same three changes in `QualitySolver::mixAtNodes()` and `QualitySolver::updateLinkQuality()`.

Effect on other models, with private patched builds of both engines: EPA's Example1, `events_example.inp` and the 6.0.0 `site_drainage_example.inp` have no decay and give byte-identical `.out` files; the parity deck `steady_quality.inp` (STEADY, K1 = 0.1/day > 0) is byte-identical too. With a decay coefficient the change scales with K1Δt: Example1 converted to DYNWAVE (15 s step) with TSS K1 = 10/day reports External Outflow 405.728 lb instead of 405.723 lb and Mass Reacted 13.579 lb instead of 13.584 lb; the pond decks above change by 0.8 % to 23 %.

6.0.0's unit tests pinned the linear factor in three places, which the 6.0.0 patch updates: `QualityRoutingTest.ExecuteFirstOrderDecay` expected `50 (1 - 0.01 dt)`; the manufactured benchmark `tests/benchmarks/manufactured/quality-cstr-first-order-decay` (used by `QualityCSTR.FirstOrderDecayTrajectory`) was the recurrence `100 x 0.94^n`, documented there as deliberately "NOT the continuous exponential", and is now the exact solution `100 exp(-0.001 t)` (its `definition.md` and `provenance.yaml` are rewritten to match); and `test_reaction_legacy_binding.cpp` used the linear product as the no-reactions baseline. In the last, the parity guard now expects the exponential, and the bracket of `LinkDecayIsNotDoubleApplied` was re-measured with the patch (correct run 0.899 times the no-reactions run, double decay 0.150 times) and set to 0.5 to 1.5.
