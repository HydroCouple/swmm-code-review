# CRASH-18: SMO_getSystemResult() reads into an unallocated buffer when the period is out of range

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Asking the output reader for the system results of period -1 crashes the host application: 5.3.0 freads 15 floats into a NULL pointer (SEGV), 5.2.4 into an uninitialised pointer (memory corruption, crash at a later call). For a period past the end the call returns error 422 but still sets the length to 15, and 5.2.4 also hands back a junk non-NULL pointer. |
| **Reached from** | `SMO_getSystemResult()` with a period index outside `0 .. Nperiods-1`, or when its 15-float allocation fails (error 411) |
| **5.3.0** | `SMO_getSystemResult()` in [`src/legacy/output/swmm_output.c:1689-1693`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1689-L1693) |
| **5.2.4** | Same code, [`src/outfile/swmm_output.c:937-941`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/outfile/swmm_output.c#L937-L941), and `temp` is not even initialised |
| **6.0.0** | Not affected: [`OutputReader::get_system_result()`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/output/OutputReader.cpp#L239-L242) checks the period before it seeks and writes into a caller-owned value |
| **Since** | 5.1.14, the first release that ships the output library (`src/outfile`) |
| **Fix** | Add the missing `else`: [`CRASH-18_swmm530.patch`](CRASH-18_swmm530.patch) |

## The problem

The output-file reader returns all system variables (rainfall, runoff, flooding, outflow and so on, 15 values) for one reporting period through `SMO_getSystemResult(handle, period, 0, &values, &length)`. A period outside `0 .. Nperiods-1` is supposed to give error 422, "reporting period index out of range", and no data.

It does set 422, but then reads the file anyway. For period -1 the read lands inside the file and 15 floats are written through the result pointer, which was never allocated: NULL in 5.3.0, so the host program dies with a SEGV; whatever was on the stack in 5.2.4, so the 15 floats overwrite some unrelated memory and the program crashes later. For a period one past the end the seek goes beyond the end of the file, fread reads nothing, and the call "works": error 422, but with `length` set to 15 and, in 5.2.4, a junk non-NULL pointer that a caller might free.

The other result getters (`SMO_getSubcatchResult`, `SMO_getNodeResult`, `SMO_getLinkResult`) have the same checks and are not affected.

## Why it happens

The read block is a bare compound statement after the last `else if`, not its `else` branch:

```c
// src/legacy/output/swmm_output.c, SMO_getSystemResult()
    if (p_data == NULL)
        errorcode = -1;
    else if (periodIndex < 0 || periodIndex >= p_data->Nperiods)
        errorcode = ERR422;
    else if MEMCHECK (temp = newFloatArray(p_data->SysVars))
        errorcode = ERR411;
    {                                   // runs on every path, also after ERR422
        offset = p_data->ResultsPos + (periodIndex)*p_data->BytesPerPeriod +
                 2 * RECORDSIZE;
        ...
        _fseek(p_data->file, offset, SEEK_SET);
        fread(temp, RECORDSIZE, p_data->SysVars, p_data->file);

        *outValueArray = temp;
        *arrayLength = p_data->SysVars;
    }
```

When the period is rejected, `newFloatArray()` is never called. 5.3.0 initialises `temp` to NULL; 5.2.4 declares `float *temp;` without an initialiser. The same braces also run when `p_data` is NULL (see [CRASH-19](../CRASH-19-output-reader-null-handle/)).

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-18_reader.inp`](CRASH-18_reader.inp) | One subcatchment, three junctions, an outfall and three conduits; 6 hours reported every 15 minutes (24 periods) |
| [`CRASH-18_test.c`](CRASH-18_test.c) | Runs the deck, then calls `SMO_getSystemResult()` for periods 0, 23, 24 and -1 (5.2.4 and 5.3.0) |
| [`CRASH-18_test6.c`](CRASH-18_test6.c) | The same periods through 6.0.0's `swmm_output_get_system_result()` |

```sh
tools/run-test.sh CRASH-18            # 5.2.4 and 5.3.0: CRASH; 6.0.0: PASS
tools/run-test.sh CRASH-18 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix**, 5.3.0 returns 422 with a length of 15 for period 24, and period -1 writes through NULL:

```
  period  error  length  buffer
       0      0        15  non-NULL
      23      0        15  non-NULL
      24    422        15  NULL
AddressSanitizer:DEADLYSIGNAL
==30503==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000 (pc 0x7f960d9a16e3 bp 0x7fff43b1ba20 sp 0x7fff43b1b9d8 T0)
==30503==The signal is caused by a WRITE memory access.
    #3 0x7f960d88657a in _IO_fread libio/iofread.c:38:16
    #5 0x7f960dfa6bb0 in SMO_getSystemResult .../src/legacy/output/swmm_output.c:1705:9
CRASH-18 5.3.0 base: CRASH
```

5.2.4 returns a non-NULL pointer with both errors, and the period -1 call corrupts memory; here the next `fflush(stdout)` in the test crashes (an earlier build of the same test crashed in `SMO_close()`'s `free()` instead):

```
  period  error  length  buffer
       0      0        15  non-NULL
      23      0        15  non-NULL
      24    422        15  non-NULL
      -1    422        15  non-NULL
==30485==ERROR: AddressSanitizer: SEGV on unknown address 0x000400180f03 (pc 0x55efd9195d34 bp 0x6000000000000000 sp 0x7ffc7aeac680 T0)
    #1 0x55efd9169977 in fflush (.../524-base/test.bin+0x8c977)
    #2 0x55efd91e2bde in probe .../524-base/CRASH-18_test.c:27:5
CRASH-18 5.2.4 base: CRASH
```

6.0.0 rejects both periods and leaves the caller's value alone:

```
  period  rc     rainfall
       0      0  0
      23      0  0
      24     -1  -999
      -1     -1  -999
PASS: swmm_output_get_system_result() rejects periods 24 and -1 and leaves the caller's value untouched
CRASH-18 6.0.0 base: PASS
```

**With the fix**, both invalid periods return 422, no buffer and an untouched length (the test sets it to -1 before each call):

```
  period  error  length  buffer
       0      0        15  non-NULL
      23      0        15  non-NULL
      24    422        -1  NULL
      -1    422        -1  NULL
PASS: SMO_getSystemResult() returns error 422 for periods 24 and -1 without reading into an unallocated buffer
CRASH-18 5.3.0 patched: PASS
```

## The fix

```diff
     else if MEMCHECK (temp = newFloatArray(p_data->SysVars))
         errorcode = ERR411;
+    else
     {
         // calculate byte offset to start time for series
```

Valid periods return exactly the same values as before. 6.0.0 needs no change.
