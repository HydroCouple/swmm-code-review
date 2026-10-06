# CRASH-06: A SWEEP_START or SWEEP_END value of 24 or more characters overflows a stack buffer

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Stack buffer overflow while the input file is read: up to 5 bytes (`1947` and the terminator) are written past `char strDate[25]` in `project_readOption()`. Under AddressSanitizer the run aborts with an empty report; a release build corrupts the stack instead of reporting ERROR 213 (invalid date/time). |
| **Reached from** | `[OPTIONS]` SWEEP_START or SWEEP_END with a value of 24 or more characters (malformed or generated input); in principle any `sstrcat()` call whose destination is already full |
| **5.3.0** | `sstrcat()` in [`src/legacy/engine/swmm5.c:3478`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L3478), called from `project_readOption()` [`src/legacy/engine/project.c:563`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L563) |
| **5.2.4** | Same code, [`src/solver/swmm5.c:1418`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L1418) and [`src/solver/project.c:546`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/project.c#L546) |
| **6.0.0** | Not affected: `OptionsHandler.cpp` parses the value from a `std::string` with `std::from_chars` ([`src/engine/input/handlers/OptionsHandler.cpp:397`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/OptionsHandler.cpp#L397)) |
| **Since** | Every release. Up to 5.1 the value was copied with `strcpy`/`strcat` into the same 25-byte buffer; 5.2.0 switched to `sstrncpy`/`sstrcat`, but `sstrcat()` misses the case of a full destination |
| **Fix** | Check the room left before each write in `sstrcat()`: [`CRASH-06_swmm530.patch`](CRASH-06_swmm530.patch) |

## The problem

SWEEP_START and SWEEP_END take a month/day. `project_readOption()` copies the value into a 25-byte buffer and appends a year so that it can be parsed as a date (see [NUM-44](../../1-numerical/NUM-44-day-of-year-leap-years/)):

```c
// src/legacy/engine/project.c, project_readOption()
    char     strDate[25];
    ...
      case SWEEP_START:
      case SWEEP_END:
        sstrncpy(strDate, s2, 24);           // up to 24 characters + '\0'
        sstrcat(strDate, "/1947", 25);
        if ( !datetime_strToDate(strDate, &aDate) )
        {
            return error_setInpError(ERR_DATETIME, s2);
        }
```

With a value of 24 or more characters `sstrncpy()` fills the buffer, and `sstrcat()` then writes `/1947` and its terminator from index 24 on: 5 bytes past the end of `strDate`. The test deck has `SWEEP_START 01/01/0000000000000000000` (25 characters). The review's fuzzer reached the same write with three different mutated decks.

## Why it happens

```c
// src/legacy/engine/swmm5.c, sstrcat()
    dest_len = strlen(dest);                 // 24
    offset = dest_len;
    ...
    while (*(src + src_index) != '\0')
    {
        *(dest + offset) = *(src + src_index);   // first write at dest[24]
        offset++;                                // 25
        src_index++;
        // don't copy more than size - dest_len - 1 characters
        if (offset == destsize - 1)              // 25 == 24 never holds again
            break;
    }
    *(dest + offset) = '\0';
```

The bound is tested after each write and with `==`. When the destination already holds `destsize - 1` characters, the first write replaces its terminator, `offset` passes `destsize - 1`, and the loop copies the whole of `src` and a terminator past the buffer. For every other length the function works. The other callers append a token of the same input line to a `MAXLINE` buffer and do not reach a full destination, so this option is the only path the review found.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-06_long-sweep-date.inp`](CRASH-06_long-sweep-date.inp) | One conduit; `SWEEP_START 01/01/0000000000000000000` |
| [`CRASH-06_test.c`](CRASH-06_test.c) | Calls `swmm_open()` on the deck through the legacy toolkit and prints what it returns and the ERROR line of the report |
| [`CRASH-06_test6.c`](CRASH-06_test6.c) | Opens and runs the deck through the 6.0.0 C API |

```sh
tools/run-test.sh CRASH-06            # 5.2.4 and 5.3.0: CRASH; 6.0.0: PASS
tools/run-test.sh CRASH-06 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0 (5.2.4 stops at `swmm5.c:1440:26`, called from `project.c:546`):

```
==14840==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7fec16602089 at pc 0x7fec187d179d bp 0x7ffc9d997e20 sp 0x7ffc9d997e18
WRITE of size 1 at 0x7fec16602089 thread T0
    #0 in sstrcat .../src/legacy/engine/swmm5.c:3492:26
    #1 in project_readOption .../src/legacy/engine/project.c:563:9
    #2 in readOption .../src/legacy/engine/input.c:699:12
    #3 in input_countObjects .../src/legacy/engine/input.c:128:43
    [112, 137) 'strDate' (line 465) <== Memory access at offset 137 overflows this variable
CRASH-06 5.2.4 base: CRASH
CRASH-06 5.3.0 base: CRASH
```

6.0.0 accepts the value. It reads the month and day and ignores the rest, so SWEEP_START is 01/01:

```
swmm_engine_open() returned 0, the run returned 0
PASS: the 25-character SWEEP_START value is handled without a memory error (code 0)
CRASH-06 6.0.0 base: PASS
```

**With the fix**, 5.3.0 rejects the value as legacy SWMM intends:

```
swmm_open() returned 200
Report:   ERROR 213: invalid date/time 01/01/0000000000000000000 at line 15 of [OPTION] section:
PASS: the 25-character SWEEP_START value is rejected as an invalid date (ERROR 213) without overrunning strDate
CRASH-06 5.3.0 patched: PASS
CRASH-06 6.0.0 patched: PASS
```

The truncated 24-character string `01/01/000000000000000000` gets no suffix, its year 0 is out of range, and `datetime_strToDate()` fails.

## The fix

```diff
     // append src
     src_index = 0;
-    while (*(src + src_index) != '\0')
+    // don't copy more than size - dest_len - 1 characters
+    // (checked before each write so that a full dest gets nothing)
+    while (*(src + src_index) != '\0' && offset < destsize - 1)
     {
         *(dest + offset) = *(src + src_index);
         offset++;
         src_index++;
-        // don't copy more than size - dest_len - 1 characters
-        if (offset == destsize - 1)
-            break;
     }
```

Whenever the destination has room, the function copies exactly the same characters as before, so no valid input changes. The fix is in `sstrcat()` rather than in `project_readOption()` so that every caller is covered. 6.0.0 has no fixed-size buffer here and needs no patch. It accepts the malformed value instead of rejecting it, which is a difference in input checking, not a memory error.
