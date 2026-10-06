# NUM-49: After a cyclic treatment, later pollutants at the node get a removal of -100 % and their concentration doubles

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Wrong results with no warning. At a node whose treatment contains a cycle, every pollutant evaluated after the cycle with a concentration-type equation leaves the node at twice its concentration. The created mass is not booked anywhere, so the quality continuity error goes to about -100 %, which is the only sign. |
| **Reached from** | `[TREATMENT]` at a node where one pollutant's removal depends on itself, directly (`R = 0.5*R_TN`) or through another pollutant, and a later pollutant (in `[POLLUTANTS]` order) has a `C = ...` equation |
| **5.3.0** | `treatmnt_treat()` in [`src/legacy/engine/treatmnt.c:237`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/treatmnt.c#L237), with the latch in `getRemoval()` at [`:402-406`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/treatmnt.c#L402-L406) |
| **5.2.4** | Same code, [`src/solver/treatmnt.c:251`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/treatmnt.c#L251) and [`:416-420`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/treatmnt.c#L416-L420) |
| **6.0.0** | Not affected: `applyNodeTreatment()` copies the latch but skips every removal `<= 0` ([`src/engine/quality/QualityRouting.cpp:1672`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L1672)), so the later pollutants are left untreated rather than doubled |
| **Since** | The oldest code on GitHub (5.0.017-era `treatmnt.c`, 2014 initial commit), so every 5.x release |
| **Fix** | Skip removals `<= 0` instead of `== 0`: [`NUM-49_swmm530.patch`](NUM-49_swmm530.patch). [CON-12](../../2-conceptual/CON-12-cyclic-treatment-error-unreachable/) makes the cycle itself an error. |

## The problem

Junction J1 takes 1 cfs carrying TN and TSS at 10 mg/L each. Its treatment is:

```
J1  TN   R = 0.5*R_TN
J1  TSS  C = 0.9*TSS
```

TN's removal refers to itself, which the manual calls a cyclic dependency (ERROR 161). TSS's equation is independent of TN and asks for 9 mg/L. In 5.2.4 and 5.3.0 the run completes with no error or warning, TSS leaves J1 at 20 mg/L, and the outfall receives 17.86 lb of TSS for 8.97 lb that entered:

```
  Quality Routing Continuity           lbs           lbs
  Dry Weather Inflow .......         8.974         8.974
  External Outflow .........         8.930        17.861
  Mass Reacted .............         0.000         0.000
  Continuity Error (%) .....        -0.073      -100.147
```

Treatment can only remove mass. Any concentration-type pollutant listed after the cyclic one at the same node is doubled at every routing step, whatever its own equation says.

## Why it happens

`treatmnt_treat()` marks every removal as "not computed" with -1, then asks `getRemoval()` for each pollutant that has an equation:

```c
// src/legacy/engine/treatmnt.c, treatmnt_treat()
    for ( p = 0; p < Nobjects[POLLUT]; p++) R[p] = -1.0;
    for ( p = 0; p < Nobjects[POLLUT]; p++)
    {
        treatment = &Node[j].treatment[p];
        if ( treatment->equation == NULL ) R[p] = 0.0;
        else if ( treatment->treatType == REMOVAL && q <= ZERO ) R[p] = 0.0;
        else getRemoval(p);
    }
```

`getRemoval()` sets `R[p] = 10` while a removal is being evaluated. When an equation reaches a removal that is still being evaluated, it latches a node-wide flag, and from then on every call returns 0 at once, without assigning `R[p]`:

```c
// src/legacy/engine/treatmnt.c, getRemoval()
    if ( R[p] > 1.0 || ErrCode )
    {
        ErrCode = 1;
        return 0.0;
    }
```

So TN gets `R = 0.5*0 = 0`, and TSS, evaluated next, returns at the latch with `R[TSS]` still -1. The latch was meant to stop the apply pass ([CON-12](../../2-conceptual/CON-12-cyclic-treatment-error-unreachable/): it compares `ErrCode` with 161 and never matches), so the apply pass runs and only skips removals that are exactly zero:

```c
// src/legacy/engine/treatmnt.c, treatmnt_treat()
        if ( R[p] == 0.0 ) continue;
        ...
            cOut = (1.0 - R[p]) * Node[j].newQual[p];      // (1 - (-1)) * C = 2*C
        ...
        massLost = MAX(0.0, massLost);                      // negative loss dropped
        massbal_addReactedMass(p, massLost);
        Node[j].newQual[p] = cOut;
```

The mass loss is negative and is clamped to zero, so the created mass appears only as a continuity error. A removal-type equation in the same position gives `MIN(2*Cin, C)`, which does not raise the concentration but does not apply the equation either.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-49_cyclic-then-ctype.inp`](NUM-49_cyclic-then-ctype.inp) | J1 with 1 cfs DWF at TN = TSS = 10 mg/L, `TN R = 0.5*R_TN`, `TSS C = 0.9*TSS`, KINWAVE, 4 h |
| [`NUM-49_test.c`](NUM-49_test.c) | Runs the deck (5.2.4, 5.3.0), reads TSS at J1 and O1 from the `.out` and the quality continuity error |
| [`NUM-49_test6.c`](NUM-49_test6.c) | The same check through the 6.0.0 API |
| [`NUM-49_swmm530.patch`](NUM-49_swmm530.patch) | The fix for 5.3.0 |

The tests pass if TSS stays at or below its 10 mg/L inflow concentration and quality continuity holds, or if the run stops with ERROR 161 (the outcome once [CON-12](../../2-conceptual/CON-12-cyclic-treatment-error-unreachable/) is fixed).

```sh
tools/run-test.sh NUM-49            # 5.2.4 and 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh NUM-49 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same):

```
---- NUM-49 on 5.3.0 (base) ----
TSS inflow concentration      10.00 mg/L
TSS at J1 (end of run)        20.00 mg/L
TSS at outfall O1             20.00 mg/L
Quality continuity error     -100.15 %
FAIL: treatment raised TSS from 10 to 20.00 mg/L (removal R = -1 applied) and created mass: quality continuity error -100.15 %
NUM-49 5.3.0 base: FAIL
---- NUM-49 on 6.0.0 (base) ----
TSS inflow concentration      10.00 mg/L
TSS at J1 (end of run)        10.00 mg/L
TSS at outfall O1             10.00 mg/L
TSS quality continuity error  -0.08 %
PASS: TSS is not raised above its inflow concentration and quality continuity holds
NUM-49 6.0.0 base: PASS
```

**With the fix:**

```
---- NUM-49 on 5.3.0 (patched) ----
TSS inflow concentration      10.00 mg/L
TSS at J1 (end of run)        10.00 mg/L
TSS at outfall O1             10.00 mg/L
Quality continuity error      -0.07 %
PASS: TSS is not raised above its inflow concentration and quality continuity holds
NUM-49 5.3.0 patched: PASS
```

## The fix

Skip a removal that was never computed, as 6.0.0 does:

```diff
-        if ( R[p] == 0.0 ) continue;
+        // --- skip R[p] = -1 too (removal never computed)
+        if ( R[p] <= 0.0 ) continue;
```

With this patch alone, a deck with a cycle still runs without an error, and the pollutants after the cycle are left untreated (TSS stays at 10 mg/L, not the 9 mg/L its equation asks for), which is what 6.0.0 does today. [CON-12](../../2-conceptual/CON-12-cyclic-treatment-error-unreachable/) reports the cycle as ERROR 161 and stops the run, as the manual describes; with that patch applied the apply pass no longer runs after a latch, and this one is a guard that keeps an unset removal from ever being applied.

**Effect on other models:** none without a cycle. Every `R[p]` is then assigned a value in [0, 1] before the apply pass, so `<= 0` and `== 0` skip the same pollutants. None of the 73 regression decks has a `[TREATMENT]` section.
