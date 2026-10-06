# CRASH-10: Node indexes read from a binary RDII interface file are not range-checked

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Reusing a binary RDII file written by another model, or by the same model before nodes were added or deleted, reads outside the `Node` array: a heap overflow, or a segmentation fault for an index far out of range, instead of ERROR 345 |
| **Reached from** | `[FILES] USE RDII` with a binary RDII interface file (the format SWMM writes with `SAVE RDII`) |
| **5.3.0** | `readRdiiFileHeader()` in [`src/legacy/engine/rdii.c:624`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rdii.c#L624) |
| **5.2.4** | Same code, [`src/solver/rdii.c:587`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/rdii.c#L587) |
| **6.0.0** | Fixed already: `RdiiInterfaceFile` rejects an index outside the node range ([`src/engine/hydrology/RdiiInterface.cpp:113`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/RdiiInterface.cpp#L113)) |
| **Since** | Every release in the repository history (initial commit, 2014) |
| **Fix** | Check the index before using it: [`CRASH-10_swmm530.patch`](CRASH-10_swmm530.patch) |

## The problem

An RDII interface file lets a model reuse RDII flows computed in an earlier run. The binary format stores, after the `SWMM5-RDII` stamp, the RDII time step, the number of RDII nodes and the node *indexes* (positions in the model's node list), then records of a date and one flow per node. An index means something only in the model that wrote the file. When the file comes from a different model, or the node list has changed since it was written, the stored index can be past the end of the current node list.

5.2.4 and 5.3.0 use the index from the file directly. In the test model, which has two nodes, an index of 2 reads one element past the end of `Node[]`, and indexes of 100000 or -1 read memory far outside it. The text format of the same file stores node names and is looked up safely; only the binary format is affected.

## Why it happens

```c
// src/legacy/engine/rdii.c, readRdiiFileHeader()
    // --- read indexes of RDII nodes
    if ( feof(Frdii.file) ) return ERR_RDII_FILE_FORMAT;
    fread(RdiiNodeIndex, sizeof(INT4), NumRdiiNodes, Frdii.file);
    for ( i=0; i<NumRdiiNodes; i++ )
    {
        j = RdiiNodeIndex[i];
        if ( Node[j].rdiiInflow == NULL ) return ERR_RDII_FILE_FORMAT;
    }
```

The check that the node has an RDII inflow is the right idea, but it reads `Node[j]` before anything confirms that `j` is a node.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-10_use-rdii.inp`](CRASH-10_use-rdii.inp) | Two nodes, J1 (index 0, with an [RDII] entry) and outfall O1 (index 1); `USE RDII "CRASH-10.rdii"` |
| [`CRASH-10_test.c`](CRASH-10_test.c) | Writes `CRASH-10.rdii` with one RDII node and a 1.0 cfs record, for node index 0 (control), 2, 100000 and -1, and runs the deck through the legacy toolkit after each. The control must run and deliver 1.0 cfs to J1; the others must stop with ERROR 345 |
| [`CRASH-10_test6.c`](CRASH-10_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh CRASH-10            # 5.2.4, 5.3.0: CRASH; 6.0.0: PASS
tools/run-test.sh CRASH-10 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix**, 5.3.0 (5.2.4 is the same, at `rdii.c:587`) handles the control and stops at index 2:

```
node index in file  error  max RDII inflow at J1 (cfs)
                 0      0  1.000
                 2  =================================================================
==27548==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x5160000008e0 at pc 0x7f7a80177275 bp 0x7ffe4b610a10 sp 0x7ffe4b610a08
READ of size 8 at 0x5160000008e0 thread T0
    #0 0x7f7a80177274 in readRdiiFileHeader .../src/legacy/engine/rdii.c:624:22
CRASH-10 5.3.0 base: CRASH
```

The review pass ran index 100000 on its own: both legacy versions stop with a segmentation fault at the same line.

6.0.0 already rejects the three bad files with `ERROR 345: invalid format for RDII interface file.` (its API returns code 11):

```
node index in file  error  max RDII inflow at J1 (cfs)
                 0      0  1.000
                 2     11  0.000
            100000     11  0.000
                -1     11  0.000
PASS: out-of-range node indexes in an RDII file are rejected
CRASH-10 6.0.0 base: PASS
```

**With the fix**, 5.3.0:

```
node index in file  error  max RDII inflow at J1 (cfs)
                 0      0  1.000
                 2    345  0.000
            100000    345  0.000
                -1    345  0.000
PASS: out-of-range node indexes in an RDII file are rejected with error 345
CRASH-10 5.3.0 patched: PASS
```

## The fix

```diff
         j = RdiiNodeIndex[i];
+        if ( j < 0 || j >= Nobjects[NODE] ) return ERR_RDII_FILE_FORMAT;
         if ( Node[j].rdiiInflow == NULL ) return ERR_RDII_FILE_FORMAT;
```

A file whose indexes all belong to nodes with RDII inflow is read as before. The format still cannot detect an index that is in range but belongs to a different node than when the file was written; as long as that node also has an [RDII] entry the flows go to it without a message. Storing node names (as the text format does) would close that gap but changes the file format, so it is not part of this fix.

6.0.0 needs no change for the crash. Its reader checks only the range, though, not that the node has an [RDII] entry as legacy does: a file that names outfall O1 (index 1) is rejected by 5.3.0 with ERROR 345 but accepted by 6.0.0, which then adds the file's flow to O1.
