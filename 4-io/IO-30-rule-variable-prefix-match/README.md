# IO-30: Control-rule VARIABLE and EXPRESSION names are looked up by prefix

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A named variable or expression resolves to the first one defined whose name is a prefix of it: with `VARIABLE D1` and `VARIABLE D10`, every use of `D10` reads `D1`, in premises and inside `EXPRESSION` formulas; with `EXPRESSION H` and `H2`, `IF H2 > ...` reads `H`. Rules silently test the wrong quantity. A short name such as `N` also captures the object keyword `NODE`, so `IF NODE J1 DEPTH > 1` fails with "invalid keyword J1". |
| **Reached from** | `[CONTROLS]` `VARIABLE` and `EXPRESSION` definitions where one name begins with another (`Q1` / `Q10`, `D` / `DEPTH2`, `N` / `NODE`) |
| **5.3.0** | `getVariableIndex()` and `getExpressionIndex()` in [`src/legacy/engine/controls.c:1046`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1046) and [`:1068`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L1068), using `match()` from [`input.c:805`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L805) |
| **5.2.4** | Same code, [`src/solver/controls.c:403`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/controls.c#L403) and [`:432`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/controls.c#L432) |
| **6.0.0** | Not affected: names are compared whole and case-insensitively ([`src/engine/controls/Controls.cpp:1092`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/controls/Controls.cpp#L1092), with a comment that legacy's prefix match is "a quirk we deliberately do not copy") |
| **Since** | 5.2.0, when named variables and expressions were added |
| **Fix** | Compare whole names: [`IO-30_swmm530.patch`](IO-30_swmm530.patch) |

## The problem

Since 5.2.0 the `[CONTROLS]` section can name a variable (`VARIABLE Q1 = LINK C1 FLOW`) or a math expression (`EXPRESSION NetIn = Q1 + Q2 - Q3`) and use the name in rule premises and in other expressions. Each name is resolved with `match()`, SWMM's keyword matcher, which accepts a token when the defined name is a case-insensitive **prefix** of it, and the first such name wins.

Numbered names are the common way to hit it. In the test deck:

```
VARIABLE D1 = NODE SU1 DEPTH        ;; 1 ft
VARIABLE D10 = NODE SU2 DEPTH       ;; 3 ft
EXPRESSION H = D1                   ;; 1
EXPRESSION H2 = 5 * D1              ;; 5
EXPRESSION G = D10                  ;; 3

RULE RV  IF D10 > 2  THEN ORIFICE OR1 SETTING = 0.5
RULE RE  IF H2 > 2   THEN ORIFICE OR2 SETTING = 0.5
RULE RG  IF G > 2    THEN ORIFICE OR3 SETTING = 0.5
```

(each rule on three lines in the deck). All three premises are true, but 5.2.4 and 5.3.0 read `D10` as `D1` (1 ft) both in rule RV and inside the formula of G, and `H2` as `H` (1), so no rule acts. No error or warning is given.

A one- or two-letter name also captures the object keywords that start a premise. With `VARIABLE N = NODE SU1 DEPTH`, the premise `IF NODE SU2 DEPTH > 2` is read as `IF N SU2 ...` and the input is rejected with "ERROR 205: invalid keyword SU2". As the right-hand side of a premise (`... > NODE J2 DEPTH`) the same capture is silent: the premise compares against N and ignores the rest of the line.

## Why it happens

```c
// src/legacy/engine/controls.c
int getVariableIndex(char *varName)
{
    int i;
    for (i = 0; i < VariableCount; i++)
    {
        if (match(varName, NamedVariable[i].name))
            return i;
    }
    return -1;
}
```

`getExpressionIndex()` is the same over `Expression[]`. `match(str, substr)` (input.c) returns 1 when every character of `substr` matches the start of `str`:

```c
    // --- check if substr matches remainder of str
    for (i = k,j = 0; substr[j]; i++,j++)
    {
        if (!str[i] || UCHAR(str[i]) != UCHAR(substr[j])) return(0);
    }
    return(1);
```

That is what SWMM wants for keywords (`CONT` for `CONTINUITY`), not for user-defined names. `addPremise()` tries the expression names and then the variable names on the premise's first token before it tries the object keywords, and `controls_addExpression()` passes `getVariableIndex` to `mathexpr_create()` to resolve the identifiers in a formula, so every use goes through the prefix match. The variable and expression parsers do not check for duplicate names either.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-30_prefix-names.inp`](IO-30_prefix-names.inp) | Isolated storage units SU1 (1 ft) and SU2 (3 ft), the variables, expressions and rules above, and orifices OR1 to OR3 |
| [`IO-30_test.c`](IO-30_test.c) | Reads OR1, OR2 and OR3's settings after the second step through the legacy toolkit; all must be 0.5 |
| [`IO-30_test6.c`](IO-30_test6.c) | The same through the 6.0.0 API (passes unpatched) |

```sh
tools/run-test.sh IO-30            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-30 --patched  # 5.3.0 with the fix: PASS (6.0.0 has no patch)
```

**Without the fix** (5.2.4 and 5.3.0):

```
  rule premise                 link  setting (expected 0.5)
  IF D10 > 2  (D10 = 3)        OR1   1.00
  IF H2 > 2   (H2 = 5)         OR2   1.00
  IF G > 2    (G = D10 = 3)    OR3   1.00
FAIL: names resolve by prefix: D10 is read as D1 and H2 as H (OR1 1.00, OR2 1.00, OR3 1.00 instead of 0.50)
IO-30 5.2.4 base: FAIL
IO-30 5.3.0 base: FAIL
```

**6.0.0 unpatched, and 5.3.0 with the fix:**

```
  rule premise                 link  setting (expected 0.5)
  IF D10 > 2  (D10 = 3)        OR1   0.50
  IF H2 > 2   (H2 = 5)         OR2   0.50
  IF G > 2    (G = D10 = 3)    OR3   0.50
PASS: each variable and expression name resolves to the object with exactly that name
IO-30 6.0.0 base: PASS
IO-30 5.3.0 patched: PASS
```

## The fix

Compare whole names with `strcomp()` (case-insensitive equality, used by `findExactMatch()` for keywords):

```diff
 int getVariableIndex(char *varName)
 {
     int i;
+    char name[MAXVARNAME + 1];
+
+    // --- names are stored truncated to MAXVARNAME characters
+    sstrncpy(name, varName, MAXVARNAME);
     for (i = 0; i < VariableCount; i++)
     {
-        if (match(varName, NamedVariable[i].name))
+        if (strcomp(name, NamedVariable[i].name))
             return i;
     }
     return -1;
 }
```

and the same in `getExpressionIndex()`. The token is cut to `MAXVARNAME` (32) characters before the comparison because `controls_addVariable()` and `controls_addExpression()` store names cut to that length. The prefix match had made longer names work by accident; a plain `strcomp()` broke them: a 36-character variable name gave "ERROR 233: invalid math expression" and "ERROR 205: invalid keyword" in a first version of this patch. With the cut, names longer than 32 characters still work, and two names that differ only after their 32nd character are still the same name, as before.

**Effect on other models.** Only models with one name that begins with another change. `control_rules_test.inp`, the regression deck with named variables (`Q1`, `Q2`, `Q3`) and an expression (`Net_Inflow`), gives a byte-identical `.out` file with the patch.
