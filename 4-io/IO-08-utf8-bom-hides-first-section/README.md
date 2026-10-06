# IO-08: A UTF-8 byte order mark hides the first section of the input file

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | An input file saved as "UTF-8 with BOM" loses its first section without any message, in all three engines. With [INFLOWS] first, the 5 cfs inflow is never applied (external inflow 0.000 instead of 2.479 acre-ft) and the inflow line is printed as the report title. With [OPTIONS] first, 5.2.4 and 5.3.0 stop with the misleading `ERROR 191: simulation start date comes after ending date`, and 6.0.0 runs with default options (CFS, dynamic wave) from a start date of 02/06/8616. A file with [TITLE] first only gets a garbled title. |
| **Reached from** | Any input file that starts with the bytes EF BB BF: Notepad's "UTF-8" in Windows before 2019, "UTF-8 with BOM" in VS Code and other editors, Windows PowerShell 5.1's `-Encoding UTF8`, Python's `utf-8-sig` |
| **5.3.0** | `input_countObjects()` and `input_readData()` in [`src/legacy/engine/input.c:104`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L104) and [`:180-210`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/input.c#L180-L210) |
| **5.2.4** | Same code: [`src/solver/input.c:104`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/input.c#L104), [`:174-204`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/input.c#L174-L204) |
| **6.0.0** | Reproduces: `InputReader::read_stream()` tests `trimmed_raw.front() == '['` ([`InputReader.cpp:137`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/InputReader.cpp#L137)) and drops lines that come before the first recognised header |
| **Since** | Every release |
| **Fix** | Skip a BOM at the start of the file: [`IO-08_swmm530.patch`](IO-08_swmm530.patch), [`IO-08_swmm600.patch`](IO-08_swmm600.patch) |

## The problem

A UTF-8 byte order mark is three bytes, EF BB BF, that some editors and scripting tools put at the start of a text file to mark its encoding. It carries no text. SWMM reads it as part of the first line, so the first section header becomes `\xEF\xBB\xBF[INFLOWS]`, which does not start with `[` and is not taken as a header. Nothing reports this.

The test uses two versions of one model (J1 → C1 → J2 → C2 → O1, 6 hours), both saved with a BOM:

| Deck | First section | 5.2.4 and 5.3.0 | 6.0.0 |
|---|---|---|---|
| `IO-08_bom-inflows-first.inp` | [INFLOWS]: 5 cfs at J1 | Runs with no inflow: external inflow 0.000 acre-ft (2.479 without the BOM). The report title shows the `[INFLOWS]` header, with the BOM bytes in front of it, and the inflow line | Runs with no inflow, external inflow 0.000 |
| `IO-08_bom-options-first.inp` | [OPTIONS]: CMS, KINWAVE, 06/01/2020 0:00 to 6:00 | `ERROR 191: simulation start date comes after ending date` (no dates were read) | Runs with CFS and DYNWAVE, `Starting Date 02/06/8616`, `Ending Date N/A`, and ends at once |

A deck that starts with [TITLE], the usual layout, keeps its data (external inflow 2.479 acre-ft in all three engines); legacy then prints the BOM and `[TITLE]` as the first line of the title, and 6.0.0 prints no title.

## Why it happens

The two legacy passes treat the first section differently, and neither recognises it:

```c
// src/legacy/engine/input.c, input_countObjects()   (sect starts at -1)
        tok = strtok(wLine, SEPSTR);        // get first text token on line
        ...
        if ( *tok == '[' )
        ...
        if ( sect == s_OPTION ) errcode = readOption(line);
        else if ( sect >= 0 )   errcode = addObject(sect, tok);

// src/legacy/engine/input.c, input_readData()
    sect = 0;
    ...
        if (*Tok[0] == '[')
```

The object-count pass starts outside any section (`sect = -1`) and ignores every line until the second header, so the options in a first [OPTIONS] section are never read; that is why the dates are left unset and ERROR 191 appears. The data pass starts in [TITLE] (`sect = 0`), so the first section's lines are copied into the title. The 5.3.0 "Unknown section" warning does not fire either, because it also requires the line to start with `[`.

6.0.0 checks the first character of the trimmed line in the same way and ignores lines that come before the first header it recognises.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-08_bom-inflows-first.inp`](IO-08_bom-inflows-first.inp) | Starts with EF BB BF, then [INFLOWS] (5 cfs baseline at J1), [OPTIONS] (CFS, DYNWAVE, 01/01/2020 0:00 to 6:00), [TITLE] and the network |
| [`IO-08_bom-options-first.inp`](IO-08_bom-options-first.inp) | Starts with EF BB BF, then [OPTIONS] (CMS, KINWAVE, 06/01/2020 0:00 to 6:00), [TITLE], [INFLOWS] (0.15 cms at J1) and the network |
| [`IO-08_test.c`](IO-08_test.c) | Legacy toolkit. Correct: deck 1 runs with J1's lateral inflow 5.0 cfs at 3:00; deck 2 runs in CMS (`swmm_FLOWUNITS` 3) from day 43983 (06/01/2020) with 0.15 cms at J1 |
| [`IO-08_test6.c`](IO-08_test6.c) | The same check with the 6.0.0 API |

```sh
tools/run-test.sh IO-08            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-08 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.2.4 prints the same as 5.3.0):

```
---- IO-08 on 5.3.0 (base) ----
[INFLOWS] first: error   0, J1 inflow at 3:00 = 0.0000 cfs (expected 5.0)
[OPTIONS] first: error 191, flow units -1 (expected 3 = CMS), start day -1 (expected 43983), J1 inflow at 3:00 = -1.0000 (expected 0.15)
FAIL: the first section of a file saved with a UTF-8 byte order mark is not read ([INFLOWS] lost) ([OPTIONS] lost)
IO-08 5.3.0 base: FAIL
---- IO-08 on 6.0.0 (base) ----
[INFLOWS] first: error   0, J1 inflow at 3:00 = 0.0000 cfs (expected 5.0)
[OPTIONS] first: error   0, flow units CFS (expected CMS), start day 2453006 (expected 43983), J1 inflow at 3:00 = -1.0000 (expected 0.15)
FAIL: the first section of a file saved with a UTF-8 byte order mark is not read ([INFLOWS] lost) ([OPTIONS] lost)
IO-08 6.0.0 base: FAIL
```

(-1 means the value was never read: the deck did not open, or the run ended before 3:00.)

**With the fix:**

```
---- IO-08 on 5.3.0 (patched) ----
[INFLOWS] first: error   0, J1 inflow at 3:00 = 5.0000 cfs (expected 5.0)
[OPTIONS] first: error   0, flow units 3 (expected 3 = CMS), start day 43983 (expected 43983), J1 inflow at 3:00 = 0.1500 (expected 0.15)
PASS: files with a UTF-8 byte order mark are read like files without one
IO-08 5.3.0 patched: PASS
---- IO-08 on 6.0.0 (patched) ----
[INFLOWS] first: error   0, J1 inflow at 3:00 = 5.0000 cfs (expected 5.0)
[OPTIONS] first: error   0, flow units CMS (expected CMS), start day 43983 (expected 43983), J1 inflow at 3:00 = 0.1500 (expected 0.15)
PASS: files with a UTF-8 byte order mark are read like files without one
IO-08 6.0.0 patched: PASS
```

## The fix

5.3.0: both passes step over the three BOM bytes before reading the first line, and rewind if the file starts with anything else:

```diff
     rewind(Finp.file);
+    if ( fgetc(Finp.file) != 0xEF || fgetc(Finp.file) != 0xBB ||
+         fgetc(Finp.file) != 0xBF ) rewind(Finp.file);    // skip a UTF-8 BOM
     while ( fgets(line, MAXLINE, Finp.file) != NULL )
```

(`input_countObjects()` gets the same lines, with a `rewind()` first.) 6.0.0 removes the BOM from the first line in `InputReader::read_stream()`:

```diff
+        if (lines_read_ == 1 && raw_line.compare(0, 3, "\xEF\xBB\xBF") == 0)
+            raw_line.erase(0, 3);
```

Files without a BOM are read exactly as before; none of the 73 regression decks has one. Other non-ASCII text in a UTF-8 file (names, comments) is unaffected either way, because the reader treats it as opaque bytes.

The data pass also still treats any text before the first header as title text, while the count pass ignores it. With the BOM skipped this no longer matters for files written by tools, so it is left as it is.
