# IO-07: A ';' inside a quoted file name is read as the start of a comment

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A quoted token is cut at its first `;`. A time series, rain or interface file whose path contains a semicolon cannot be used: the run stops with `ERROR 361: could not open external file used for Time Series TS1`, and the message does not show the shortened name. Semicolons are legal in file and folder names on Windows, macOS and Linux. |
| **Reached from** | Any quoted token containing `;`: file names in [TIMESERIES] `FILE`, [RAINGAGES] `FILE`, [FILES], `TEMPDIR` |
| **5.3.0** | `getTokens()` in [`src/legacy/engine/input.c:887-889`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L887-L889) |
| **5.2.4** | Same code: [`src/solver/input.c:902-904`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/input.c#L902-L904) |
| **6.0.0** | Not affected: `Tokenizer::strip_comment()` ignores a `;` inside quotes ([`Tokenizer.cpp:44-55`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/Tokenizer.cpp#L44-L55)) |
| **Since** | Every release |
| **Fix** | Skip quoted tokens when looking for the comment: [`IO-07_swmm530.patch`](IO-07_swmm530.patch) |

## The problem

Quotes exist in the input format so that a token can contain characters that would otherwise end it, typically a file path. `getTokens()` documents this itself ("Text between quotes is treated as a single token"). It does not hold for `;`. The test deck reads its inflow series from a file whose name contains one:

```
[TIMESERIES]
TS1  FILE  "IO-07_flow;v2.dat"
```

5.2.4 and 5.3.0 read the file name as `IO-07_flow`, cannot open it, and stop with `ERROR 361: could not open external file used for Time Series TS1`. 6.0.0 reads the series and the run gets its 2 cfs inflow.

## Why it happens

```c
// src/legacy/engine/input.c, getTokens()
    // --- truncate s at start of comment 
    c = strchr(s,';');
    if (c) *c = '\0';
    len = (int)strlen(s);

    // --- scan s for tokens until nothing left
    while (len > 0 && n < MAXTOKS)
    {
        ...
            if (*s == '"')                  // token begins with quote
            {
                s++;                        // start token after quote
                len--;                      // reduce length of s
                m = (int)strcspn(s,"\"\n"); // find end quote or new line
            }
```

The line is cut at the first `;` before the scan for tokens starts, so by the time the quote is seen, the rest of the token is gone. The quoted token then runs to the end of what is left, `IO-07_flow`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-07_quoted-semicolon.inp`](IO-07_quoted-semicolon.inp) | One conduit J1 → O1, kinematic wave, 6 h; J1's inflow is series TS1, read from `"IO-07_flow;v2.dat"` |
| [`IO-07_flow;v2.dat`](IO-07_flow%3Bv2.dat) | The series file: 2 cfs at 0:00 and at 6:00 |
| [`IO-07_test.c`](IO-07_test.c) | Legacy toolkit: runs the deck and reads J1's lateral inflow at 3:00. Correct: no error and 2.0 cfs |
| [`IO-07_test6.c`](IO-07_test6.c) | The same check with the 6.0.0 API |

```sh
tools/run-test.sh IO-07            # 5.2.4 and 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh IO-07 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same):

```
---- IO-07 on 5.3.0 (base) ----
error code 361, J1 inflow at 3:00 = -1.0000 cfs (expected 2.0)
FAIL: the quoted file name "IO-07_flow;v2.dat" is cut at the ';' (error 361)
IO-07 5.3.0 base: FAIL
---- IO-07 on 6.0.0 (base) ----
error code 0, J1 inflow at 3:00 = 2.0000 cfs (expected 2.0)
PASS: a ';' inside a quoted file name is part of the name; TS1 is read from IO-07_flow;v2.dat
IO-07 6.0.0 base: PASS
```

(-1 means the run never reached 3:00.)

**With the fix:**

```
---- IO-07 on 5.3.0 (patched) ----
error code 0, J1 inflow at 3:00 = 2.0000 cfs (expected 2.0)
PASS: a ';' inside a quoted file name is part of the name; TS1 is read from IO-07_flow;v2.dat
IO-07 5.3.0 patched: PASS
```

## The fix

Find the comment by scanning the line, and step over a quoted token, using the same rule the tokenizer uses for one (a `"` at the start of a token), when it has a closing quote on the line:

```diff
-    // --- truncate s at start of comment 
-    c = strchr(s,';');
-    if (c) *c = '\0';
+    // --- truncate s at start of comment (a ';' inside a quoted token,
+    //     e.g. a file name, is part of the token)
+    for (c = s; *c; c++)
+    {
+        if ( *c == '"' && (c == s || strchr(SEPSTR, c[-1])) &&
+             strchr(c+1, '"') ) c = strchr(c+1, '"');
+        else if ( *c == ';' )
+        {
+            *c = '\0';
+            break;
+        }
+    }
```

A line is cut differently from before only if a `;` lies between an opening and a closing quote. Without a closing quote, the line is still cut at its first `;`, so an unbalanced quote cannot swallow a comment. None of the 73 regression decks has a `;` inside quotes (222 of their lines contain quotes), so none changes.

The count pass (`input_countObjects()`) only reads the first token of each line, with `strtok()`, and does not handle quotes at all. That matters for quoted object names, not for file names, and is not changed here.
