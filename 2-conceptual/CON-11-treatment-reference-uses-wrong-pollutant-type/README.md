# CON-11: A pollutant named in a treatment equation is read by the wrong equation's type

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | At a storage unit, a treatment equation that uses another pollutant's concentration reads the inflow concentration where it should read the pond concentration, or the reverse. In the test, `TP C = 0.05*TSS` in a pond holding TSS = 100 mg/L sets TP to 0 instead of 5 mg/L, and `TP R = 0.005*TSS` with TSS-free inflow removes about half of the incoming TP instead of none. Adding or removing an unrelated equation for TSS changes TP's result. No warning. |
| **Reached from** | `[TREATMENT]` at a storage unit (or any node with stored volume) where pollutant A's equation names pollutant B, and B has an equation of the other type, or (5.x) no equation at all |
| **5.3.0** | `getVariableValue()` in [`src/legacy/engine/treatmnt.c:375`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/treatmnt.c#L375) |
| **5.2.4** | Same code, [`src/solver/treatmnt.c:389`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/treatmnt.c#L389) |
| **6.0.0** | Reproduces, except for an untreated referenced pollutant. `applyNodeTreatment()` copies the legacy rule on purpose ([`src/engine/quality/QualityRouting.cpp:1553`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1553)) but treats a pollutant with no equation as concentration-type. |
| **Since** | 5.1.008 (commit c70dd4fe, 2015). Before it, the type came from the equation being evaluated (a module-wide `Treatment` pointer); the 5.1.008 fix for recursive treatment calls replaced it with the referenced pollutant's treatment. |
| **Fix** | Choose by the type of the equation being evaluated: [`CON-11_swmm530.patch`](CON-11_swmm530.patch), [`CON-11_swmm600.patch`](CON-11_swmm600.patch) |

## The problem

A treatment equation can use the concentration of any pollutant. The reference manual (Vol. III, section 5.4.2) says what that concentration is:

> Note that if treatment is made a function of pollutant concentrations, then for concentration-based treatment these represent the concentrations at the node prior to treatment while for removal-based functions they are the concentrations in the node's combined influent stream. If the node has no volume (e.g., is a non-storage node) then these two types of concentrations are equivalent.

So `TP C = 0.05*TSS` uses the TSS in the pond, and `TP R = 0.005*TSS` uses the TSS in the water flowing in. 5.2.4 and 5.3.0 instead decide by the equation of the pollutant that is named, here TSS. If TSS has a removal-type equation, or none, `TSS` means its inflow concentration in every equation at that node; if TSS has a concentration-type equation, it means the pond concentration in every equation.

At a storage unit the two values can be far apart. A pond that is not filling has an inflow concentration of 0, so a concentration-type equation that refers to an untreated pollutant evaluates to 0: in the test, `TP C = 0.05*TSS` sets TP from 10 to 0 mg/L in one step instead of to 0.05 x 100 = 5, and books all stored TP as reacted. Giving TSS a concentration-type equation, even the no-op `TSS C = TSS`, makes the same TP equation give 5; giving it a removal-type one keeps 0. The other way round, `TP R = 0.005*TSS` with TSS-free inflow should remove nothing, but because TSS has a concentration-type equation it reads the pond's TSS (100 mg/L at the start, falling as the pond fills) and removes up to half of the incoming TP; TP ends at 7.37 mg/L instead of 10.

6.0.0 treats an untreated pollutant as concentration-type, which fixes the first case, but keeps the rule for pollutants that have an equation, so the other two still give 0 and 7.37 mg/L.

## Why it happens

`getVariableValue()` answers for a pollutant name by looking at that pollutant's treatment record, not at the equation being evaluated:

```c
// src/legacy/engine/treatmnt.c, getVariableValue()
    // --- variable is a pollutant concentration
    else if ( varCode < PVMAX + Nobjects[POLLUT] )
    {
        p = varCode - PVMAX;
        treatment = &Node[J].treatment[p];                  // the REFERENCED pollutant
        if ( treatment->treatType == REMOVAL ) return Cin[p];
        return Node[J].newQual[p];
    }
```

The treatment records are allocated with `calloc`, and `REMOVAL` is the first value of `enum TreatmentType` (0), so a pollutant without an equation counts as removal-type and always reads as `Cin`.

Up to 5.1.007 the test was `Treatment->treatType`, where `Treatment` was a module-wide pointer set to the pollutant whose equation was being evaluated, which is the manual's rule. 5.1.008 removed that pointer to fix recursive `R_` evaluations ("A bug in evaluating recursive calls to treatment functions was fixed") and wrote `&Node[J].treatment[p]` here, with `p` now the referenced pollutant.

6.0.0 builds the same choice into an array once per node and documents it as the legacy behaviour:

```cpp
// src/engine/quality/QualityRouting.cpp, applyNodeTreatment()
            const bool p_is_removal =
                ci < treat.compiled.size() && treat.compiled[ci].is_removal;
            cpollut[up2] = p_is_removal
                ? treat.cin[up2]
                : ((ci < nodes.conc.size()) ? nodes.conc[ci] : 0.0);
```

## How to reproduce

All three decks have a storage unit SU1 (1000 ft2, 4 ft deep, no outflow) holding TSS = 100 mg/L and TP = 10 mg/L, run for 1 hour.

| File | What it is |
|---|---|
| [`CON-11_ctype-untreated-ref.inp`](CON-11_ctype-untreated-ref.inp) | No inflow. `TP C = 0.05*TSS`, TSS untreated. Expected TP = MIN(10, 0.05 x 100) = 5 mg/L |
| [`CON-11_ctype-rtype-ref.inp`](CON-11_ctype-rtype-ref.inp) | As above plus `TSS R = 0.5` (inactive without inflow). Expected TP = 5 mg/L |
| [`CON-11_rtype-ctype-ref.inp`](CON-11_rtype-ctype-ref.inp) | 1 cfs DWF with TP = 10 mg/L and no TSS. `TP R = 0.005*TSS`, `TSS C = TSS`. Inflow TSS is 0, so the removal is 0. Expected TP = 10 mg/L |
| [`CON-11_test.c`](CON-11_test.c) | Runs the three decks (5.2.4, 5.3.0) and reads TP in SU1 at the last reporting period of the `.out` file |
| [`CON-11_test6.c`](CON-11_test6.c) | The same through the 6.0.0 API (`swmm_node_get_quality()` at the end of the run) |
| [`CON-11_swmm530.patch`](CON-11_swmm530.patch), [`CON-11_swmm600.patch`](CON-11_swmm600.patch) | The fixes |

```sh
tools/run-test.sh CON-11            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-11 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix:**

```
---- CON-11 on 5.3.0 (base) ----
Deck                            TSS eqn  TP eqn         TP expected  TP computed
CON-11_ctype-untreated-ref.inp  (none)   C = 0.05*TSS       5.00       0.00  <-- wrong
CON-11_ctype-rtype-ref.inp      R = 0.5  C = 0.05*TSS       5.00       0.00  <-- wrong
CON-11_rtype-ctype-ref.inp      C = TSS  R = 0.005*TSS     10.00       7.37  <-- wrong
FAIL: TP in SU1 is wrong in 3 of 3 decks: TSS in TP's equation is read by TSS's treatment type, not TP's
---- CON-11 on 6.0.0 (base) ----
Deck                            TSS eqn  TP eqn         TP expected  TP computed
CON-11_ctype-untreated-ref.inp  (none)   C = 0.05*TSS       5.00       5.00
CON-11_ctype-rtype-ref.inp      R = 0.5  C = 0.05*TSS       5.00       0.00  <-- wrong
CON-11_rtype-ctype-ref.inp      C = TSS  R = 0.005*TSS     10.00       7.37  <-- wrong
FAIL: TP in SU1 is wrong in 2 of 3 decks: TSS in TP's equation is read by TSS's treatment type, not TP's
```

5.2.4 prints the same except 7.02 for the third deck: its `.out` file ends one reporting period earlier (0:45 instead of 1:00).

**With the fix:**

```
---- CON-11 on 5.3.0 (patched) ----
Deck                            TSS eqn  TP eqn         TP expected  TP computed
CON-11_ctype-untreated-ref.inp  (none)   C = 0.05*TSS       5.00       5.00
CON-11_ctype-rtype-ref.inp      R = 0.5  C = 0.05*TSS       5.00       5.00
CON-11_rtype-ctype-ref.inp      C = TSS  R = 0.005*TSS     10.00      10.00
PASS: TSS in TP's equation is the pond concentration in a C = equation and the inflow concentration in an R = equation
```

6.0.0 patched prints the same numbers.

## The fix

5.3.0: `getRemoval()` records the type of the equation it is about to evaluate in a new module variable `TreatType`, and `getVariableValue()` uses it. The previous value is restored after the evaluation, because an `R_<pollutant>` reference evaluates another pollutant's equation in the middle of this one.

```diff
     // --- apply treatment eqn.
     treatment = &Node[J].treatment[p];
+    TreatType = treatment->treatType;
     r = mathexpr_eval(treatment->equation, getVariableValue);
+    TreatType = treatType;
```
```diff
         p = varCode - PVMAX;
-        treatment = &Node[J].treatment[p];
-        if ( treatment->treatType == REMOVAL ) return Cin[p];
+        if ( TreatType == REMOVAL ) return Cin[p];
         return Node[J].newQual[p];
```

6.0.0: fill `cpollut` with the node concentrations and pass the inflow concentrations instead when the equation being evaluated is a removal equation.

```diff
                 treat.cin.data(), r_view.data(), np, area,
-                cpollut.data());
+                expr.is_removal ? treat.cin.data() : cpollut.data());
```

A pollutant's reference to itself (`TSS R = 0.1*TSS`) and every equation that names no other pollutant give the same result as before, because there the two types agree. At a node without stored volume the inflow and node concentrations are equal, so only nodes with volume (storage units, and dynamic-wave junctions holding water) change. **Effect on other models:** none of the 73 regression decks has a `[TREATMENT]` section.
