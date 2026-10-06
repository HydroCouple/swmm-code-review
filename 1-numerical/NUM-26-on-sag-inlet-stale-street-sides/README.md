# NUM-26: An on-sag inlet is scaled by the street sides of the inlet processed before it

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | An on-sag inlet on a half street (Sides = 1) captures twice its capacity when the inlet processed before it sits on a full street (Sides = 2). The reverse halves a full-street sag inlet. Which case applies depends on the order of the `[INLET_USAGE]` rows and on which other inlets have flow. In EPA's Example 7 with inlets, the two-inlet half-street sag inlet on Street5 is doubled: fixing it lowers its peak capture from 92.7% to 74.6% and raises its peak bypass from 2.0 to 6.9 cfs. No warning. |
| **Reached from** | A model with on-sag inlets (placement ON_SAG, or AUTOMATIC at a dead-end node) and streets with different `Sides` values |
| **5.3.0** | `getOnSagCapturedFlow()` in [`src/legacy/engine/inlet.c:1587`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inlet.c#L1587) |
| **5.2.4** | Same code, [`src/solver/inlet.c:1587`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/inlet.c#L1587) |
| **6.0.0** | Reproduces on purpose through `InletSolver::nsides_prev_` ([`src/engine/hydraulics/Inlet.hpp:367`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Inlet.hpp#L367), [`Inlet.cpp:997`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Inlet.cpp#L997)) |
| **Since** | 5.2.0, when inlets were added |
| **Fix** | Read the street sides after loading the inlet's geometry: [`NUM-26_swmm530.patch`](NUM-26_swmm530.patch), [`NUM-26_swmm600.patch`](NUM-26_swmm600.patch) |

## The problem

An on-sag inlet captures flow according to the water depth at its node (weir or orifice flow through the grate or curb opening). For a street with two sides, SWMM places the inlet on both sides and doubles the capture. The number of sides it uses, though, is that of whichever inlet was evaluated just before, not that of the inlet's own street.

The test deck has two unconnected streets. Street H is a half street ending in a 2 ft x 2 ft grate placed on sag; street F is a full street with the same grate placed on grade. With 6 cfs on street H, the sag grate sits in 0.224 ft of water. One grate captures 1.66 cfs at that depth, but SWMM removes 3.32 cfs from the street, exactly twice as much. With the fix, the depth is 0.258 ft and the grate captures 2.09 cfs, which is what one grate takes at that depth.

Deleting street F's inlet from the model, which has nothing to do with street H, also gives the correct result.

## Why it happens

The inlet code keeps the street geometry of the inlet being evaluated in file-scope variables that `getConduitGeometry()` fills in. `getOnSagCapturedFlow()` uses one of them, `Nsides`, before it calls that function:

```c
// src/legacy/engine/inlet.c, getOnSagCapturedFlow()
if (inlet->numInlets == 0) return 0.0;
totalInlets = Nsides * inlet->numInlets;     // line 1587: Nsides of the previous inlet
linkIndex = inlet->linkIndex;
designIndex = inlet->designIndex;

// --- store conduit geometry in shared variables
getConduitGeometry(inlet);                   // line 1592: sets Nsides for this inlet
...
qCaptured *= (double)totalInlets;
```

`getConduitGeometry()` sets `Nsides = Street[t].sides` (line 1170), or 1 for a non-street conduit (line 1192). `inlet_findCapturedFlows()` evaluates the inlets in list order, which is the reverse of the `[INLET_USAGE]` rows, and the value carries over from one routing step to the next. The first inlet of a step therefore uses the sides of the last inlet of the previous step, and at the very first step the value left by `inlet_validate()`. An on-grade inlet whose street is dry returns before calling `getConduitGeometry()` and leaves `Nsides` unchanged, so the result can also depend on which streets have flow.

6.0.0 noticed the defect and kept it to match 5.3.0. `nsides_prev_` holds "the street sides of the PREVIOUSLY processed inlet", seeded from the first `[INLET_USAGE]` row; the comment names Example 7's Street5 as a case it affects.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-26_half-and-full-street.inp`](NUM-26_half-and-full-street.inp) | Half street H with an ON_SAG grate (6 cfs) and an unconnected full street F with an ON_GRADE grate (4 cfs) |
| [`NUM-26_test.c`](NUM-26_test.c) | Legacy toolkit (5.2.4, 5.3.0). At steady state, compares the capture at H (drop in street flow across the inlet) with the HEC-22 weir/orifice capacity of one grate at the depth at the inlet |
| [`NUM-26_test6.c`](NUM-26_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-26            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-26 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints 0.2243 ft, 2.675 and 3.325 cfs; 6.0.0 prints the 5.3.0 values):

```
Half street H, one grate on sag (end of run):
  depth at inlet BH            0.2242 ft
  street flow H1 / H2           6.000 / 2.678 cfs
  captured flow                 3.322 cfs
  one grate at that depth       1.661 cfs (HEC-22)
  captured / one grate          2.000   (correct: 1.000)
FAIL: the one-sided sag inlet captures 3.322 cfs, 2.00 times what one grate takes at 0.224 ft (1.661 cfs)
NUM-26 5.2.4 base: FAIL
NUM-26 5.3.0 base: FAIL
NUM-26 6.0.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same values):

```
Half street H, one grate on sag (end of run):
  depth at inlet BH            0.2583 ft
  street flow H1 / H2           6.000 / 3.906 cfs
  captured flow                 2.094 cfs
  one grate at that depth       2.094 cfs (HEC-22)
  captured / one grate          1.000   (correct: 1.000)
PASS: the one-sided sag inlet captures what one grate takes at its depth (ratio 1.000)
NUM-26 5.3.0 patched: PASS
NUM-26 6.0.0 patched: PASS
```

## The fix

Compute `totalInlets` after `getConduitGeometry()`:

```diff
     if (inlet->numInlets == 0) return 0.0;
-    totalInlets = Nsides * inlet->numInlets;
     linkIndex = inlet->linkIndex;
     designIndex = inlet->designIndex;
 
     // --- store conduit geometry in shared variables
     getConduitGeometry(inlet);
+    totalInlets = Nsides * inlet->numInlets;
```

The 6.0.0 patch passes the inlet's own `g.nsides` to `getOnSagCapturedFlow()` and removes `nsides_prev_`, which had no other use.

**Effect on other models.** Of the eleven inlet decks in the regression suite, only `update_v52/Example7-Inlets.inp` changes, in both engines. Its Street5 is a half street with two inlets on sag, evaluated right after Street1, a full street. The Street Flow Summary for Street5 changes as follows:

| Street5 | unpatched | patched |
|---|---|---|
| Peak flow (cfs) | 27.237 | 26.999 |
| Maximum depth (ft) | 0.632 | 0.726 |
| Peak flow capture (%) | 92.67 | 74.58 |
| Peak capture per inlet (cfs) | 12.62 | 10.07 |
| Peak bypass flow (cfs) | 2.00 | 6.86 |

The other streets change in the second or third decimal. The routing continuity error goes from 3.639% to 3.636% (most of it is NUM-02).
