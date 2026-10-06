# IO-40: An expression with "-" right after ")" is rejected, so "(TSS)-1" is ERROR 233

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A valid treatment, groundwater-flow or control-rule expression in which a closing parenthesis is followed directly by a minus sign and a digit, such as `C = (TSS)-1` or `(Hgw-Hcb)-0.5`, stops the run with `ERROR 233: invalid math expression`. The same expression with a space, `(TSS) - 1`, is accepted, so whether a deck runs depends on whitespace. |
| **Reached from** | `[TREATMENT]`, `[GWF]` and `[CONTROLS]` `EXPRESSION` lines containing `)-<digit>` |
| **5.3.0** | `getOperand()` in [`src/legacy/engine/mathexpr.c:263`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/mathexpr.c#L263); rejected in `getTree()` at [`:459`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/mathexpr.c#L459) |
| **5.2.4** | Same code, [`src/solver/mathexpr.c:263`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/mathexpr.c#L263) |
| **6.0.0** | Not affected. Its tokenizers decide between a unary and a binary minus from the previous token, and `)` gives a binary minus ([`src/engine/math/MathExpr.cpp:136`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/math/MathExpr.cpp#L136), [`src/engine/quality/Treatment.cpp:227`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/Treatment.cpp#L227)). |
| **Since** | 5.2.2 (commit c54b60f6, "Problems related to '^' operator", 2022). Before it a minus was read as a sign only at the start or after `(`. |
| **Fix** | Do not treat `-` as a sign after `)`: [`IO-40_swmm530.patch`](IO-40_swmm530.patch) |

## The problem

Junction J1 in the test receives TSS = 10 mg/L and has the treatment equations

```
J1  TP   C = (TSS)-1
J1  COD  C = (TSS+2)-3
```

Both are ordinary subtractions with the value 9. 5.2.4 and 5.3.0 refuse the input file:

```
ERROR 233: invalid math expression at line 51 of [TREATMENT] section:
```

Written as `(TSS) - 1` and `(TSS+2) - 3`, the same equations are accepted and give 9. A user who gets ERROR 233 has no hint that a space would fix it. The same happens in a groundwater flow expression such as `0.001*(Hgw-Hcb)-0.5` and in a control-rule `EXPRESSION`.

## Why it happens

The lexer reads a minus sign that is followed by a digit as part of a negative number when the previous lexeme could not end an operand. The condition for that is written as a range of lexeme codes:

```c
// src/legacy/engine/mathexpr.c, getOperand()
      case '-': code = 4;
        if (Pos < Len-1 &&
            isDigit(S[Pos+1]) &&
            (CurLex <= 6 || CurLex == 31))      // 0 = start, 1 = '(', 2 = ')', 3..6 = + - * /, 31 = '^'
        {
            Pos++;
            Fvalue = -getNumber();
            code = 7;                           // a number
        }
```

Code 2 is `)`, which does end an operand. After `(TSS)` the lexer returns the number -1 instead of a minus operator, and `getTree()` then finds a number where it expects `+` or `-`:

```c
// src/legacy/engine/mathexpr.c, getTree()
        if (lex != 3 && lex != 4 )
        {
            Err = 1;
            break;
        }
```

Up to 5.2.1 the condition was `(CurLex == 0 || CurLex == 1)`, the start of the expression or after `(`. 5.2.2 widened it to allow a negative number after an operator (`2*-3`, `X^-1`), and the range `<= 6` took `)` in by accident.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-40_paren-minus.inp`](IO-40_paren-minus.inp) | Junction J1 (kinematic wave), 1 cfs DWF with TSS = 10, TP = 20, COD = 40 mg/L; `TP C = (TSS)-1`, `COD C = (TSS+2)-3` |
| [`IO-40_paren-minus-spaced.inp`](IO-40_paren-minus-spaced.inp) | The same with a space after each `)` |
| [`IO-40_test.c`](IO-40_test.c) | Runs both decks (5.2.4, 5.3.0); prints the first input error from the report, or TP and COD at J1 |
| [`IO-40_test6.c`](IO-40_test6.c) | The same through the 6.0.0 API |
| [`IO-40_swmm530.patch`](IO-40_swmm530.patch) | The fix |

```sh
tools/run-test.sh IO-40            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-40 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same as 5.3.0):

```
---- IO-40 on 5.3.0 (base) ----
Deck                          TP eqn          COD eqn             Error  TP     COD  (expected 9, 9)
IO-40_paren-minus.inp         C = (TSS)-1     C = (TSS+2)-3         200  rejected: ERROR 233: invalid math expression at line 51 of [TREATMENT] section:
IO-40_paren-minus-spaced.inp  C = (TSS) - 1   C = (TSS+2) - 3         0   9.00   9.00
FAIL: 1 of 2 decks with a valid '-' after ')' is rejected or gives a wrong result
---- IO-40 on 6.0.0 (base) ----
Deck                          TP eqn          COD eqn             Error  TP     COD  (expected 9, 9)
IO-40_paren-minus.inp         C = (TSS)-1     C = (TSS+2)-3           0   9.00   9.00
IO-40_paren-minus-spaced.inp  C = (TSS) - 1   C = (TSS+2) - 3         0   9.00   9.00
PASS: a minus sign after ')' is read as a subtraction, with or without a space
```

The test prints the first error; the report lists ERROR 233 for both lines, 51 (TP) and 52 (COD).

**With the fix:**

```
---- IO-40 on 5.3.0 (patched) ----
Deck                          TP eqn          COD eqn             Error  TP     COD  (expected 9, 9)
IO-40_paren-minus.inp         C = (TSS)-1     C = (TSS+2)-3           0   9.00   9.00
IO-40_paren-minus-spaced.inp  C = (TSS) - 1   C = (TSS+2) - 3         0   9.00   9.00
PASS: a minus sign after ')' is read as a subtraction, with or without a space
```

## The fix

Leave `)` out of the set of lexemes after which a minus sign starts a number:

```diff
         if (Pos < Len-1 &&
             isDigit(S[Pos+1]) &&
-            (CurLex <= 6 || CurLex == 31))
+            ((CurLex <= 6 && CurLex != 2) || CurLex == 31))
```

Expressions that were accepted before parse exactly as before; only `)-<digit>`, which was always an error, now parses as a subtraction. There is no 6.0.0 patch. **Effect on other models:** none of the 73 regression decks has a `[TREATMENT]` or `[GWF]` section or a control-rule expression.
