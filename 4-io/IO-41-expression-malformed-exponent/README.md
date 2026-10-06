# IO-41: A malformed number in an expression ("2.5E+", ".") is accepted instead of rejected

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A typo in a numeric constant of a treatment, groundwater-flow or control-rule expression is accepted without an error. 5.x reads the constant as 0, so `C = 2.5E+` sets a treated concentration to 0 and `R = 0.5*5e` removes nothing. 6.0.0 reads the valid prefix (`2.5E+` is 2.5, `1.5.2` is 1.5), and a decimal point with no digits (`2 + .`) terminates the program with an uncaught C++ exception. |
| **Reached from** | `[TREATMENT]`, `[GWF]` and `[CONTROLS]` `EXPRESSION` lines with an exponent marker that has no digits after it (`5e`, `2.5E+`, `1e-`) or a lone decimal point |
| **5.3.0** | `getNumber()` in [`src/legacy/engine/mathexpr.c:246`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/mathexpr.c#L246) |
| **5.2.4** | Same code, [`src/solver/mathexpr.c:246`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/mathexpr.c#L246) |
| **6.0.0** | Reproduces differently: `std::stod` in the tokenizers reads a prefix or throws, [`src/engine/math/MathExpr.cpp:111`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/math/MathExpr.cpp#L111) (control rules, `[GWF]`) and [`src/engine/quality/Treatment.cpp:150`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/Treatment.cpp#L150) (`[TREATMENT]`). `[GWF]` expressions are checked first by a strict validator and are rejected correctly. |
| **Since** | The oldest code on GitHub (2014 initial commit), so every 5.x release |
| **Fix** | Flag a parse error for a number that is not complete: [`IO-41_swmm530.patch`](IO-41_swmm530.patch), [`IO-41_swmm600.patch`](IO-41_swmm600.patch) |

## The problem

The expression parser rejects most malformed input with `ERROR 233: invalid math expression`, for instance `1.5.2` or an unbalanced parenthesis. Two kinds of malformed number slip through:

- an exponent marker with nothing after it, or only a sign: `5e`, `2.5E+`, `1e-`
- a decimal point with no digits: `.`, `2 + .`, `.e3`

5.2.4 and 5.3.0 accept the expression and use 0 for the constant. A user who meant `R = 0.5*5e-2` and lost the last digit gets `R = 0` and no treatment, with nothing in the report to say so.

6.0.0 converts the characters with `std::stod`, which stops at the first character it cannot use. `2.5E+` becomes 2.5 and `1.5.2` becomes 1.5, both accepted without a message. When there is no valid prefix at all, as for `.`, `std::stod` throws `std::invalid_argument`; nothing catches it and the process ends with `terminate called after throwing an instance of 'std::invalid_argument'`, from the API call or the command-line program alike.

The test uses two control-rule expressions, `EXPRESSION E1 = 2.5E+` and `EXPRESSION E1 = 2 + .`, because both engines report a control expression that fails to parse as an input error.

## Why it happens

`getNumber()` notices the dangling exponent, but keeps the finding in a local flag:

```c
// src/legacy/engine/mathexpr.c, getNumber()
                if (Pos >= Len || !isDigit(S[Pos])) errflag = 1;
    ...
    Pos--;
    if (errflag) return 0;                 // the parser's Err is never set
    else return atof(sNumber);
```

`mathexpr_create()` only looks at the module flag `Err`, so the expression is built with the number 0. A decimal point with no digits does not even set `errflag`: `sNumber` is `"."` and `atof(".")` is 0.

6.0.0 collects digits, `.`, `e`/`E` and an exponent sign, then converts:

```cpp
// src/engine/math/MathExpr.cpp, tokenize()
            Token t; t.type = TokenType::NUMBER;
            t.value = std::stod(s.substr(start, i - start));   // prefix only, or throws
```

`[TREATMENT]` has the same line in `quality/Treatment.cpp`. The `[GWF]` validator (`groundwater::gwf_validate()`) uses `strtod` and checks that the whole word was consumed, which is why `[GWF]` is not affected.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-41_exponent.inp`](IO-41_exponent.inp) | One junction and outfall, with the control-rule expression `EXPRESSION E1 = 2.5E+` |
| [`IO-41_point.inp`](IO-41_point.inp) | The same with `EXPRESSION E1 = 2 + .` |
| [`IO-41_test.c`](IO-41_test.c) | Opens both decks (5.2.4, 5.3.0) and expects `swmm_open()` to fail with ERROR 233 in the report |
| [`IO-41_test6.c`](IO-41_test6.c) | Opens both decks through the 6.0.0 API and expects an input error |
| [`IO-41_swmm530.patch`](IO-41_swmm530.patch), [`IO-41_swmm600.patch`](IO-41_swmm600.patch) | The fixes |

```sh
tools/run-test.sh IO-41            # 5.2.4 and 5.3.0: FAIL; 6.0.0: CRASH
tools/run-test.sh IO-41 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same as 5.3.0):

```
---- IO-41 on 5.3.0 (base) ----
Deck                 Expression     Error  Report
IO-41_exponent.inp   E1 = 2.5E+         0  accepted  <-- malformed number read as 0
IO-41_point.inp      E1 = 2 + .         0  accepted  <-- malformed number read as 0
FAIL: 2 of 2 expressions with a malformed number are accepted
---- IO-41 on 6.0.0 (base) ----
Deck                 Expression     Error  Message
IO-41_exponent.inp   E1 = 2.5E+         0  accepted  <-- malformed number not rejected
terminate called after throwing an instance of 'std::invalid_argument'
  what():  stod
IO-41 6.0.0 base: CRASH
```

**With the fix:**

```
---- IO-41 on 5.3.0 (patched) ----
Deck                 Expression     Error  Report
IO-41_exponent.inp   E1 = 2.5E+       200  ERROR 233: invalid math expression at line 41 of [CONTROL] section:
IO-41_point.inp      E1 = 2 + .       200  ERROR 233: invalid math expression at line 41 of [CONTROL] section:
PASS: both malformed numbers are rejected with ERROR 233
---- IO-41 on 6.0.0 (patched) ----
Deck                 Expression     Error  Message
IO-41_exponent.inp   E1 = 2.5E+         5    ERROR 217: control rule clause invalid or out of sequence in [CONTROLS] rule block #1, line 1: could not parse the formula for EXPRESSION 'E1'.
IO-41_point.inp      E1 = 2 + .         5    ERROR 217: control rule clause invalid or out of sequence in [CONTROLS] rule block #1, line 1: could not parse the formula for EXPRESSION 'E1'.
PASS: both malformed numbers are rejected with an input error
```

## The fix

5.3.0: set the parser's error flag for a dangling exponent and for a number without digits. A number has a digit either first or right after a leading decimal point.

```diff
     Pos--;
-    if (errflag) return 0;
-    else return atof(sNumber);
+
+    /* --- a number with no digits (".") or no exponent digits ("5e")
+           is an error */
+    if (errflag || !(isDigit(sNumber[0]) || isDigit(sNumber[1])))
+    {
+        Err = 1;
+        return 0;
+    }
+    return atof(sNumber);
```

6.0.0: in both tokenizers, call `std::stod` with a position argument and catch its exception; a number it does not consume completely is reported by the tokenizer, and `mathexpr::parse()` returns an error for it. An empty expression still parses as an empty one, which 6.0.0's unit test `MathExprParse.EmptyString` requires. The treatment validator, `treatment::validate()`, gets the same check.

```diff
             Token t; t.type = TokenType::NUMBER;
-            t.value = std::stod(s.substr(start, i - start));
+            size_t used = 0;
+            try { t.value = std::stod(s.substr(start, i - start), &used); } catch (...) {}
+            if (used != i - start) return {};  // malformed number, e.g. "5e" or "."
             tokens.push_back(t);
```

Every well-formed number converts as before. In 6.0.0, a `[TREATMENT]` expression with a malformed number now fails to compile instead of crashing or using a prefix, but `SWMMEngine::initQuality()` skips a treatment expression that fails to compile without reporting it, so that case runs untreated rather than stopping with an error. That silent skip affects every invalid treatment expression in 6.0.0 and is outside this issue. **Effect on other models:** none of the 73 regression decks has a `[TREATMENT]` or `[GWF]` section or a control-rule expression.
