# IO-06: Values past the 40th token on an input line are dropped without a message

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A curve or time series written with more than 19 pairs on one line loses the rest of the line, with no warning. In the test a 41-point storage curve keeps only its first 19 points: the storage unit fills to 25.15 ft and spills 1,081 ft³ over its weir instead of standing at 11.925 ft. A [TRANSECTS] GR line with more than 19 stations is refused with a misleading "too few items" error. |
| **Reached from** | Any input line with more than 40 whitespace-separated items: [CURVES], [TIMESERIES] and [TRANSECTS] GR lines with many pairs, long [TREATMENT] or [GWF] expressions |
| **5.3.0** | `getTokens()` in [`src/legacy/engine/input.c:893`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L893), with `MAXTOKS` 40 in [`consts.h:82`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/consts.h#L82) |
| **5.2.4** | Same code: [`src/solver/input.c:908`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/input.c#L908), [`consts.h:26`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/consts.h#L26) |
| **6.0.0** | Not affected: `Tokenizer::tokenize()` returns every token ([`Tokenizer.cpp:82`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/Tokenizer.cpp#L82)) and `handle_curves()` reads every pair on a line |
| **Since** | Every release |
| **Fix** | Make the token array large enough for any line the reader accepts: [`IO-06_swmm530.patch`](IO-06_swmm530.patch) |

## The problem

The input format lets a curve or time series put any number of x-y pairs on a line, and lets a transect's GR line carry any number of stations. The legacy tokenizer keeps the first 40 items of a line and ignores the rest. Nothing is reported, and the manual mentions no such limit.

The test deck writes storage curve SC1 as a single line of 84 tokens (name, `STORAGE`, 41 depth/area pairs): an area of 100 ft² from 0 to 9 ft and 1000 ft² from 9.5 to 20 ft. A constant 1 cfs flows into storage unit SU1 for one hour, and its only outlet is a weir with its crest at 25 ft. SU1 therefore holds 3600 ft³ at 1:00: 900 ft³ below 9 ft, 275 ft³ between 9 and 9.5 ft, and 2425 ft³ at 1000 ft² above that, a depth of 9.5 + 2.425 = **11.925 ft**.

5.2.4 and 5.3.0 keep the name, the type and the first 19 pairs, so SC1 ends at 9 ft with an area of 100 ft², and the storage lookup extends that area upwards. SU1 rises past the 25-ft weir crest, peaks at 25.16 ft at 0:42 and spills 1,081 ft³. The run reports no error or warning, and its continuity error is -0.029 %. 6.0.0 reads the whole line and stands at 11.92 ft.

How the cut shows depends on the line:

- an even number of values survives (name and type, then pairs, as here, or a time-series line whose last kept item is a value): the rest is dropped silently;
- an odd number survives: a [TRANSECTS] GR line keeps `GR` plus 39 values, and `transect_readParams()` refuses it with `ERROR 203: too few items`, about a line that has too many (checked with extran8a's transect 91 written as one GR line of 26 stations);
- a [TREATMENT] or [GWF] expression is concatenated from the first 40 tokens, so a long expression is cut short.

## Why it happens

```c
// src/legacy/engine/consts.h
#define MAXTOKS 40 // Max. items per line of input

// src/legacy/engine/input.c, getTokens()
    // --- scan s for tokens until nothing left
    while (len > 0 && n < MAXTOKS)
    {
        ...
    }
    return n;
```

The loop stops at 40 tokens whether or not text remains, and `getTokens()` returns 40 as if that were the whole line. `Tok[]` is a static array of `MAXTOKS` pointers, the only use of the constant.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-06_one-line-curve.inp`](IO-06_one-line-curve.inp) | Storage unit SU1 (curve SC1 on one 84-token line), 1 cfs inflow for 1 hour, weir W1 with its crest at 25 ft to outfall O1 |
| [`IO-06_test.c`](IO-06_test.c) | Legacy toolkit: runs the model and checks SU1's depth at 1:00 against 11.925 ft (tolerance 0.05 ft, about 50 ft³ or 1.4 % of the volume); also prints the volume over the weir |
| [`IO-06_test6.c`](IO-06_test6.c) | The same check with the 6.0.0 API |

```sh
tools/run-test.sh IO-06            # 5.2.4 and 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh IO-06 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same):

```
---- IO-06 on 5.3.0 (base) ----
SU1 depth at 1:00   expected 11.925 ft, got 25.153 ft (max 25.156 ft)
weir W1 outflow     expected 0 ft3,      got 1081 ft3
error code 0
FAIL: storage curve written on one line is cut after 40 tokens (depth 25.153 ft instead of 11.925 ft)
IO-06 5.3.0 base: FAIL
---- IO-06 on 6.0.0 (base) ----
SU1 depth at 1:00   expected 11.925 ft, got 11.922 ft (max 11.922 ft)
weir W1 outflow     expected 0 ft3,      got 0 ft3
error code 0
PASS: all 41 points of the one-line storage curve are read (depth 11.922 ft at 1:00)
IO-06 6.0.0 base: PASS
```

**With the fix:**

```
---- IO-06 on 5.3.0 (patched) ----
SU1 depth at 1:00   expected 11.925 ft, got 11.922 ft (max 11.922 ft)
weir W1 outflow     expected 0 ft3,      got 0 ft3
error code 0
PASS: all 41 points of the one-line storage curve are read (depth 11.922 ft at 1:00)
IO-06 5.3.0 patched: PASS
```

The patched 5.3.0 and 6.0.0 give the same depth.

## The fix

`getTokens()` works on a copy of the line of at most 1023 characters, which can hold at most 512 tokens (one-character tokens separated by single blanks). Sizing the token array for that many means no token of a line that is read can be dropped:

```diff
-#define MAXTOKS 40 // Max. items per line of input
+#define MAXTOKS (MAXLINE/2) // Max. items per line of input (all that fit in a line)
```

Every parser takes its token count from `getTokens()`; none assumes 40 or fewer. The loops that consume a variable number of tokens are bounded by their own limits (24 pattern factors, the transect station limit, `MAXLINE` for concatenated expressions). Lines of 40 tokens or fewer are parsed exactly as before, and no line of the 73 regression decks has more than 40 tokens, so their results do not change. The cost is that `getTokens()` clears 512 pointers per line instead of 40.

Lines longer than 1023 characters are a separate problem: [IO-05](../IO-05-long-lines-split-silently/).

## Notes

- With more than 19 pairs on a [TIMESERIES] line, 6.0.0 has its own problem: `handle_timeseries()` reads only the first time/value pair of each line (see [IO-05](../IO-05-long-lines-split-silently/)'s notes). That is why this issue is tested with a curve.
