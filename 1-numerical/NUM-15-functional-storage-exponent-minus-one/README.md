# NUM-15: A FUNCTIONAL storage unit with exponent -1 or less is accepted and gets an infinite or meaningless volume

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | With A2 = -1 every volume of the unit is `inf`: the report shows average and maximum volume `inf`, percent full `-nan` and a flow routing continuity error of `-nan`, and the run still ends normally. With A2 < -1 the run looks normal but the volume is a finite number for a basin that would hold infinitely much (50 ft³ at 1 ft in the test, from a formula that is negative below 0.71 ft). No input error or warning in either case. |
| **Reached from** | `[STORAGE]` units of shape `FUNCTIONAL` with exponent A2 ≤ −1 (the input manual puts no limit on A2) |
| **5.3.0** | `storage_readParams()` checks only A0, [`src/legacy/engine/node.c:693`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L693); the volume divides by A2 + 1 in `storage_getVolume()`, [`:946`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L946), and in `storage_getDepth()`, [`:838`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L838) |
| **5.2.4** | Same code, [`src/solver/node.c:724`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L724) and [`:960`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L960) |
| **6.0.0** | Reproduces: the `[STORAGE]` handler reads A2 without a check, [`src/engine/input/handlers/NodesHandler.cpp:379`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/NodesHandler.cpp#L379), and the volume divides by `g.b + 1.0`, [`src/engine/hydraulics/Node.cpp:141`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Node.cpp#L141) |
| **Since** | Every release (5.1.013 used the same formula, `aCoeff / e * pow(y, e)` with `e = aExpon + 1`) |
| **Fix** | Reject A2 ≤ −1 with ERROR 211 at the exponent: [`NUM-15_swmm530.patch`](NUM-15_swmm530.patch), [`NUM-15_swmm600.patch`](NUM-15_swmm600.patch) |

## The problem

A FUNCTIONAL storage unit has surface area A(d) = A0 + A1·d<sup>A2</sup> and volume V(d) = ∫<sub>0</sub><sup>d</sup> A(x) dx = A0·d + A1/(A2 + 1)·d<sup>A2+1</sup>. For A1 ≠ 0 the integral is finite only if A2 > −1: below that the area grows too fast towards the bottom.

SWMM checks that A0 is not negative and accepts any exponent. With A2 = −1 the formula divides by zero and every volume is infinite. With A2 < −1 it returns a finite value of the wrong sign for the divergent part: for A0 = 100, A1 = 50, A2 = −2 it gives V(d) = 100 d − 50/d, which is 50 ft³ at 1 ft and negative below 0.71 ft. That run reports a continuity error of 0.000% because the wrong volume is used consistently.

The numeric fuzzer that found this changed an exponent of 0 to −1 in a regression deck; a sign typo in A2 is enough to hit it.

## Why it happens

```c
// src/legacy/engine/node.c, storage_readParams()
    switch (m)
    {
        case FUNCTIONAL:
            // area at 0 depth can't be negative
            if (y[2] < 0.0) return error_setInpError(ERR_NUMBER, tok[7]);
            break;
```

```c
// src/legacy/engine/node.c, storage_getVolume()
        // --- for FUNCTIONAL relation, integrate a0 + a1*d^a2
        case FUNCTIONAL:
            d *= UCF(LENGTH);
            n = Storage[k].a2 + 1.0;
            v = (Storage[k].a0 * d) + Storage[k].a1 / n * pow(d, n);
```

`node_initState()` computes the full volume with this formula, so with A2 = −1 `Node[j].fullVolume` is `inf` from the start of the run; `storage_getDepth()` inverts it with `e = 1.0 / (a2 + 1.0)`. 6.0.0 ports both formulas (Node.cpp) and its `[STORAGE]` handler stores A2 without a check.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-15_a2-minus1.inp`](NUM-15_a2-minus1.inp) | Storage S1, FUNCTIONAL A1 = 50, A2 = −1, A0 = 100, initial depth 1 ft, 2 cfs inflow, side orifice outlet; dynamic wave, 2 hours |
| [`NUM-15_a2-minus2.inp`](NUM-15_a2-minus2.inp) | The same with A2 = −2 |
| [`NUM-15_a2-plus0.5.inp`](NUM-15_a2-plus0.5.inp) | The same with A2 = 0.5, a valid exponent (control) |
| [`NUM-15_test.c`](NUM-15_test.c) | Opens each deck through the legacy toolkit: the two decks with A2 ≤ −1 must be rejected, the control must run with finite volumes and continuity error |
| [`NUM-15_test6.c`](NUM-15_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-15            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-15 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print:

```
Deck         Result                 V(1 ft)    Max volume  Continuity (%)
a2-minus1    ran                        inf          inf            -nan
a2-minus2    ran                     50.000      394.013           0.000
a2-plus0.5   ran                    133.333      679.296           0.000
FAIL: 2 of 3 decks handled wrongly: a FUNCTIONAL unit with A2 <= -1 has no finite volume but is run
NUM-15 5.3.0 base: FAIL
```

6.0.0 gives the same volumes. (Its `swmm_get_routing_continuity_error()` returns −0.022% for the last two decks, while its own report, like legacy's, prints 0.000%.)

**With the fix**, both decks with A2 ≤ −1 stop with `ERROR 211: invalid number -1 at line 18 of [STORAGE] section` (5.3.0; `swmm_open` returns 200) or `ERROR 211: invalid number -1.` (6.0.0; `swmm_engine_open` returns 5), and the control deck is unchanged:

```
Deck         Result                 V(1 ft)    Max volume  Continuity (%)
a2-minus1    rejected (error 200)
a2-minus2    rejected (error 200)
a2-plus0.5   ran                    133.333      679.296           0.000
PASS: exponents A2 <= -1 are rejected and a valid exponent runs
NUM-15 5.3.0 patched: PASS
NUM-15 6.0.0 patched: PASS
```

## The fix

```diff
         case FUNCTIONAL:
             // area at 0 depth can't be negative
             if (y[2] < 0.0) return error_setInpError(ERR_NUMBER, tok[7]);
+            // volume (integral of area from 0 depth) requires exponent > -1
+            if (y[1] <= -1.0) return error_setInpError(ERR_NUMBER, tok[6]);
             break;
```

The 6.0.0 patch adds the same check to the FUNCTIONAL branch of the `[STORAGE]` handler. The check does not depend on A1: with A1 = 0 the exponent has no meaning, and A2 = −1 would still give 0/0 in the volume formula.

Exponents between −1 and 0 are still accepted. Their volume is finite, but the area is infinite at zero depth; that is a modelling choice, not a numerical failure, and is left alone.

Effect on other models: only decks with a FUNCTIONAL exponent of −1 or less change, and they now stop with an input error. All 18 FUNCTIONAL units in the regression decks use exponent 0.
