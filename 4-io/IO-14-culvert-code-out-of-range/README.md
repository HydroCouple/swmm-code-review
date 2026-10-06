# IO-14: A culvert code above 57 is accepted and turns off normal-flow limiting

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A mistyped culvert code (58 or more) is accepted without a message. It gives no inlet control, because there is no such code, but it also stops the conduit from being normal-flow limited. In the test the depth upstream of a steep pipe drops from 0.912 ft (normal depth, no culvert code) to 0.754 ft. |
| **Reached from** | `[XSECTIONS]` Culvert column (8th item) above 57; in 6.0.0 also a negative code, which is ignored instead of rejected |
| **5.3.0** | `link_readXsectParams()` in [`src/legacy/engine/link.c:265`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L265); run-time effect in [`dwflow.c:331`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dwflow.c#L331) and [`culvert.c:197`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/culvert.c#L197) |
| **5.2.4** | Same code, [`src/solver/link.c:262`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L262), [`dwflow.c:251`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dwflow.c#L251) |
| **6.0.0** | Reproduces: [`handle_xsections()`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/LinksHandler.cpp#L504) stores any positive code, [`DynamicWave.cpp:2590`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/DynamicWave.cpp#L2590) takes the culvert branch, [`Culvert.cpp:229`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Culvert.cpp#L229) ignores the code |
| **Since** | Every SWMM 5 release (5.0.022 has the same check) |
| **Fix** | Reject codes outside 0–57 when the line is read: [`IO-14_swmm530.patch`](IO-14_swmm530.patch), [`IO-14_swmm600.patch`](IO-14_swmm600.patch) |

## The problem

The Culvert item of an `[XSECTIONS]` line is a code from the culvert table (Table H-1 of the hydraulics reference manual), which lists codes 1 to 57; 0 or blank means the conduit is not a culvert. The reader only rejects negative codes, so 58, 75 or 570 are all accepted and stored.

At run time the two places that use the code disagree about what it means:

- `dwflow.c` treats every code above 0 as a culvert. For positive flow in a conduit that is not full, it asks `culvert_getInflow()` for the inlet-controlled flow and skips the normal-flow check that every other conduit gets.
- `culvert_getInflow()` has coefficients for codes 1 to 57 only. For any other code it returns the dynamic-wave flow unchanged.

So an invalid code gives the conduit neither inlet control nor normal-flow limiting. With `NORMAL_FLOW_LIMITED BOTH` (the default), a steep pipe whose upstream end is supercritical is normally held to its normal-flow capacity at the upstream depth. With code 58 it is not. In the test model (a 3 ft pipe at 5 % into a mild 4 ft pipe, 30 cfs), the depth at the upstream junction J1 settles at 0.912 ft without a culvert code and at 0.754 ft with code 58. Nothing in the report points at the code.

## Why it happens

```c
// src/legacy/engine/link.c, link_readXsectParams()
        if ( Link[j].type == CONDUIT && ntoks >= 8 )
        {
            i = atoi(tok[7]);
            if ( i < 0 ) return error_setInpError(ERR_NUMBER, tok[7]);
            else Link[j].xsect.culvertCode = i;
        }

// src/legacy/engine/dwflow.c, dwflow_findConduitFlow()
        if ( xsect->culvertCode > 0 && !isFull )
            q = culvert_getInflow(linkIndex, q, h1);
        else if (NormalFlowLtd != NEITHER && ... )
            q = checkNormalFlow(linkIndex, q, y1, y2, a1, r1);

// src/legacy/engine/culvert.c, culvert_getInflow()
    if ( code <= 0 || code > MAX_CULVERT_CODE ) return q0;     // MAX_CULVERT_CODE = 57
```

6.0.0 has the same three pieces. Its reader also drops a negative code silently (`if (cc > 0) ... = cc;`), where legacy rejects it.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-14_code-58.inp`](IO-14_code-58.inp) | Steep 3 ft pipe C1 (5 %) into a mild 4 ft pipe C2 (0.1 %), 30 cfs at J1, culvert code 58 on C1 |
| [`IO-14_code-0.inp`](IO-14_code-0.inp) | The same model without a culvert code |
| [`IO-14_test.c`](IO-14_test.c) | Runs both decks through the legacy toolkit (5.2.4, 5.3.0) and prints the depth at J1 at the end of each run |
| [`IO-14_test6.c`](IO-14_test6.c) | The same through the 6.0.0 API |

Code 58 is not a culvert code, so the correct result is that the deck is rejected. The depth comparison only shows what the accepted code does. It is not the pass criterion.

```sh
tools/run-test.sh IO-14            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-14 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (all three engines print the same):

```
Culvert code on C1   error   J1 depth at end (ft)
  0 (none)               0      0.912
 58 (invalid)            0      0.754
FAIL: culvert code 58 (valid codes are 1-57) was accepted without a message; it turned off normal-flow limiting and changed the J1 depth from 0.912 to 0.754 ft
IO-14 5.3.0 base: FAIL
```

0.912 ft is the normal depth for 30 cfs in the 3 ft pipe at 5 % (n = 0.013). With code 58, J1 settles 0.16 ft lower.

**With the fix**:

```
Culvert code on C1   error   J1 depth at end (ft)
  0 (none)               0      0.912
 58 (invalid)          200   (rejected)
PASS: a culvert code outside 1-57 is rejected as invalid input
IO-14 5.3.0 patched: PASS
```

The report says `ERROR 211: invalid number 58 at line 32 of [XSECT] section:` (5.3.0) and `ERROR 211: invalid number 58.` (6.0.0, which returns error code 5 instead of 200).

## The fix

```diff
             i = atoi(tok[7]);
-            if ( i < 0 ) return error_setInpError(ERR_NUMBER, tok[7]);
+            if ( i < 0 || i > 57 )               // culvert codes are 1 - 57
+                return error_setInpError(ERR_NUMBER, tok[7]);
             else Link[j].xsect.culvertCode = i;
```

The 6.0.0 patch reports `ERR_NUMBER` for a code below 0 or above 57 in `handle_xsections()`, so a negative code is now rejected there as it is in legacy. Valid codes and 0 are read as before. **Effect on other models:** none of the regression decks has a culvert code, so none is affected.

`swmm_link_set_culvert_code()` in the 6.0.0 API also stores any value without a check (as the other 6.0.0 link setters do, e.g. `swmm_link_set_barrels()`). It is not changed here.
