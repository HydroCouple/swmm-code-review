# CRASH-20: 5.2.4 overflows its 256-byte error-text buffer on a long input token

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | 5.2.4 only. An input error on a token of 256 characters or more (a misspelt ID, a garbled number, a pasted line without separators) writes past the global `ErrString[256]` into the variables that follow it. The run already fails with an input error, but the overflow can crash it or garble the report. It was the most frequent crash in the review's fuzzing of 5.2.4: 395 of 3,603 mutated decks, from every section type |
| **Reached from** | Any input error whose subject is a token of 256+ characters: undefined names (ERROR 209), invalid numbers (ERROR 211), invalid keywords (ERROR 205), ... |
| **5.3.0** | Not affected: `ErrString` is `char ErrString[MAXMSG]` (1024 bytes, as long as an input line) in [`src/legacy/engine/error.c:31`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/error.c#L31) |
| **5.2.4** | `char ErrString[256]` and `strcpy(ErrString, s)` in `error_setInpError()`, [`src/solver/error.c:28`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/error.c#L28) and [`error.c:47`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/error.c#L47) |
| **6.0.0** | Not affected: error messages are built as `std::string` ([`src/engine/core/ErrorCodes.cpp:284`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/ErrorCodes.cpp#L284)) |
| **Since** | Every release in the repository (initial commit, 2014); fixed in the HydroCouple fork by commit 91de2fdd (February 2024), from which 5.3.0 descends. EPA's `develop` branch (07c371f8, February 2025) still declares `ErrString[256]` |
| **Fix** | None here (5.2.4 is not patched); the 5.3.0 change is the fix to take into EPA's code |

## The problem

Every legacy input reader reports an error by passing the offending token to `error_setInpError()`, which keeps it for the error message:

```c
// src/solver/error.c (5.2.4)
char  ErrString[256];
...
int  error_setInpError(int errcode, char* s)
{
    strcpy(ErrString, s);
    return errcode;
}
```

Input lines are read with `fgets(line, MAXLINE, ...)` and `MAXLINE` is 1024, so a token can hold up to 1023 characters. One of 256 or more overflows `ErrString`. The test deck has a conduit whose From node is a misspelt 300-character name; 5.2.4 writes 301 bytes into the 256-byte buffer:

```
ERROR: AddressSanitizer: global-buffer-overflow on address 0x7fab6a0866a0
WRITE of size 301 at 0x7fab6a0866a0 thread T0
    #1 in error_setInpError .../src/solver/error.c:47:5
    #2 in conduit_readParams .../src/solver/link.c
0x7fab6a0866a0 is located 0 bytes after global variable 'ErrString' defined in '.../src/solver/error.c:28' of size 256
```

Without a sanitizer nothing stops the write: it lands in whatever globals the linker placed after `ErrString`, and the error message is then printed from the overflowed buffer (the fuzzing runs also show the matching read overflow in `report_writeInputErrorMsg()`, report.c:1433).

## Why it was fixed in 5.3.0

The fork commit 91de2fdd ("WIP addressing #162, #150", February 2024) changed the declaration to `char ErrString[MAXMSG];` with `MAXMSG` 1024, the same as `MAXLINE`, so any token from a line fits. 5.3.0 reports the same deck correctly:

```
ERROR 209: undefined object N0123456789...  (the whole 300-character name)
```

A stale declaration remains in both versions: report.c declares `extern char ErrString[81];` (report.c:72), which no longer matches the definition (256 in 5.2.4, 1024 in 5.3.0). report.c only passes the array on as a pointer, so this has no effect on the generated code, but the mismatch is undefined behaviour in C (C17 6.2.7) and should be `extern char ErrString[];` or come from a header.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-20_long-name.inp`](CRASH-20_long-name.inp) | A one-conduit model whose conduit starts at an undefined node with a 300-character name |
| [`CRASH-20_test.c`](CRASH-20_test.c) | Opens the deck through the legacy toolkit; correct is an input error with ERROR 209 and the whole name in the report |
| [`CRASH-20_test6.c`](CRASH-20_test6.c) | The same check through the 6.0.0 API |

```sh
tools/run-test.sh CRASH-20   # 5.2.4: CRASH; 5.3.0, 6.0.0: PASS
```

5.2.4 stops with the AddressSanitizer report above (`CRASH-20 5.2.4 base: CRASH`). 5.3.0 and 6.0.0:

```
swmm_open returned 200; ERROR 209 in the report: yes; with the whole 300-character name: yes
PASS: the input error is reported with the whole name and no memory error
CRASH-20 5.3.0 base: PASS
swmm_engine_open returned 5; ERROR 209 in the report: yes; with the whole 300-character name: yes
PASS: the input error is reported with the whole name and no memory error
CRASH-20 6.0.0 base: PASS
```

## The fix

There is no patch: 5.2.4 is reviewed unpatched, and 5.3.0 and 6.0.0 are not affected. For EPA's code base the fix is the one 5.3.0 carries:

```diff
-char  ErrString[256];
+char  ErrString[MAXMSG];
```

together with correcting the `extern` declaration in report.c. A bounded copy (`sstrncpy(ErrString, s, MAXMSG)`) would also protect callers that pass strings not taken from an input line.
