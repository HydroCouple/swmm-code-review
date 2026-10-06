# CRASH-04: The object-count pass misreads a curve's type: a name-only line crashes, a quoted "Shape" overflows the heap

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Two crashes while the input is read, before any message is written. A `[CURVES]` line with only a curve name dereferences a NULL pointer (segmentation fault) where ERROR 203 is intended. A shape curve whose type is written `"Shape"` makes `project_validate()` write past a zero-length heap array; without sanitizers the custom section's tables are built in memory that belongs to something else. |
| **Reached from** | `[CURVES]` input: a first line holding only the name (also produced when a row longer than 1023 characters is split by the reader), or a quoted type keyword on a shape curve's first line |
| **5.3.0** | `addObject()` in [`src/legacy/engine/input.c:404-405`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L404-L405); the overflow is the write in `project_validate()` at [`project.c:235`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L235) |
| **5.2.4** | Same code, [`src/solver/input.c:394-395`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/input.c#L394-L395) and [`project.c:227`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L227) |
| **6.0.0** | No crash (there is no separate count pass), and the quoted type works. The name-only line is ignored without a message in `handle_curves()` ([`src/engine/input/handlers/TablesHandler.cpp:269`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/TablesHandler.cpp#L269)) instead of giving ERROR 203 |
| **Since** | 5.0.010, which added the shape-curve count to the count pass |
| **Fix** | Skip a missing type and a leading quote before matching: [`CRASH-04_swmm530.patch`](CRASH-04_swmm530.patch); report the name-only row in 6.0.0: [`CRASH-04_swmm600.patch`](CRASH-04_swmm600.patch) |

## The problem

The legacy reader reads the input file twice. The first pass, `input_countObjects()`, counts the objects of each kind so that their arrays can be allocated. The second pass reads the data. For curves, the count pass also counts shape curves, because the `Shape` array needs one element per shape curve, by looking at the token after the curve name on the curve's first line:

```c
// src/legacy/engine/input.c, addObject()
      case s_CURVE:
        // --- a Curve can span several lines
        if ( project_findObject(CURVE, id) < 0 )
        {
            ...
            // --- check for a conduit shape curve
            id = strtok(NULL, SEPSTR);
            if ( findmatch(id, CurveTypeWords) == SHAPE_CURVE )
                Nobjects[SHAPE]++;
        }
```

The two passes disagree in two ways:

1. **No type token.** If the line holds only the curve name, `strtok()` returns NULL, and `findmatch()` hands it to `match()`, which reads `str[k]`. The `[TRANSECTS]` case a few lines below checks `strtok()`'s result for NULL; this one does not. The read pass would have rejected the line: `table_readCurve()` returns ERROR 203 (too few items) for fewer than two tokens. Such a line also appears when a `[CURVES]` row is longer than the 1023 characters `fgets()` reads: the rest of the row is read as a new line, and if it holds a single token that is not an existing curve name, the count pass crashes. The review's fuzzer found three such inputs (mutants of `extran9.inp`, `Storage_Shape_Test.inp` and `fv_structures.inp`).
2. **Quoted type.** The read pass splits lines with `getTokens()`, which removes the quotes around a token, so `SH1 "Shape" 0 1` is a shape curve there ([`table.c:93`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/table.c#L93)). The count pass sees the token `"Shape"` with its quotes, which does not match `SHAPE`, so `Nobjects[SHAPE]` stays 0 and `Shape` is allocated with no elements ([`project.c:1064`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L1064)). `project_validate()` then fills it for every shape curve:

```c
// src/legacy/engine/project.c, project_validate()
    j = 0;
    for ( i=0; i<Nobjects[CURVE]; i++ )
    {
        if ( Curve[i].curveType == SHAPE_CURVE )
        {
            Curve[i].refersTo = j;
            Shape[j].curve = i;
            if ( !shape_validate(&Shape[j], &Curve[i]) )
```

`Shape[0].curve = i` is the first write past the allocation; `shape_validate()` then writes the shape's geometry tables (three tables of 51 doubles) after it.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-04_quoted-shape.inp`](CRASH-04_quoted-shape.inp) | Conduit C1 is `CUSTOM`, 2 ft high, with shape curve SH1 written `SH1 "Shape" 0 1` / `SH1 1 1`: a closed 2 x 2 ft square |
| [`CRASH-04_curve-id-only.inp`](CRASH-04_curve-id-only.inp) | A `[CURVES]` section whose only line is `CV1` |
| [`CRASH-04_test.c`](CRASH-04_test.c) | Opens and runs the shape deck and checks C1's full flow against Manning for the square (A = 4 ft², R = 0.5 ft, n = 0.013, S = 1/400: 14.40 cfs); then expects ERROR 203 for the name-only deck |
| [`CRASH-04_test6.c`](CRASH-04_test6.c) | The same with the 6.0.0 API, checking C1's full area and hydraulic radius |
| [`CRASH-04_swmm530.patch`](CRASH-04_swmm530.patch), [`CRASH-04_swmm600.patch`](CRASH-04_swmm600.patch) | The fixes |

```sh
tools/run-test.sh CRASH-04            # 5.2.4 and 5.3.0: CRASH, 6.0.0: FAIL
tools/run-test.sh CRASH-04 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.3.0 stops on the shape deck (5.2.4 the same, at `project.c:227`):

```
CRASH-04_quoted-shape.inp:
==17761==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x502000000210 at pc ...
WRITE of size 4 at 0x502000000210 thread T0
    #0 ... in project_validate .../src/legacy/engine/project.c:235:28
    #1 ... in swmm_open .../src/legacy/engine/swmm5.c:671:9
...
0x502000000211 is located 0 bytes after 1-byte region [0x502000000210,0x502000000211)
...
    #1 ... in createObjects .../src/legacy/engine/project.c:1064:30
CRASH-04 5.3.0 base: CRASH
```

The name-only deck, run on its own (`openswmm-legacy CRASH-04_curve-id-only.inp ...`):

```
../src/src/legacy/engine/input.c:813:17: runtime error: applying zero offset to null pointer
    #0 ... in match .../src/legacy/engine/input.c
    #1 ... in findmatch .../src/legacy/engine/input.c:796:11
```

6.0.0 builds the quoted shape correctly but accepts the name-only line:

```
CRASH-04_quoted-shape.inp:
    swmm_engine_open returned 0
    C1 full area 4.0000 ft2 (expected 4), full hydraulic radius 0.5000 ft (expected 0.5), run returned 0
CRASH-04_curve-id-only.inp:
    swmm_engine_open returned 0
FAIL: the name-only curve line is not reported as ERROR 203
CRASH-04 6.0.0 base: FAIL
```

**With the fix:**

```
CRASH-04_quoted-shape.inp:
    swmm_open returned 0
    C1 full flow 14.4019 cfs (expected 14.4019), run returned 0
CRASH-04_curve-id-only.inp:
    swmm_open returned 200
    report: ERROR 203: too few items at line 40 of [CURVE] section:
PASS: the name-only curve line gives ERROR 203 and the quoted Shape curve builds a 2 x 2 ft section (full flow 14.40 cfs)
CRASH-04 5.3.0 patched: PASS
```

```
CRASH-04_quoted-shape.inp:
    swmm_engine_open returned 0
    C1 full area 4.0000 ft2 (expected 4), full hydraulic radius 0.5000 ft (expected 0.5), run returned 0
CRASH-04_curve-id-only.inp:
    swmm_engine_open returned 5
    report: ERROR 203: too few items.
PASS: the name-only curve line gives ERROR 203 and the quoted Shape curve builds the 2 x 2 ft section
CRASH-04 6.0.0 patched: PASS
```

The fuzzer's `extran9.inp` mutant (a 2,017-character `[CURVES]` row) gives `ERROR 203: too few items at line 81 of [CURVE] section` with the patched 5.3.0 instead of the crash.

## The fix

5.3.0: test the token before matching it, and skip a leading quote as `getTokens()` does, so both passes classify the curve the same way.

```diff
             // --- check for a conduit shape curve
+            //     (the type may be missing, or quoted as getTokens allows)
             id = strtok(NULL, SEPSTR);
-            if ( findmatch(id, CurveTypeWords) == SHAPE_CURVE )
+            if ( id && *id == '"' ) id++;
+            if ( id && findmatch(id, CurveTypeWords) == SHAPE_CURVE )
                 Nobjects[SHAPE]++;
```

`match()` compares the keyword with the start of the token, so the closing quote left at the end of `Shape"` does not matter. A name-only line now reaches `table_readCurve()`, which reports ERROR 203 as intended.

6.0.0: `handle_curves()` reports a row with fewer than two tokens as ERROR 203, as legacy does, instead of skipping it:

```diff
+        // A row with only the curve name is too few items (legacy
+        // table_readCurve, ERR_ITEMS); v6 formerly ignored it silently.
+        if (tok.size() < 2) {
+            ctx.errors.push_back(format_error(ERR_ITEMS, tok[0]));
+            continue;
+        }
```

**Effect on other models.** None of the 73 regression decks has a shape curve, a quoted curve type or a name-only `[CURVES]` row, so neither patch changes how they are read. `Storage_Shape_Test.inp` and `extran9.inp` give byte-identical `.rpt` (apart from the run times) and `.out` files with the patched 5.3.0.

## Notes

- The count pass reads every section's object name with `strtok()`, so a quoted name with a space (e.g. `"J 1"`) is registered as `"J`, and the read pass then reports `ERROR 209: undefined object J 1`. That is an error message, not a crash, and is outside this issue, which only fixes the curve-type token.
