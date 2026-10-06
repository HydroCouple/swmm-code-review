# CON-12: ERROR 161 for a cyclic treatment dependency can never be reported

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | A `[TREATMENT]` set whose removals depend on each other runs with no error or warning. Every removal in the cycle is set to 0, so those pollutants are not treated at all, and pollutants evaluated after the cycle at the same node are skipped or, in 5.x, doubled ([NUM-49](../../1-numerical/NUM-49-treatment-latch-leaves-minus-one-removal/)). The manual documents ERROR 161 for this input. |
| **Reached from** | `[TREATMENT]` at a node where a removal depends on itself, directly (`TN R = 0.5*R_TN`) or through other pollutants (`BOD5 R = 0.5*R_TN`, `TN R = 0.5*R_BOD5`), with the pollutant present at the node |
| **5.3.0** | `getRemoval()` in [`src/legacy/engine/treatmnt.c:404`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/treatmnt.c#L404) sets `ErrCode = 1`; `treatmnt_treat()` at [`:229`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/treatmnt.c#L229) tests it against 161 |
| **5.2.4** | Same code, [`src/solver/treatmnt.c:418`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/treatmnt.c#L418) and [`:243`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/treatmnt.c#L243) |
| **6.0.0** | Partly fixed. `SWMMEngine::initQuality()` checks each node's dependency graph at load and reports ERROR 161 for a cycle between pollutants, but drops a pollutant's reference to its own removal ([`src/engine/core/SWMMEngine.cpp:9252`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L9252)), so that cycle runs. At run time it copies the legacy latch on purpose ([`QualityRouting.cpp:1565-1581`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1565-L1581)). |
| **Since** | The oldest code on GitHub (5.0.017-era `treatmnt.c`, 2014 initial commit), so every 5.x release |
| **Fix** | Set the error code the caller tests for (5.3.0); keep self-references in the cycle check (6.0.0): [`CON-12_swmm530.patch`](CON-12_swmm530.patch), [`CON-12_swmm600.patch`](CON-12_swmm600.patch) |

## The problem

Treatment equations can use the removal of another pollutant (`R_TN`). If removals depend on each other in a loop, there is no value to compute, and the manual (Appendix A) lists the error for it:

> ERROR 161: cyclic dependency in treatment functions at Node xxx.
> An example would be where the removal of pollutant 1 is defined as a function of the removal of pollutant 2 while the removal of pollutant 2 is defined as a function of the removal of pollutant 1.

Neither 5.2.4 nor 5.3.0 ever reports it. With the manual's own example at junction J1:

```
J1  BOD5  R = 0.5*R_TN
J1  TN    R = 0.5*R_BOD5
```

and 10 mg/L of both entering J1, the run completes with error code 0. Both removals are evaluated as 0.5*0 = 0, so the report shows `Mass Reacted 0.000` for both pollutants, and nothing tells the user that their treatment was ignored. The same happens when a removal refers to itself (`TN R = 0.5*R_TN`).

6.0.0 catches the two-pollutant cycle when it loads the model but not the self-reference.

## Why it happens

`getRemoval()` marks a removal it is evaluating with `R[p] = 10`. When an equation reaches a removal still marked that way, it has found a cycle and sets a flag:

```c
// src/legacy/engine/treatmnt.c, getRemoval()
    // --- case where removal already being computed for another pollutant
    if ( R[p] > 1.0 || ErrCode )
    {
        ErrCode = 1;
        return 0.0;
    }
```

After all removals are evaluated, `treatmnt_treat()` checks for the cycle error, but against a different value:

```c
// src/legacy/engine/treatmnt.c, treatmnt_treat()
    // --- check for error condition
    if ( ErrCode == ERR_CYCLIC_TREATMENT )                  // 161, never equal to 1
    {
         report_writeErrorMsg(ERR_CYCLIC_TREATMENT, Node[J].ID);
    }

    // --- update nodal concentrations and mass balances
    else for ( p = 0; p < Nobjects[POLLUT]; p++ )
```

`ErrCode` is only ever 0 or 1, so the error message is never written and the apply pass always runs, with the cyclic removals at 0 and the removals of later pollutants left at their "not computed" value -1 ([NUM-49](../../1-numerical/NUM-49-treatment-latch-leaves-minus-one-removal/)).

6.0.0 added a load-time check that builds each node's `R_<pollutant>` dependency graph and searches it for a cycle, but leaves out self-references:

```cpp
// src/engine/core/SWMMEngine.cpp, SWMMEngine::initQuality()
                    if (tok.type == treatment::TokenType::VARIABLE
                        && tok.var == treatment::TreatVar::R_POLLUT
                        && tok.pollut_ref >= 0 && tok.pollut_ref < np
                        && tok.pollut_ref != p) {
```

A removal that depends on itself is the shortest cycle, and legacy `getRemoval()` detects it the same way as a longer one.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-12_self-reference.inp`](CON-12_self-reference.inp) | J1 with 1 cfs DWF at TN = 10 mg/L and `TN R = 0.5*R_TN` |
| [`CON-12_mutual.inp`](CON-12_mutual.inp) | J1 with 1 cfs DWF at BOD5 = TN = 10 mg/L, `BOD5 R = 0.5*R_TN`, `TN R = 0.5*R_BOD5` (the manual's example) |
| [`CON-12_test.c`](CON-12_test.c) | Runs both decks (5.2.4, 5.3.0) and expects error code 161 |
| [`CON-12_test6.c`](CON-12_test6.c) | Runs both decks through the 6.0.0 API and expects an error whose message is ERROR 161 |
| [`CON-12_swmm530.patch`](CON-12_swmm530.patch), [`CON-12_swmm600.patch`](CON-12_swmm600.patch) | The fixes |

```sh
tools/run-test.sh CON-12            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh CON-12 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same as 5.3.0):

```
---- CON-12 on 5.3.0 (base) ----
Deck                         Error code  Message
CON-12_self-reference.inp             0  (run completed)  <-- no ERROR 161
CON-12_mutual.inp                     0  (run completed)  <-- no ERROR 161
FAIL: 2 of 2 decks with a cyclic treatment dependency run without ERROR 161
CON-12 5.3.0 base: FAIL
---- CON-12 on 6.0.0 (base) ----
Deck                         Error code  Message
CON-12_self-reference.inp             0  (run completed)  <-- no ERROR 161
CON-12_mutual.inp                     5    ERROR 161: cyclic dependency in treatment functions at node J1.
FAIL: 1 of 2 decks with a cyclic treatment dependency run without ERROR 161
CON-12 6.0.0 base: FAIL
```

**With the fix:**

```
---- CON-12 on 5.3.0 (patched) ----
Deck                         Error code  Message
CON-12_self-reference.inp           161     ERROR 161: cyclic dependency in treatment functions at node J1.
CON-12_mutual.inp                   161     ERROR 161: cyclic dependency in treatment functions at node J1.
PASS: both cyclic treatment dependencies stop the run with ERROR 161
CON-12 5.3.0 patched: PASS
---- CON-12 on 6.0.0 (patched) ----
Deck                         Error code  Message
CON-12_self-reference.inp             5    ERROR 161: cyclic dependency in treatment functions at node J1.
CON-12_mutual.inp                     5    ERROR 161: cyclic dependency in treatment functions at node J1.
PASS: both cyclic treatment dependencies stop the run with ERROR 161
CON-12 6.0.0 patched: PASS
```

6.0.0 returns its own code (5, `SWMM_ERR_PARSE`) with the legacy message.

## The fix

5.3.0: set the error code that `treatmnt_treat()` tests for. It then writes ERROR 161 to the report, skips the apply pass for that node, and the run stops with error 161 at the end of the step.

```diff
     if ( R[p] > 1.0 || ErrCode )
     {
-        ErrCode = 1;
+        ErrCode = ERR_CYCLIC_TREATMENT;
         return 0.0;
     }
```

6.0.0: keep a pollutant's reference to its own removal in the dependency graph; the existing depth-first search then finds the self-loop.

```diff
                         && tok.var == treatment::TreatVar::R_POLLUT
-                        && tok.pollut_ref >= 0 && tok.pollut_ref < np
-                        && tok.pollut_ref != p) {
+                        && tok.pollut_ref >= 0 && tok.pollut_ref < np) {
```

The two engines detect the cycle at different times. 6.0.0 checks the input when the model is loaded, so every cyclic deck stops before the run. 5.3.0 finds the cycle only when it evaluates the equations, which needs the pollutant to be present at the node (and, for a removal-type equation, inflow), so a cycle at a node that never sees the pollutant still runs. Both decks here stop in both engines.

Users with a cyclic treatment, which was silently ignored, now get ERROR 161 and have to rewrite the equations. **Effect on other models:** none of the 73 regression decks has a `[TREATMENT]` section.
