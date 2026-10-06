# CRASH-16: swmm_getSavedValue reads outside the object arrays for an out-of-range index

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Heap read out of bounds. An index of -1 (what `swmm_getIndex` returns for an unknown ID) or one past the last object reads `rptFlag` from outside the `Subcatch`, `Node` or `Link` array; the garbage is then used as a record offset into the binary output file, so the caller gets an arbitrary number from the file or a crash, with no error. |
| **Reached from** | `swmm_getSavedValue(property, index, period)` after `swmm_end`, with `index < 0` or `index >=` the number of subcatchments, nodes or links |
| **5.3.0** | `swmm_getSavedValue()` in [`src/legacy/engine/swmm5.c:2458`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L2458); the reads in `getSavedSubcatchValue()`, `getSavedNodeValue()`, `getSavedLinkValue()` at [`:3071`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L3071), [`:3101`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L3101), [`:3137`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L3137) |
| **5.2.4** | Same code: [`src/solver/swmm5.c:922`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L922), node read at [`:1279`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L1279) |
| **6.0.0** | Not applicable: saved results are read through the output-file reader (`openswmm_output.h`), whose series and attribute functions return -1 for an index out of range (checked by the 6.0.0 test) |
| **Since** | 5.2.0, which added `swmm_getSavedValue` |
| **Fix** | Treat an out-of-range index as an object that was not saved (return 0): [`CRASH-16_swmm530.patch`](CRASH-16_swmm530.patch) |

## The problem

`swmm_getSavedValue` returns a result stored in the binary output file for one object at one reporting period. It validates everything except the object:

```c
// src/legacy/engine/swmm5.c, swmm_getSavedValue()
    if (!IsOpenFlag)
        return 0;
    if (IsStartedFlag)
        return 0;
    if (period < 1 || period > Nperiods)
        return 0;
    ...
    if (property < 400)
        return getSavedNodeValue(property, index, period);
```

Every other getter in the toolkit tests `index < 0 || index >= Nobjects[...]` first. A caller that loops one past the end, or passes the -1 that `swmm_getIndex` returns for a misspelt ID, reads memory beyond the array. On the test deck (2 nodes), `swmm_getSavedValue(swmm_NODE_DEPTH, 2, 1)` reads 16 bytes past the end of the `Node` array in both 5.2.4 and 5.3.0.

## Why it happens

The helpers turn the object index into the object's position in the output file through its `rptFlag`, without a bounds check:

```c
// src/legacy/engine/swmm5.c, getSavedNodeValue()
    // --- order in which node was saved to output results file
    int outIndex = Node[index].rptFlag - 1;
    if (outIndex < 0)
        return 0;

    output_readNodeResults(period, outIndex);
```

`getSavedSubcatchValue()` and `getSavedLinkValue()` are the same. If the garbage `rptFlag` is positive, `output_readNodeResults()` seeks to a record that belongs to another object or lies outside the period's block.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-16_model.inp`](CRASH-16_model.inp) | 1 subcatchment, 2 nodes, 1 conduit; 1 hour, all objects saved every 15 minutes |
| [`CRASH-16_test.c`](CRASH-16_test.c) | Runs the deck, reads J1's saved depth at period 1, then calls `swmm_getSavedValue` at period 1 with index = count and -1 for a node, a link and a subcatchment; each must return 0 |
| [`CRASH-16_test6.c`](CRASH-16_test6.c) | Runs the deck in 6.0.0 and reads the `.out` file with the output reader using the same out-of-range indices |

```sh
tools/run-test.sh CRASH-16            # 5.2.4 and 5.3.0: CRASH; 6.0.0: PASS
tools/run-test.sh CRASH-16 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0:

```
saved depth of J1 (index 0), period 1 (0:15) = 0.8742 ft
swmm_getSavedValue, node index = node count          ( 2) ...
=================================================================
==18463==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x5160000002a0 ...
READ of size 4 at 0x5160000002a0 thread T0
    #0 0x7f9ce8dd0f76 in getSavedNodeValue .../src/legacy/engine/swmm5.c:3101:32
    #1 0x7f9ce8dd09d8 in swmm_getSavedValue .../src/legacy/engine/swmm5.c:2471:16
    #2 0x56317bd63af7 in main .../CRASH-16_test.c:62:13

0x5160000002a0 is located 16 bytes after 528-byte region [0x516000000080,0x516000000290)
allocated by thread T0 here:
    #1 0x7f9ce8d61078 in createObjects .../src/legacy/engine/project.c:1046:30
CRASH-16 5.3.0 base: CRASH
```

5.2.4 stops at the same call (`getSavedNodeValue swmm5.c:1279`, "16 bytes after 512-byte region", allocated in `createObjects project.c:1015`). 6.0.0's reader refuses every out-of-range index:

```
output file: 1 subcatchments, 2 nodes, 1 links, 4 periods
depth of node 0 (J1) at the first period (0:15) = 0.8742 ft (rc 0)
node series,    index  2 -> rc -1
link series,    index  1 -> rc -1
subcatch series, index  1 -> rc -1
node attribute, index  2 -> rc -1
node series,    index -1 -> rc -1
...
PASS: out-of-range indices are refused and in-range ones return the saved result
CRASH-16 6.0.0 base: PASS
```

**With the fix**, 5.3.0:

```
saved depth of J1 (index 0), period 1 (0:15) = 0.8742 ft
swmm_getSavedValue, node index = node count          ( 2) ...
    returned 0
swmm_getSavedValue, node index = -1                  (-1) ...
    returned 0
...
swmm_getSavedValue, subcatch index = -1              (-1) ...
    returned 0
PASS: out-of-range indices return 0 and in-range ones the saved result
CRASH-16 5.3.0 patched: PASS
```

## The fix

Each helper only reads `rptFlag` for an index inside the array, and otherwise falls through to the existing "not saved" exit:

```diff
     // --- order in which node was saved to output results file
-    int outIndex = Node[index].rptFlag - 1;
+    int outIndex = -1;
+    if (index >= 0 && index < Nobjects[NODE])
+        outIndex = Node[index].rptFlag - 1;
     if (outIndex < 0)
         return 0;
```

Returning 0 matches what the function already does for an invalid period or an object that was not saved; `swmm_getSavedValue` returns a `double`, so it has no separate error channel. Valid calls are unchanged.

A side observation from the test, not part of this issue: with `REPORT_STEP 00:15:00` over a 1-hour run, 5.2.4's `swmm_getSavedValue` sees 3 periods (0:15 to 0:45; period 4 returns 0), while 5.3.0 and 6.0.0 see 4. The test uses period 1 so that the in-range check holds on all three engines.
