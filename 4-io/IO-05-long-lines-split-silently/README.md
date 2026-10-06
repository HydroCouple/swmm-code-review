# IO-05: Input lines longer than 1023 characters are split in two, and the check for them never fires

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | The part of a line beyond character 1023 is read as a separate line. A long description comment above an object turns into a bogus object, and a valid deck is refused with an error about text inside the comment (`ERROR 209: undefined object at`). A long data line gets an error about a fragment of itself, at the wrong line number, instead of `ERROR 201: too many characters in input line`. Where the fragment happens to parse, it is accepted silently. |
| **Reached from** | Any input line longer than 1023 characters (excluding the line break), in any section |
| **5.3.0** | `input_countObjects()` and `input_readData()` in [`src/legacy/engine/input.c:94`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L94) and [`:183-206`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L183-L206) |
| **5.2.4** | Same code: [`src/solver/input.c:94`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/input.c#L94), [`:177-200`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/input.c#L177-L200) |
| **6.0.0** | Not affected: `InputReader` reads whole lines with `std::getline()` ([`InputReader.cpp:118`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/InputReader.cpp#L118)), so it has no line limit and never reports ERROR 201 |
| **Since** | Every release |
| **Fix** | Read past the rest of a line that did not fit, and count it: [`IO-05_swmm530.patch`](IO-05_swmm530.patch) |

## The problem

The manual sets a limit on line length and names the error for it: "ERROR 201: too many characters in input line. A line in the input file cannot exceed 1024 characters." The legacy reader never reports it. It reads each line into a 1024-byte buffer, and whatever does not fit is read back as the next line.

Two decks show the two ways this goes wrong:

- `IO-05_long-comment.inp` is a valid one-conduit model with a 1151-character description line (`;The conduit runs under the parking lot ...`) above conduit C1, the way the SWMM GUI writes object descriptions. Characters 1024 onwards (`ed at both ends, roughness taken from ...`) no longer start with `;`, so they are read as a conduit named `ed` from node `at` to node `both`. 5.2.4 and 5.3.0 refuse the deck:

  ```
  ERROR 209: undefined object at at line 28 of [CONDUIT] section:
  ed at both ends, roughness taken from the relining report, no lateral connections ...
  ```

  Line 28 is the conduit C1 itself; the text comes from line 27, the comment.

- `IO-05_long-data-line.inp` writes the inflow series TS1 (2 cfs every 5 minutes from 0:00 to 6:00) as one [TIMESERIES] line of 1098 characters. The manual's answer is ERROR 201. 5.2.4 and 5.3.0 instead report `ERROR 211: invalid number 05:45:00 at line 38 of [TIMESERIES] section`: line 38 is blank, and the split-off tail (` 05:40:00 2.000 05:45:00 2.000 ...`) was read as a new series named `05:40:00` whose first time is `2.000` and whose first value, `05:45:00`, is not a number.

When the split falls so that the tail parses, there is no message at all. With six extra spaces after `TS1` on the same line, the tail starts with a value (` 2.000 05:40:00 2.000 ...`) and becomes a valid series named `2.000`; 5.3.0 then runs the model without an error or warning and reports an external inflow of 0.248 acre-ft instead of 0.991 (most of that loss comes from the 40-token limit of [IO-06](../IO-06-tokens-past-40-dropped/), which truncates the first part).

## Why it happens

```c
// src/legacy/engine/input.c, input_readData()
    while ( fgets(line, MAXLINE, Finp.file) != NULL )
    {
        ...
        // --- check if max. line length exceeded
        lineLength = (int)strlen(line);
        if ( lineLength >= MAXLINE )
        {
            // --- don't count comment if present
            comment = strchr(line, ';');
            if ( comment ) lineLength = (int)(comment - line);  // Pointer math here
            if ( lineLength >= MAXLINE )
            {
                inperr = ERR_LINE_LENGTH;
```

`fgets(line, MAXLINE, f)` stores at most `MAXLINE - 1` = 1023 characters and a terminating zero, so `strlen(line)` is at most 1023 and `lineLength >= MAXLINE` is never true. The characters left in the stream are returned by the next `fgets()` call as a new line, and `lineCount` is incremented again, which is why the errors name the following line. `input_countObjects()` has the same loop, so the tail is also counted as an object (conduit `ed` in the first deck, time series `05:40:00` in the second).

The check was written to exempt a long comment ("don't count comment if present"), which shows the intended behaviour: a long comment is fine, a long data line is ERROR 201.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-05_long-comment.inp`](IO-05_long-comment.inp) | One conduit J1 → O1, kinematic wave, 6 h; a 1151-character comment line (line 27) above C1 |
| [`IO-05_long-data-line.inp`](IO-05_long-data-line.inp) | The same network with its inflow from TS1, written as one 1098-character line (line 37) |
| [`IO-05_test.c`](IO-05_test.c) | Legacy toolkit. Correct: the comment deck opens with 2 nodes and 1 link and runs; the data-line deck is refused with ERROR 201 at line 37 (read from the report) |
| [`IO-05_test6.c`](IO-05_test6.c) | 6.0.0. Correct: lines are not split, so the comment deck opens and runs and the data-line deck opens (6.0.0 has no line limit) |

```sh
tools/run-test.sh IO-05            # 5.2.4 and 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh IO-05 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same):

```
---- IO-05 on 5.3.0 (base) ----
long comment line:   error code 200, -1 nodes, -1 links  ERROR 209: undefined object at at line 28 of [CONDUIT] section:
long data line:      error code 200  ERROR 211: invalid number 05:45:00 at line 38 of [TIMESERIES] section:
FAIL: the tail of a long comment line is parsed as data; a long data line is not reported as ERROR 201 at line 37
IO-05 5.3.0 base: FAIL
---- IO-05 on 6.0.0 (base) ----
long comment line:   error code 0, 2 nodes, 1 links
long data line:      error code 0  
PASS: lines longer than 1024 characters are read whole (comment ignored, data line opens)
IO-05 6.0.0 base: PASS
```

**With the fix:**

```
---- IO-05 on 5.3.0 (patched) ----
long comment line:   error code 0, 2 nodes, 1 links  
long data line:      error code 200  ERROR 201: too many characters in input line at line 37 of [TIMESERIES] section:
PASS: a long comment line is ignored, and a long data line is refused with ERROR 201 at its own line number
IO-05 5.3.0 patched: PASS
```

## The fix

Both passes check whether `fgets()` stopped before the end of the line (no `'\n'` in the buffer) and, if so, read and discard the rest of the line, so it is never parsed. `input_readData()` counts the discarded characters into `lineLength` before it skips blank and comment lines, and the existing check then works as written:

```diff
         lineCount++;
+        lineLength = (int)strlen(line);
+
+        // --- skip the rest of a line too long for the buffer, counting
+        //     its characters, so that it is not read as a new line
+        if ( strchr(line, '\n') == NULL )
+        {
+            int c;
+            while ( (c = fgetc(Finp.file)) != EOF && c != '\n' )
+                if ( c != '\r' ) lineLength++;
+        }
         sstrncpy(wLine, line, MAXLINE);
         ...
         // --- check if max. line length exceeded
-        lineLength = (int)strlen(line);
         if ( lineLength >= MAXLINE )
```

A long comment, or a data line whose comment starts within the first 1023 characters, is accepted with the rest of the comment ignored; a longer data part is ERROR 201 at its own line number. The limit is 1023 characters plus the line break, 1024 in all. Lines that fit the buffer are handled exactly as before, so no model that runs today changes; none of the 73 regression decks has a line longer than 1000 characters.

6.0.0 needs no change for this issue. It reads lines of any length, which is more permissive than the documented limit; a deck with a 1100-character line therefore runs in 6.0.0 and is refused by the fixed 5.3.0.

## Notes

- 6.0.0 reads the long [TIMESERIES] line whole but takes only its first time/value pair (`handle_timeseries()` reads `tok[1]` and `tok[2]` of each line and ignores the rest), so the second deck runs in 6.0.0 with an external inflow of 0.000 instead of 0.991 acre-ft. That is a separate 6.0.0 defect, which affects any [TIMESERIES] line with more than one pair, whatever its length.
- 5.2.4 and 5.3.0 also keep only the first 40 tokens of a line ([IO-06](../IO-06-tokens-past-40-dropped/)), so a long line that is split here would lose tokens even if it were read whole.
