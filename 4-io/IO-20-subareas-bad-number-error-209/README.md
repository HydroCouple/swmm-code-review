# IO-20: An invalid [SUBAREAS] number is reported as "undefined object"; 6.0.0 accepts it

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | 5.2.4 and 5.3.0 stop as they should, but with the wrong message: N-Perv `abc` gives "ERROR 209: undefined object abc", which sends the user looking for a missing object. 6.0.0 gives no error at all. `abc` runs as n = 0, and an S-Perv of -0.5 in is used as a negative depression storage: 2.32 in of runoff from 0.90 in of rain, with a runoff continuity error of -158 %. |
| **Reached from** | `[SUBAREAS]` N-Imperv, N-Perv, S-Imperv, S-Perv or PctZero that is not a number or is negative |
| **5.3.0** | `subcatch_readSubareaParams()` in [`src/legacy/engine/subcatch.c:242`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L242) |
| **5.2.4** | Same code, [`src/solver/subcatch.c:222`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L222) |
| **6.0.0** | Worse: `handle_subareas()` in [`src/engine/input/handlers/CatchmentHandler.cpp:218`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/CatchmentHandler.cpp#L218) converts the five values with `to_double()` and does not check them, so the input is accepted |
| **Since** | Every release |
| **Fix** | 5.3.0: use ERR_NUMBER. 6.0.0: parse and check the values as legacy does. [`IO-20_swmm530.patch`](IO-20_swmm530.patch), [`IO-20_swmm600.patch`](IO-20_swmm600.patch) Apply after IO-17 (its `Requires:` line). |

## The problem

The five numbers on a `[SUBAREAS]` line (two Manning's n values, two depression storages and PctZero) must be non-negative numbers. 5.2.4 and 5.3.0 check this but report a failure as ERROR 209, the error for a name that does not match any object:

```
ERROR 209: undefined object abc at line 34 of [SUBAREA] section:
ERROR 209: undefined object -0.5 at line 34 of [SUBAREA] section:
```

The run stops, which is right, but the message points the user to the wrong kind of mistake. Every other numeric check in `subcatch.c` (and the PctRouted check on the same line) reports ERROR 211, "invalid number".

6.0.0 does not check these values at all:

- N-Perv `abc` is converted to 0, so the pervious area runs as n = 0 ("no routing"). The run then hits [NUM-29](../../1-numerical/NUM-29-zero-roughness-subarea-depression-storage/) and reports a -5.556 % continuity error.
- S-Perv -0.5 in is used as a negative depression storage. In the same deck it gives 2.323 in of runoff from 0.900 in of rain, a continuity error of -158.149 %.

## Why it happens

```c
// src/legacy/engine/subcatch.c, subcatch_readSubareaParams()
// --- read in Mannings n, depression storage, & PctZero values
for (i = 0; i < 5; i++)
{
    if ( ! getDouble(tok[i+1], &x[i])  || x[i] < 0.0 )
        return error_setInpError(ERR_NAME, tok[i+1]);     // ERR_NAME = 209
}
```

```cpp
// src/engine/input/handlers/CatchmentHandler.cpp, handle_subareas()
ctx.subcatches.n_imperv[idx]  = to_double(tok[1]);       // garbage -> 0, negatives kept
ctx.subcatches.n_perv[idx]    = to_double(tok[2]);
ctx.subcatches.ds_imperv[idx] = to_double(tok[3]);
ctx.subcatches.ds_perv[idx]   = to_double(tok[4]);
...
ctx.subcatches.pct_zero[idx] = to_double(tok[5]);
```

## How to reproduce

| File | What it is |
|---|---|
| [`IO-20_nperv-abc.inp`](IO-20_nperv-abc.inp) | One subcatchment whose N-Perv is `abc` |
| [`IO-20_sperv-negative.inp`](IO-20_sperv-negative.inp) | The same with a valid N-Perv and an S-Perv of -0.5 |
| [`IO-20_test.c`](IO-20_test.c) | Opens both decks through the legacy toolkit (5.2.4 and 5.3.0) and checks that each is rejected with ERROR 211 |
| [`IO-20_test6.c`](IO-20_test6.c) | The same with the 6.0.0 engine API |

```sh
tools/run-test.sh IO-20            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-20 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** 5.2.4 and 5.3.0 print:

```
IO-20_nperv-abc.inp:
  rejected, error code 200; the report says:
    ERROR 209: undefined object abc at line 34 of [SUBAREA] section:
  -> not reported as ERROR 211 (invalid number)
IO-20_sperv-negative.inp:
  rejected, error code 200; the report says:
    ERROR 209: undefined object -0.5 at line 34 of [SUBAREA] section:
  -> not reported as ERROR 211 (invalid number)
FAIL: 2 of 2 invalid [SUBAREAS] numbers are not reported as ERROR 211 (invalid number)
IO-20 5.2.4 base: FAIL
IO-20 5.3.0 base: FAIL
```

and 6.0.0:

```
IO-20_nperv-abc.inp:
  accepted: the run finished, runoff continuity error -5.556 %
IO-20_sperv-negative.inp:
  accepted: the run finished, runoff continuity error -158.149 %
FAIL: 2 of 2 invalid [SUBAREAS] numbers are not reported as ERROR 211 (invalid number)
IO-20 6.0.0 base: FAIL
```

**With the fix**:

```
IO-20_nperv-abc.inp:
  rejected, error code 200; the report says:
    ERROR 211: invalid number abc at line 34 of [SUBAREA] section:
IO-20_sperv-negative.inp:
  rejected, error code 200; the report says:
    ERROR 211: invalid number -0.5 at line 34 of [SUBAREA] section:
PASS: both invalid [SUBAREAS] numbers are reported as ERROR 211
IO-20 5.3.0 patched: PASS
IO-20_nperv-abc.inp:
  rejected, error code 5; the engine says:
    ERROR 211: invalid number abc.
IO-20_sperv-negative.inp:
  rejected, error code 5; the engine says:
    ERROR 211: invalid number -0.5.
PASS: both invalid [SUBAREAS] numbers are reported as ERROR 211
IO-20 6.0.0 patched: PASS
```

## The fix

5.3.0 needs only the right error code:

```diff
         if ( ! getDouble(tok[i+1], &x[i])  || x[i] < 0.0 )
-            return error_setInpError(ERR_NAME, tok[i+1]);
+            return error_setInpError(ERR_NUMBER, tok[i+1]);
```

6.0.0 gets the check legacy has, using its whole-token parser `parse_double_strict()` (legacy `getDouble()` semantics), before the values are stored:

```cpp
// n, depression storage and PctZero must be numbers >= 0
// (legacy subcatch_readSubareaParams: ERROR 211)
bool bad = false;
for (std::size_t k = 1; k <= 5 && !bad; ++k) {
    double v = 0.0;
    if (!parse_double_strict(tok[k], v) || v < 0.0) {
        ctx.errors.push_back(format_error(ERR_NUMBER, tok[k]));
        bad = true;
    }
}
if (bad) continue;
```

The upper limit on PctZero is [IO-18](../IO-18-pct-zero-over-100/), patched separately. The two patches change different lines of the same functions and apply in either order.

**Effect on other models.** Valid input is unaffected. All 73 regression decks still open in the patched 6.0.0 without an ERROR 211.

`handle_subareas()` also has two other gaps, which this patch leaves alone. A line with fewer than 6 tokens is skipped where legacy reports ERROR 203 (too few items; legacy also requires the RouteTo keyword, i.e. 7 tokens). A line naming an unknown subcatchment is skipped where legacy reports ERROR 209.
