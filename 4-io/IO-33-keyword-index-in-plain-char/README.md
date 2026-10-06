# IO-33: Misspelt [FILES] and [REPORT] keywords are not caught where char is unsigned, and never in 6.0.0

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Input validation depends on the platform. On Linux on ARM/aarch64 (for example AWS Graviton or a Raspberry Pi), PowerPC and s390x, 5.2.4 and 5.3.0 drop a [FILES] line with a misspelt mode, such as `SVAE HOTSTART x.hsf`, without a message, so the hot start file the user asked for is never written; on x86 the same deck stops with ERROR 205. A misspelt [REPORT] keyword is reported against its value (`invalid keyword YES`). 6.0.0 ignores misspelt [FILES] modes, [FILES] file types and [REPORT] keywords on every platform. |
| **Reached from** | [FILES] lines with an unknown mode; [REPORT] lines with an unknown keyword; in 6.0.0 also [FILES] lines with an unknown file type |
| **5.3.0** | `iface_readFileParams()` in [`src/legacy/engine/iface.c:79-87`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/iface.c#L79-L87) and `report_readOptions()` in [`src/legacy/engine/report.c:101-105`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/report.c#L101-L105) |
| **5.2.4** | Same code: [`src/solver/iface.c:77-84`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/iface.c#L77-L84), [`src/solver/report.c:101-105`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/report.c#L101-L105) |
| **6.0.0** | Reproduces on every platform: `handle_files()` skips an unknown mode "silently (legacy behaviour)" and ignores an unknown type ([`FilesHandler.cpp:72-114`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/FilesHandler.cpp#L72-L114)); `handle_report()` ignores an unknown keyword ([`ControlsHandler.cpp:136-176`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/ControlsHandler.cpp#L136-L176)) |
| **Since** | Every release (legacy); 6.0.0's new input reader |
| **Fix** | Legacy: keep the index in an `int` ([`IO-33_swmm530.patch`](IO-33_swmm530.patch)). 6.0.0: report unknown words as ERROR 205 ([`IO-33_swmm600.patch`](IO-33_swmm600.patch)) Apply after IO-31 (its `Requires:` line). |

## The problem

Three one-word typos, each in an otherwise valid deck:

| Deck | Line | x86 legacy (char signed) | Legacy where char is unsigned | 6.0.0 |
|---|---|---|---|---|
| `IO-33_files-typo.inp` | `SVAE HOTSTART "IO-33.hsf"` | `ERROR 205: invalid keyword SVAE` | Runs; the line is dropped and no hot start file is written | Runs; the line is dropped |
| `IO-33_report-typo.inp` | `CONTINUTY YES` | `ERROR 205: invalid keyword CONTINUTY` | `ERROR 205: invalid keyword YES` | Runs; the line is ignored |
| `IO-33_files-type-typo.inp` | `SAVE HOTSTRAT "IO-33.hsf"` | `ERROR 205: invalid keyword HOTSTRAT` | `ERROR 205: invalid keyword HOTSTRAT` | Runs; the line is ignored |

Whether `char` is signed is up to the platform ABI. It is signed on x86 and x86-64 (all systems), on Windows on ARM64 and on macOS on Apple silicon, and unsigned on Linux on ARM and aarch64, PowerPC and s390x. A deck that x86 users see refused is accepted on an ARM Linux server, and the user who meant to save a hot start file finds none. 6.0.0 accepts all three decks everywhere; its comment calls skipping an unknown mode "legacy behaviour", which is what legacy does only where `char` is unsigned.

## Why it happens

```c
// src/legacy/engine/iface.c, iface_readFileParams()
    char  k;
    ...
    k = (char)findmatch(tok[0], FileModeWords);
    if ( k < 0 ) return error_setInpError(ERR_KEYWORD, tok[0]);
```

`findmatch()` returns an `int`, -1 when the word matches no keyword. Converted to an unsigned `char`, -1 becomes 255, `k < 0` is false, and the function goes on with a mode of 255. For `HOTSTART` neither the `USE_FILE` nor the `SAVE_FILE` branch matches, so the line has no effect; for `RAINFALL` and `RUNOFF` the file's mode becomes 255. `report_readOptions()` has the same declaration and cast; there `k = 255` falls through to `default: return error_setInpError(ERR_KEYWORD, tok[1])`, which names the second token. The file type in `iface_readFileParams()` is held in an `int` (`j`), so a misspelt type is caught on every platform.

6.0.0 compares the words with fixed strings and simply moves on when none matches:

```cpp
// src/engine/input/handlers/FilesHandler.cpp, handle_files()
        if      (mode_word == "SAVE") mode = FileMode::SAVE;
        else if (mode_word == "USE")  mode = FileMode::USE;
        else continue;  // unrecognised mode — skip silently (legacy behaviour)
```

## How to reproduce

| File | What it is |
|---|---|
| [`IO-33_files-typo.inp`](IO-33_files-typo.inp) | One conduit J1 → O1, 6 hours; `[FILES] SVAE HOTSTART "IO-33.hsf"` |
| [`IO-33_report-typo.inp`](IO-33_report-typo.inp) | The same model; `[REPORT] CONTINUTY YES` |
| [`IO-33_files-type-typo.inp`](IO-33_files-type-typo.inp) | The same model; `[FILES] SAVE HOTSTRAT "IO-33.hsf"` |
| [`IO-33_test.c`](IO-33_test.c) | Legacy toolkit: opens each deck and reads the first error in the report. Correct: ERROR 205 naming the misspelt word. Prints whether `char` is signed in the build |
| [`IO-33_test6.c`](IO-33_test6.c) | The same check with the 6.0.0 API (`swmm_engine_run`) |

```sh
tools/run-test.sh IO-33            # on x86: 5.2.4 and 5.3.0 PASS, 6.0.0 FAIL
tools/run-test.sh IO-33 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

The harness builds on x86-64, where `char` is signed, so the legacy test passes there. To show the legacy defect, the 5.3.0 engine sources at cb3e192b were compiled into a shared library with `gcc -funsigned-char` (the aarch64-Linux convention), unpatched and with `IO-33_swmm530.patch`, and the same `IO-33_test.c` was linked against each.

**Without the fix**, 5.3.0 built with `-funsigned-char`:

```
plain char is unsigned in this build
IO-33_files-typo.inp   open code   0, no error
IO-33_report-typo.inp  open code 200, ERROR 205: invalid keyword YES at line 37 of [REPORT] section:
IO-33_files-type-typo.inp open code 200, ERROR 205: invalid keyword HOTSTRAT at line 37 of [FILE] section:
FAIL: a misspelt keyword is not reported as ERROR 205 naming it ([FILES] SVAE) ([REPORT] CONTINUTY)
```

and 6.0.0 in the harness:

```
---- IO-33 on 6.0.0 (base) ----
IO-33_files-typo.inp   run code   0, no error
IO-33_report-typo.inp  run code   0, no error
IO-33_files-type-typo.inp run code   0, no error
FAIL: a misspelt keyword is not reported as ERROR 205 naming it ([FILES] SVAE) ([REPORT] CONTINUTY) ([FILES] HOTSTRAT)
IO-33 6.0.0 base: FAIL
```

5.2.4 and 5.3.0 in the harness (x86-64) give the correct result:

```
---- IO-33 on 5.3.0 (base) ----
plain char is signed in this build
IO-33_files-typo.inp   open code 200, ERROR 205: invalid keyword SVAE at line 37 of [FILE] section:
IO-33_report-typo.inp  open code 200, ERROR 205: invalid keyword CONTINUTY at line 37 of [REPORT] section:
IO-33_files-type-typo.inp open code 200, ERROR 205: invalid keyword HOTSTRAT at line 37 of [FILE] section:
PASS: misspelt [FILES] mode and type and [REPORT] keyword are refused with ERROR 205 naming the word
IO-33 5.3.0 base: PASS
```

**With the fix**, 5.3.0 built with `-funsigned-char`:

```
plain char is unsigned in this build
IO-33_files-typo.inp   open code 200, ERROR 205: invalid keyword SVAE at line 37 of [FILE] section:
IO-33_report-typo.inp  open code 200, ERROR 205: invalid keyword CONTINUTY at line 37 of [REPORT] section:
IO-33_files-type-typo.inp open code 200, ERROR 205: invalid keyword HOTSTRAT at line 37 of [FILE] section:
PASS: misspelt [FILES] mode and type and [REPORT] keyword are refused with ERROR 205 naming the word
```

and in the harness:

```
IO-33 5.3.0 patched: PASS
---- IO-33 on 6.0.0 (patched) ----
IO-33_files-typo.inp   run code   5, ERROR 205: invalid keyword SVAE.
IO-33_report-typo.inp  run code   5, ERROR 205: invalid keyword CONTINUTY.
IO-33_files-type-typo.inp run code   5, ERROR 205: invalid keyword HOTSTRAT.
PASS: misspelt [FILES] mode and type and [REPORT] keyword are refused with ERROR 205 naming the word
IO-33 6.0.0 patched: PASS
```

## The fix

Legacy: declare `k` as `int` and drop the cast, in both functions:

```diff
-    char  k;
+    int   k;
     ...
-    k = (char)findmatch(tok[0], FileModeWords);
+    k = findmatch(tok[0], FileModeWords);
```

`k` is later stored in `char` fields (`Frain.mode`, `RptFlags.*`) whose values (0 to 3, or 0/1) fit. Where `char` is signed nothing changes. These are the only two places in either legacy version that pass `findmatch()` through a `(char)` cast.

6.0.0: `handle_files()` and `handle_report()` push ERROR 205 for a mode, file type or keyword that legacy would not accept. Legacy `findmatch()` accepts any word that begins with a keyword (`NODE`, `NODES` and `NODESTATS` all match `NODE`), so the new checks test prefixes against the legacy keyword lists, and `NO` and `SCRATCH` stay valid [FILES] modes that name no file. No deck that legacy accepts is refused. `ControlsHandler.cpp` gains an include of `ErrorCodes.hpp` for `ERR_KEYWORD`.

## Notes

- 6.0.0 only acts on exact keywords, so the legacy prefix forms it now lets through without an error (for example `NODE ALL`) are still ignored rather than applied. That is a separate parity question and is not changed here.
