# CRASH-17: swmm_getError and swmm_getName write one byte past the caller's buffer

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | One-byte heap or stack overflow. A caller that passes `sizeof(buf)`, as the documentation asks, gets the terminating null written one byte past `buf` whenever the error text or the object ID is at least as long as the buffer. Error texts run to 60-80 characters and IDs to hundreds, so fixed-size buffers in bindings (32-byte ID buffers, say) are overrun in normal use. Nothing is reported; the byte silently corrupts whatever follows. |
| **Reached from** | `swmm_getError(errMsg, msgLen)` and `swmm_getName(objType, index, name, size)` |
| **5.3.0** | `swmm_getError()` and `swmm_getName()` in [`src/legacy/engine/swmm5.c:1221`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L1221) and [`:1314`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L1314), through `sstrncpy()` at [`:3458`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L3458) |
| **5.2.4** | Same: [`src/solver/swmm5.c:771`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L771), [`:821`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L821), `sstrncpy()` at [`:1392`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L1392) |
| **6.0.0** | Not affected: IDs and messages are returned as `const char*`, and the getters that fill a caller buffer (`swmm_node_get_ids_bulk`, `swmm_gage_get_timeseries`, ...) truncate to size - 1 as documented (checked by the 6.0.0 test) |
| **Since** | `swmm_getError` has written `msgLen + 1` bytes since it was added in 5.1.011 (then via `dest[maxlen] = '\0'`); `swmm_getName` since 5.2.0 |
| **Fix** | Pass size - 1 to `sstrncpy()`: [`CRASH-17_swmm530.patch`](CRASH-17_swmm530.patch) (requires [API-03](../../6-api/API-03-api-error-messages-empty/), whose change is on a neighbouring line) |

## The problem

Both functions document the length argument as the size of the buffer:

```c
// include/openswmm/legacy/engine/openswmm_solver.h
 * \param[out] errMsg Error message text
 * \param[in] msgLen Maximum size of errMsg
int swmm_getError(char *errMsg, int msgLen);
...
 * \param[out] name Object name
 * \param[in] size Size of the name array
int swmm_getName(int objType, int index, char *name, int size);
```

(5.2.4's comments say the same: "msgLen = maximum size of errMsg", "size = size of the name array".) A caller doing the obvious

```c
char *buf = malloc(16);
swmm_getError(buf, 16);
```

after `swmm_open` of a missing file (error 303, "ERROR 303: cannot open input file.") gets 15 characters plus a null in `buf[16]`, one byte past the block. `swmm_getName(swmm_NODE, j, buf, 2)` for node "J1" writes "J1" and the null into a 2-byte buffer.

## Why it happens

`sstrncpy(dest, src, n)` copies up to `n` characters and then always writes a null after them, so it needs `n + 1` bytes:

```c
// src/legacy/engine/swmm5.c, sstrncpy()
    if (n > 0)
    {
        while (*(src + offset) != '\0')
        {
            if ((size_t)offset == n)
                break;
            *(dest + offset) = *(src + offset);
            offset++;
        }
    }
    *(dest + offset) = '\0';
```

The engine's own callers use it that way (`sstrncpy(fname, hotStartFile, MAXFNAME)` into `char fname[MAXFNAME + 1]`). The two API functions pass the caller's buffer size instead:

```c
// src/legacy/engine/swmm5.c, swmm_getError()
    sstrncpy(errMsg, ErrorMsg, msgLen);
// src/legacy/engine/swmm5.c, swmm_getName()
    if (idName)
        sstrncpy(name, idName, size);
```

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-17_model.inp`](CRASH-17_model.inp) | A small valid model; its node IDs are "J1" and "O1" |
| [`CRASH-17_test.c`](CRASH-17_test.c) | In two child processes: `swmm_getError(buf, 16)` after opening a missing file, and `swmm_getName(swmm_NODE, J1, buf, 2)`, each into a heap buffer of exactly that size; the text must be cut to size - 1 characters |
| [`CRASH-17_test6.c`](CRASH-17_test6.c) | 6.0.0's `swmm_node_get_ids_bulk` (stride 2) and `swmm_gage_get_timeseries` (2-byte buffer) on the same deck |

```sh
tools/run-test.sh CRASH-17            # 5.2.4 and 5.3.0: CRASH; 6.0.0: PASS
tools/run-test.sh CRASH-17 --patched  # 5.3.0 (with API-03) and 6.0.0: PASS
```

**Without the fix**, 5.3.0 overruns both buffers:

```
swmm_getError, 16-byte buffer:
==20680==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x502000000020 ...
WRITE of size 1 at 0x502000000020 thread T0
    #0 0x7efff8bc17e3 in sstrncpy .../src/legacy/engine/swmm5.c:3471:22
    #1 0x7efff8bc243e in swmm_getError .../src/legacy/engine/swmm5.c:1221:5
    #2 0x559743bbbad2 in checkGetError .../CRASH-17_test.c:34:12
0x502000000020 is located 0 bytes after 16-byte region [0x502000000010,0x502000000020)
...
swmm_getName, 2-byte buffer:
==20682==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x502000000012 ...
WRITE of size 1 at 0x502000000012 thread T0
    #0 0x7efff8bc17e3 in sstrncpy .../src/legacy/engine/swmm5.c:3471:22
    #1 0x7efff8bc2b13 in swmm_getName .../src/legacy/engine/swmm5.c:1314:9
0x502000000012 is located 0 bytes after 2-byte region [0x502000000010,0x502000000012)
...
FAIL: 2 of 2 calls wrote past the buffer size they were given or did not truncate to size - 1 characters
CRASH-17 5.3.0 base: CRASH
```

5.2.4 reports the same two writes in `sstrncpy swmm5.c:1412`, the first called from `swmm_getError swmm5.c:771`, the second from `swmm_getName` (a tail call at `-O1`, so the stack shows the test's `checkGetName` directly). 6.0.0 truncates:

```
swmm_node_get_ids_bulk(stride 2) rc 0: "J", "O"
swmm_gage_get_timeseries(buflen 2) rc 0: "R"
PASS: both getters stay within the buffer size they are given
CRASH-17 6.0.0 base: PASS
```

**With the fix** (the "Cannot open input file" line is the engine's own console message):

```
swmm_getError, 16-byte buffer:

    Cannot open input file CRASH-17_missing.inp  swmm_getError(buf, 16) returned 303, text "   ERROR 303: c" (15 chars)
  ok
swmm_getName, 2-byte buffer:
  swmm_getName(swmm_NODE, J1, buf, 2) gave "J"
  ok
PASS: both calls stay within the buffer size they are given
CRASH-17 5.3.0 patched: PASS
```

## The fix

The two call sites pass the number of characters that fit, leaving room for the null, and copy nothing for a non-positive size:

```diff
-    sstrncpy(errMsg, ErrorMsg, msgLen);
+    // --- msgLen is the size of errMsg: copy at most msgLen-1 characters
+    if (msgLen > 0)
+        sstrncpy(errMsg, ErrorMsg, msgLen - 1);
 ...
-    if (idName)
-        sstrncpy(name, idName, size);
+    if (idName && size > 0)
+        sstrncpy(name, idName, size - 1);
```

`sstrncpy()` keeps its meaning (its other callers are correct). A caller whose buffer is larger than the text sees no change; one whose buffer is exactly as long as the text now gets the text cut by one character instead of an overrun. The patch is written on top of API-03 because API-03 changes the line two above the `swmm_getError` hunk; it declares `Requires: API-03`.

`swmm_getErrorFromCode(int, char *outErrMsg[1024])` has an odd parameter type and no size argument, but writes at most 1024 bytes (the message buffer is `MAXMSG` = 1024 bytes including its null), so it does not overrun a buffer of the size its prototype suggests.
