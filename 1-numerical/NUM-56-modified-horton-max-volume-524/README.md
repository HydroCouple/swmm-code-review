# NUM-56: 5.2.4's Modified Horton stops infiltrating after one step when Fmax is set

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | In 5.2.4, any MODIFIED_HORTON subcatchment that supplies the optional maximum infiltration volume Fmax infiltrates during the first wet time step of an event and then not at all, however large Fmax is. With 1 in of rain and Fmax = 5 in, infiltration is 0.08 in instead of 1.00 in and 0.83 in runs off instead of none. No warning. |
| **Reached from** | `INFILTRATION MODIFIED_HORTON` with a non-zero fifth value (Fmax) in `[INFILTRATION]` |
| **5.3.0** | Fixed: Fmax caps a cumulative infiltration volume and `Fe` is limited with `MIN`, [`src/legacy/engine/infil.c:556`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/infil.c#L556) and [`:563`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/infil.c#L563) |
| **5.2.4** | `modHorton_getInfil()` sets `Fe = MAX(Fe, Fmax)`, [`src/solver/infil.c:545`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/infil.c#L545) |
| **6.0.0** | Fixed, same as 5.3.0: [`src/engine/hydrology/Infiltration.cpp:196`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Infiltration.cpp#L196) |
| **Since** | 5.1.000. EPA fixed it in August 2020 (commit f2df1d26, issue #59), but the 5.2.0 release commit (930e74ed) brought the old line back, so 5.2.0 to 5.2.4 and EPA's `develop` branch still have it. The fork re-applied the fix in commit 353f8fe2 (December 2023), which is in 5.3.0 |
| **Fix** | None needed for 5.3.0 and 6.0.0 |

## The problem

The test deck has two identical pervious subcatchments with f0 = 3 in/hr, fmin = 0.5 in/hr and a decay constant of 4/hr. 1 in of rain falls at 1 in/hr. S1 has Fmax = 5 in, S2 has none. The capacity is max(3 - 4 F<sub>e</sub>, 0.5), and F<sub>e</sub> grows at f - fmin = 0.5 in/hr, so after the hour of rain the capacity is still 1.0 in/hr. All 1.00 in infiltrates on both subcatchments and nothing runs off. An Fmax of 5 in, more than the whole storm, cannot change that.

5.2.4 reports:

| Subcatchment | Fmax | Infiltration (in) | Runoff (in) |
|---|---|---|---|
| S1 | 5 in | 0.08 | 0.83 |
| S2 | none | 1.00 | 0.00 |

S1 infiltrates for one 5-minute step (1 in/hr x 5 min = 0.08 in) and then stops. Because only dry steps lower F<sub>e</sub> again, every later event on S1 also infiltrates for a single step.

## Why it happens

```c
// src/solver/infil.c (5.2.4), modHorton_getInfil()
        // --- saturated condition
        if ( infil->Fmax > 0.0 && infil->Fe >= infil->Fmax ) return 0.0;
        ...
        // --- new cumulative infiltration minus seepage
        infil->Fe += MAX((f - fmin), 0.0) * tstep;
        if ( infil->Fmax > 0.0 ) infil->Fe = MAX(infil->Fe, infil->Fmax);
```

The intent is to keep F<sub>e</sub> from exceeding Fmax; Vol I sec. 4.3.3 step 5 writes it as F<sub>e</sub> ← min(F<sub>e</sub> + (f - f<sub>∞</sub>)Δt, F<sub>max</sub>). `MAX` instead sets F<sub>e</sub> to Fmax on the first wet step, and the "saturated condition" test then returns 0 on every following wet step.

5.3.0 and 6.0.0 carry EPA's 2020 fix: they limit a separate accumulator of total infiltration, `Fmh`, to Fmax (cutting the last step's rate so the total reaches Fmax exactly), decay it in dry steps like F<sub>e</sub>, and limit F<sub>e</sub> with `MIN`. That caps total infiltration at Fmax, while Vol I caps only the excess volume F<sub>e</sub>. When Fmax binds, the two readings give different results, but neither stops infiltration early. The input reference describes Fmax as the "maximum infiltration volume possible", which matches the fixed code.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-56_fmax-not-binding.inp`](NUM-56_fmax-not-binding.inp) | S1 (Fmax 5 in) and S2 (no Fmax), 1 in of rain, MODIFIED_HORTON |
| [`NUM-56_test.c`](NUM-56_test.c) | Runs the deck through the legacy toolkit and checks Total Infil of both subcatchments in the report's Subcatchment Runoff Summary against the analytic 1.00 in, within 0.01 in |
| [`NUM-56_test6.c`](NUM-56_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-56            # 5.2.4: FAIL; 5.3.0, 6.0.0: PASS
```

5.2.4:

```
Subcatchment  Fmax (in)  Infiltration (in)  Runoff (in)
S1            5.0                     0.08         0.83
S2            none                    1.00         0.00
Expected for both                     1.00         0.00
FAIL: an Fmax of 5 in that cannot bind cuts infiltration to 0.08 in (expected 1.00 in, 1.00 in without Fmax)
NUM-56 5.2.4 base: FAIL
```

5.3.0 and 6.0.0:

```
Subcatchment  Fmax (in)  Infiltration (in)  Runoff (in)
S1            5.0                     1.00         0.00
S2            none                    1.00         0.00
Expected for both                     1.00         0.00
PASS: with Fmax = 5 in all 1.00 in of rain infiltrates, as without Fmax
NUM-56 5.3.0 base: PASS
NUM-56 6.0.0 base: PASS
```

## The fix

No patch: 5.3.0 and 6.0.0 are already fixed, and 5.2.4 is not patched in this review. For 5.2.4 users, the one-line correction in Vol I's terms is `infil->Fe = MIN(infil->Fe, infil->Fmax);`; the EPA fix of commit f2df1d26 (the 5.3.0 code) is the more complete one. Models that rely on Fmax with MODIFIED_HORTON give very different runoff in 5.2.4 than in 5.3.0, and 5.3.0 is the correct one. Hot start files do not yet carry 5.3.0's new `Fmh` state (IO-44).

The constant-rate branch of both Horton methods ignores Fmax altogether in every version; that is NUM-33.
