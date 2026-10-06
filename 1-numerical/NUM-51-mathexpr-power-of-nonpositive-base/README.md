# NUM-51: x^y is 0 for every negative x, so (-3)^2 evaluates to 0

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | In 5.x, any power of a negative number in a `[TREATMENT]`, `[GWF]` or control-rule expression is 0: `(TSS - 13)^2` with TSS = 10 gives 0 instead of 9, and `30 + (TSS - 12)^3` gives 30 instead of 22. A term such as `(Hgw - Hsw)^2` silently drops out whenever its base turns negative. In 6.0.0 the integer powers are right, but a fractional power of a negative number is NaN and becomes the result (a node concentration of NaN in the test). No warning in either. |
| **Reached from** | `^` with a negative base in a treatment expression, a custom groundwater flow or deep-loss expression (`[GWF]`), or a control-rule `EXPRESSION` |
| **5.3.0** | `mathexpr_eval()`, case 31, in [`src/legacy/engine/mathexpr.c:712`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/mathexpr.c#L712) |
| **5.2.4** | Same code, [`src/solver/mathexpr.c:712`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/mathexpr.c#L712) |
| **6.0.0** | Different defect: plain `std::pow` with no guard in all four evaluators, [`math/MathExpr.cpp:241`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/math/MathExpr.cpp#L241), [`:400`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/math/MathExpr.cpp#L400), [`quality/Treatment.cpp:605`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/Treatment.cpp#L605), [`:830`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/Treatment.cpp#L830) |
| **Since** | 5.1.001 (commit fa32a734, 2014), which computed x^y as `exp(y*log(x))` and so needed x > 0. 5.2.0 switched to `pow()` ("Fix problems with '^' operator", aac3754a) but kept the guard. |
| **Fix** | Call `pow()` for a negative base with a whole-number exponent; return 0 only where there is no real value: [`NUM-51_swmm530.patch`](NUM-51_swmm530.patch), [`NUM-51_swmm600.patch`](NUM-51_swmm600.patch) |

## The problem

The input reference lists `^` among "the standard operators +, -, *, /, ^ (for exponentiation)" for treatment functions, and the same evaluator serves `[GWF]` expressions and control-rule expressions. Under standard exponentiation (-3)^2 = 9 and (-2)^3 = -8. 5.2.4 and 5.3.0 return 0 for both. Junction J1 in the test receives TSS = 10 mg/L:

| Equation at J1 | Should be | 5.2.4 / 5.3.0 |
|---|---|---|
| `TP C = (TSS - 13)^2` | 9 | 0 |
| `COD C = 30 + (TSS - 12)^3` | 22 | 30 |

Users square a difference to get a magnitude (`(Hgw - Hsw)^2`, `(C - 50)^2`); in 5.x the whole term vanishes whenever the difference is negative, and nothing in the report shows it.

A fractional power of a negative number, `(-4)^0.5`, has no real value. 5.x returns 0 for it, the same as `sqrt()` of a negative number. 6.0.0 removed the guard and gets the integer powers right, but `std::pow(-4, 0.5)` is NaN and nothing replaces it: in the test, BOD at J1 becomes NaN, and the BOD quality continuity line in the report prints `-nan`. The 6.0.0 manual (Vol. III, section 5.4.4) states that "a treatment expression can never produce a non-finite result".

## Why it happens

```c
// src/legacy/engine/mathexpr.c, mathexpr_eval()
            case 31:
		r1 = ExprStack[stackindex];
                stackindex--;
                if (stackindex < 0) break;
                r2 = ExprStack[stackindex];
		if (r2 <= 0.0) r2 = 0.0;          // any base <= 0, whatever the exponent
		else r2 = pow(r2, r1);
```

The guard dates from 5.1, where the power was computed as `exp(r1*log(r2))` and a base <= 0 could not be used at all. 5.2.0 changed the formula to `pow()`, which handles negative bases with whole-number exponents, but left the guard as it was.

6.0.0's evaluators push `std::pow(a, b)` with no check:

```cpp
// src/engine/quality/Treatment.cpp, treatment::evaluate()
                    case TokenType::POW: stk.push(std::pow(a, b)); break;
```

and return the top of the stack without a NaN test, so the NaN reaches the treatment result. There, `std::max(result, 0.0)` and `std::min(result, c_node)` both keep a NaN that comes first, and the removal and the node's concentration become NaN.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-51_power.inp`](NUM-51_power.inp) | Junction J1 (kinematic wave) with 1 cfs DWF at TSS = 10, TP = 20, COD = 40, BOD = 20 mg/L and the treatment equations `TP C = (TSS - 13)^2`, `COD C = 30 + (TSS - 12)^3`, `BOD C = 5 + (TSS - 14)^0.5` |
| [`NUM-51_test.c`](NUM-51_test.c) | Runs the deck (5.2.4, 5.3.0) and reads J1's concentrations in the last reporting period from the `.out` file |
| [`NUM-51_test6.c`](NUM-51_test6.c) | The same through the 6.0.0 API |
| [`NUM-51_swmm530.patch`](NUM-51_swmm530.patch), [`NUM-51_swmm600.patch`](NUM-51_swmm600.patch) | The fixes |

```sh
tools/run-test.sh NUM-51            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-51 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same as 5.3.0):

```
---- NUM-51 on 5.3.0 (base) ----
Pollutant  Equation at J1          Expected  Computed (mg/L)
TP         C = (TSS - 13)^2             9.00      0.00  <-- wrong
COD        C = 30 + (TSS - 12)^3       22.00     30.00  <-- wrong
BOD        C = 5 + (TSS - 14)^0.5       5.00      5.00
FAIL: a power of a negative number is wrong: TP = 0.00 (expected 9), COD = 30.00 (expected 22), BOD = 5.00 (expected 5)
---- NUM-51 on 6.0.0 (base) ----
Pollutant  Equation at J1          Expected  Computed (mg/L)
TP         C = (TSS - 13)^2             9.00      9.00
COD        C = 30 + (TSS - 12)^3       22.00     22.00
BOD        C = 5 + (TSS - 14)^0.5       5.00      -nan  <-- wrong
FAIL: a power of a negative number is wrong: TP = 9.00 (expected 9), COD = 22.00 (expected 22), BOD = -nan (expected 5)
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
---- NUM-51 on 5.3.0 (patched) ----
Pollutant  Equation at J1          Expected  Computed (mg/L)
TP         C = (TSS - 13)^2             9.00      9.00
COD        C = 30 + (TSS - 12)^3       22.00     22.00
BOD        C = 5 + (TSS - 14)^0.5       5.00      5.00
PASS: integer powers of negative numbers are evaluated, and a fractional power of a negative number gives 0, not NaN
```

## The fix

Both engines use the same rule: `pow()` for a positive base, and for a negative base when the exponent is a whole number; 0 otherwise.

```diff
-		if (r2 <= 0.0) r2 = 0.0;
-		else r2 = pow(r2, r1);
+		// --- a negative base has a real power only for a whole exponent
+		if (r2 > 0.0 || (r2 < 0.0 && r1 == floor(r1))) r2 = pow(r2, r1);
+		else r2 = 0.0;
```

The 6.0.0 patch makes the same change at the four `std::pow` calls. A zero base keeps the legacy result 0 for every exponent, including 0^0 and 0^-1 (6.0.0 gave 1 and inf), so a term like `DEPTH^-0.5` at a dry node stays 0 as before.

Only expressions that raise a negative number to a whole power change. Two related quirks are left alone: a minus sign in front of a number is read as part of the number, so `-2^2` is (-2)^2 = 4 after the fix (it was 0), while `-X^2` with X = 2 is -4; and `LOG10(x)` of a negative number makes the whole expression 0, where `LOG(x)` gives 0 for that term only. **Effect on other models:** none of the 73 regression decks has a `[TREATMENT]` or `[GWF]` section or a `^` in a control rule.
