# IO-13: Invalid cross-section geometry is reported as "invalid number" with no number

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Diagnostics only: the deck is rejected, as it should be, but the message is `ERROR 211: invalid number` followed by nothing. In 5.2.4 and 5.3.0 the input line is echoed below it, so the conduit can be found but not the fault, and often no single number on the line is invalid (a sediment depth larger than the diameter). In 6.0.0 the message is `ERROR 211: invalid number .` with no line, link or value, so there is no way to tell which conduit it is about. |
| **Reached from** | Any `[XSECTIONS]` line that `xsect_setParams()` refuses: Geom1 <= 0, FILLED_CIRCULAR sediment >= diameter, closed rectangle without width, unknown size code, and so on |
| **5.3.0** | `link_readXsectParams()` in [`src/legacy/engine/link.c:250`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L250) |
| **5.2.4** | Same code, [`src/solver/link.c:247`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L247) |
| **6.0.0** | Reproduces, without even the line number: [`src/engine/input/handlers/LinksHandler.cpp:442`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/LinksHandler.cpp#L442) (Geom1 <= 0) and [`src/engine/input/PostParseResolver.cpp:3347`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/PostParseResolver.cpp#L3347) (everything else) |
| **Since** | Every SWMM 5 release (5.0.022 has the same line) |
| **Fix** | Report `ERROR 119: invalid cross section for Link <name>` instead: [`IO-13_swmm530.patch`](IO-13_swmm530.patch), [`IO-13_swmm600.patch`](IO-13_swmm600.patch) |

## The problem

Most input errors name the offending item: `invalid number 0.0x`, `undefined object J7`, `invalid keyword SEMI_ELLIPTICAL`. When the geometry of a cross section is refused, the message names nothing. For a deck with three bad cross sections, 5.3.0 writes:

```
  ERROR 211: invalid number  at line 36 of [XSECT] section:
  C1     FILLED_CIRCULAR  2      3      0      0
```

The line is shown, so the conduit can be found. The message does not say which value is wrong, and for C1 none is: 2 is a valid diameter and 3 is a valid sediment depth. The fault is that the sediment fills the whole pipe. The same message covers a zero diameter, an elliptical size code that does not exist, a closed rectangle with no width, and every other check in `xsect_setParams()`. (6.0.0 does not check the width of a closed rectangle at all and runs it with zero area; that is a separate defect.)

6.0.0 writes its input errors without line numbers, and copied the empty token, so for the same deck the whole diagnosis is:

```
  ERROR 211: invalid number .
  ERROR 211: invalid number .
  ERROR 211: invalid number .
```

## Why it happens

`xsect_setParams()` only returns TRUE or FALSE, and the caller has no token to name:

```c
// src/legacy/engine/link.c, link_readXsectParams()
        if ( !xsect_setParams(&Link[j].xsect, k, x, UCF(LENGTH)) )
        {
            return error_setInpError(ERR_NUMBER, "");
        }
```

6.0.0 reproduces the message on purpose (the comments next to both calls cite `link.c:250`):

```cpp
// src/engine/input/handlers/LinksHandler.cpp, handle_xsections()
            ctx.errors.push_back(format_error(ERR_NUMBER, ""));
// src/engine/input/PostParseResolver.cpp, resolve_cross_references()
                    ctx.errors.push_back(format_error(ERR_NUMBER, ""));
```

## How to reproduce

| File | What it is |
|---|---|
| [`IO-13_bad-xsections.inp`](IO-13_bad-xsections.inp) | C1 `FILLED_CIRCULAR 2 3` (sediment above the crown), C2 `CIRCULAR 0` (zero diameter), C3 `HORIZ_ELLIPSE 2 3.17 99` (size code 99; there are 23) |
| [`IO-13_test.c`](IO-13_test.c) | Opens the deck with the legacy toolkit (5.2.4, 5.3.0) and checks that the report's ERROR lines name C1, C2 and C3 |
| [`IO-13_test6.c`](IO-13_test6.c) | The same through `swmm_engine_run()` (6.0.0) |

The test counts only the ERROR line itself, not the echoed input line, because the echo does not say what is wrong and 6.0.0 does not print one.

```sh
tools/run-test.sh IO-13            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-13 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**:

```
swmm_open returned 200; error messages in the report:
  ERROR 211: invalid number  at line 36 of [XSECT] section:
  ERROR 211: invalid number  at line 37 of [XSECT] section:
  ERROR 211: invalid number  at line 38 of [XSECT] section:
FAIL: 3 error messages, but none names C1, C2, C3
IO-13 5.3.0 base: FAIL

swmm_engine_run returned 5; error messages in the report:
  ERROR 211: invalid number .
  ERROR 211: invalid number .
  ERROR 211: invalid number .
FAIL: 3 error messages, but none names C1, C2, C3
IO-13 6.0.0 base: FAIL
```

**With the fix**:

```
swmm_open returned 200; error messages in the report:
  ERROR 119: invalid cross section for Link C1.at line 36 of [XSECT] section:
  ERROR 119: invalid cross section for Link C2.at line 37 of [XSECT] section:
  ERROR 119: invalid cross section for Link C3.at line 38 of [XSECT] section:
PASS: each invalid cross section is reported with the name of its conduit
IO-13 5.3.0 patched: PASS

swmm_engine_run returned 5; error messages in the report:
  ERROR 119: invalid cross section for Link C2.
  ERROR 119: invalid cross section for Link C1.
  ERROR 119: invalid cross section for Link C3.
PASS: each invalid cross section is reported with the name of its conduit
IO-13 6.0.0 patched: PASS
```

The missing space in "C1.at line" comes from `report_writeInputErrorMsg()`, which appends "at line ..." directly to the message. Messages that end in a period run on in the same way: `ERROR 235: invalid infiltration parameters.at line ...` already does this in every release.

## The fix

Use the existing message for an invalid cross section, which names the link:

```diff
         if ( !xsect_setParams(&Link[j].xsect, k, x, UCF(LENGTH)) )
         {
-            return error_setInpError(ERR_NUMBER, "");
+            return error_setInpError(ERR_XSECT, tok[0]);
         }
```

The 6.0.0 patch makes the same change in `LinksHandler.cpp` (with `tok[0]`) and `PostParseResolver.cpp` (with `ctx.link_names.name_of(j)`), and updates the two comments that describe the legacy message. Only the report text changes: the run is still rejected, `swmm_open()` still returns 200 (ERR_INPUT) and 6.0.0 still returns its own error code. The error number in the report changes from 211 to 119. Results of valid models are not affected.

The message still does not say which rule was broken. That would take an error code per check in `xsect_setParams()`, which is a larger change than this review proposes.
