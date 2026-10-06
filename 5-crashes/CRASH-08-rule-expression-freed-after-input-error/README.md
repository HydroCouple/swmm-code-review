# CRASH-08: A model with a control-rule EXPRESSION crashes on close after an early input error

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | A common input mistake (a duplicate object name, an invalid option value) in a model whose `[CONTROLS]` section defines an `EXPRESSION` gives a segmentation fault in `swmm_close()` (or `swmm_run()`) instead of the error message, and the report file is left empty (0 bytes). A GUI that runs the engine in-process goes down with it. The same mistake in a model without an `EXPRESSION` is reported normally. |
| **Reached from** | `swmm_open()` followed by `swmm_close()`, or `swmm_run()`, on an input file with an `EXPRESSION` and an error that the object-counting pass detects |
| **5.3.0** | `controls_delete()` in [`src/legacy/engine/controls.c:849`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/controls.c#L849) |
| **5.2.4** | Same code, [`src/solver/controls.c:311`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/controls.c#L311) |
| **6.0.0** | Not affected: expressions are held in a `std::vector` inside `ControlEngine`, so there is no count to get out of step; the duplicate ID is reported as ERROR 207 |
| **Since** | 5.2.0, when control-rule expressions were added |
| **Fix** | Skip the loop when `Expression` is NULL: [`CRASH-08_swmm530.patch`](CRASH-08_swmm530.patch) |

## The problem

SWMM reads an input file twice. The first pass counts the objects of each kind, then `createObjects()` allocates the arrays, and the second pass reads the data. Control-rule expressions are counted in the first pass (`controls_addToCount()` increments `ExpressionCount` for each `EXPRESSION` line), and `Expression[]` is allocated by `controls_create()`, called from `createObjects()`.

Some errors are found in the first pass: a duplicate ID (the name is added to the hash table while counting), or an invalid option value (`[OPTIONS]` is read during counting). `createObjects()` then returns at its first line, `if ( ErrorCode ) return;`, before `controls_create()` runs, so `Expression` stays NULL while `ExpressionCount` is, say, 1. On close, `controls_delete()` walks the NULL array:

```c
// src/legacy/engine/controls.c, controls_delete()
    for (i = 0; i < ExpressionCount; i++)
    {
        mathexpr_delete(Expression[i].expression);   // Expression == NULL
        Expression[i].expression = NULL;
    }
    FREE(Expression);
```

`deleteObjects()` guards its own arrays against this case (`if ( Subcatch ) for (...)`); `controls_delete()` has no such guard. The error message has been written to the report file's buffer, but the process dies before the file is flushed, so the report is empty. A model with RULEs but no EXPRESSION is fine, because `controls_init()` resets `RuleCount` to 0 and only `controls_create()` sets it.

The review's input fuzzer found 9 mutants of `control_rules_test.inp` that crash 5.2.4 here and 6 that crash 5.3.0 (5.3.0 ignores the unknown option keywords that make the other three fail in 5.2.4).

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-08_expr-duplicate-id.inp`](CRASH-08_expr-duplicate-id.inp) | Junction J1 defined twice, and `VARIABLE D1 = NODE J1 DEPTH` / `EXPRESSION E1 = D1 * 2` |
| [`CRASH-08_expr-bad-option.inp`](CRASH-08_expr-bad-option.inp) | `FLOW_UNITS BOGUS` and the same variable and expression |
| [`CRASH-08_test.c`](CRASH-08_test.c) | Opens and closes each deck through the legacy toolkit; `swmm_open()` must return an error, `swmm_close()` must return, and the report must contain ERROR 207 / ERROR 205 |
| [`CRASH-08_test6.c`](CRASH-08_test6.c) | Opens, initializes and closes the duplicate-ID deck through the 6.0.0 API (6.0.0 does not reject `FLOW_UNITS BOGUS`, a separate input-validation issue) |

```sh
tools/run-test.sh CRASH-08            # 5.2.4 and 5.3.0: CRASH; 6.0.0: PASS
tools/run-test.sh CRASH-08 --patched  # 5.3.0 with the fix: PASS (6.0.0 has no patch)
```

**Without the fix**, 5.3.0 stops in `swmm_close()` on the first deck (5.2.4 at `controls.c:313` with the same stack):

```
CRASH-08_expr-duplicate-id.inp   swmm_open() returned 200; ../src/src/legacy/engine/controls.c:849:25: runtime error: applying zero offset to null pointer
    #0 0x7fa5d70b9fac in controls_delete .../src/legacy/engine/controls.c:849:39
    #1 0x7fa5d716843f in deleteObjects .../src/legacy/engine/project.c:1312:5
    #2 0x7fa5d716843f in project_close .../src/legacy/engine/project.c:289:5
    #3 0x7fa5d71c13bd in swmm_close .../src/legacy/engine/swmm5.c:1116:9
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior ../src/src/legacy/engine/controls.c:849:25
CRASH-08 5.2.4 base: CRASH
CRASH-08 5.3.0 base: CRASH
```

UndefinedBehaviorSanitizer stops at the null-pointer arithmetic. A build with AddressSanitizer alone gets one instruction further, `AddressSanitizer: SEGV ... controls.c:849:39 in controls_delete`, and leaves the report file at 0 bytes; a normal build dies with a segmentation fault. 6.0.0:

```
CRASH-08_expr-duplicate-id.inp  open/initialize returned 5; engine closed; report has "ERROR 207"
PASS: the input error is reported and the engine closes cleanly
CRASH-08 6.0.0 base: PASS
```

**With the fix:**

```
CRASH-08_expr-duplicate-id.inp   swmm_open() returned 200; swmm_close() returned; report has "ERROR 207"
CRASH-08_expr-bad-option.inp     swmm_open() returned 200; swmm_close() returned; report has "ERROR 205"
PASS: both input errors are reported and the projects close cleanly
CRASH-08 5.3.0 patched: PASS
```

## The fix

```diff
     int i;
 
-    for (i = 0; i < ExpressionCount; i++)
+    // --- Expression is NULL if input counting failed before it was created
+    for (i = 0; Expression && i < ExpressionCount; i++)
     {
         mathexpr_delete(Expression[i].expression);
         Expression[i].expression = NULL;
```

`FREE(NamedVariable)` and `FREE(Expression)` already accept NULL, and `deleteRules()` loops over `RuleCount`, which is 0 until `controls_create()` runs. Nothing changes for a model that opens without error.
