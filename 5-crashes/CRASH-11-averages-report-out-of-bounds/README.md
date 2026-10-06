# CRASH-11: With AVERAGES YES, 5.3.0 reads past the averaged node results and converts SI depths twice

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Two 5.3.0 regressions in "Reported Max Depth" (Node Depth Summary, and `swmm_getNodeStats()` `maxRptDepth`). With `[REPORT] NODES` listing a subset, every reporting step reads past the end of a heap block and dereferences the pointer found there: AddressSanitizer stops the run; a release build reads whatever is there, can crash, and credits reported nodes' depths to other nodes. In any SI project, every node's Reported Max Depth is 0.3048 x the true value (0.09 m for 0.31 m). No warning. |
| **Reached from** | `[REPORT] AVERAGES YES`; the overflow also needs `NODES` to list fewer than all nodes, the unit error needs SI flow units (CMS, LPS, MLD) |
| **5.3.0** | `output_saveAvgResults()` in [`src/legacy/engine/output.c:927`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L927) |
| **5.2.4** | Not affected: it updates the maximum from the node's current depth ([`src/solver/output.c:931`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/output.c#L931)) |
| **6.0.0** | Not affected. `SWMMEngine::postOutputSnapshot()` keeps the largest depth interpolated to each reporting time for every node, in display units, and does not reproduce the averaged maximum ([`src/engine/core/SWMMEngine.cpp:6093`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L6093)) |
| **Since** | 5.3.0, fork commit 7266536f ("Fix max depth reporting when average is turned on", #188) |
| **Fix** | Walk the reported nodes with their own index and drop the second conversion: [`CRASH-11_swmm530.patch`](CRASH-11_swmm530.patch) |

## The problem

With `AVERAGES YES`, 5.3.0 takes a node's "Reported Max Depth" from the depths averaged over each reporting period, instead of the depth at the reporting time (5.2.4). The change has two errors.

**SI units.** The test model (CMS, three junctions in series, inflow rising to a 1-hour plateau of 0.5 m3/s) has a maximum saved depth of 0.310 m at every junction. 5.3.0 reports:

```
  J1                   JUNCTION     ...        0.09
```

0.09 m is 0.31 x 0.3048. The Maximum Depth column of the same row says 0.31. US-unit projects are not affected by this part, because the factor is 1.

**A node subset.** With `NODES J3` in `[REPORT]`, the run aborts at the first reporting time under AddressSanitizer:

```
ERROR: AddressSanitizer: heap-buffer-overflow ... READ of size 8
    #0 in output_saveAvgResults output.c:927:55
    #1 in output_saveResults output.c:493:30
    #2 in saveResults swmm5.c:1031:13
    #3 in swmm_step swmm5.c:823:13
0x502000000c18 is located 0 bytes after 8-byte region
```

The 8-byte region is the one-element array of averaged results (one `TAvgResults` per reported node). Without a sanitizer, the loop reads the `xAvg` pointer of the heap block that follows and loads a float through it: garbage or a segmentation fault, depending on the heap. Even the first, in-bounds element is wrong: J1 is credited with the depth of J3, the only reported node.

## Why it happens

```c
// src/legacy/engine/output.c, output_saveAvgResults()
// --- update each node's max depth and contribution to system storage
for (i = 0; i < Nobjects[NODE]; i++)
{
    stats_updateMaxNodeDepth(i, AvgNodeResults[i].xAvg[NODE_DEPTH] * UCF(LENGTH) / Nsteps);
    SysResults[SYS_STORAGE] += (REAL4)(Node[i].newVolume * UCF(VOLUME));
}
```

- `AvgNodeResults` is allocated with `NumNodes` entries, the number of reported nodes (`output_openAvgResults()`), and `output_updateAvgResults()` fills it with a separate counter `k` that advances only for nodes with `rptFlag` set. The loop above indexes it with the project node index `i`, which goes up to `Nobjects[NODE] - 1`.
- `output_updateAvgResults()` accumulates the values returned by `node_getResults()`, which are already in user units (`node.c:487`: `z = (f1 * Node[nodeIndex].oldDepth + wt * Node[nodeIndex].newDepth) * UCF(LENGTH);`). Multiplying by `UCF(LENGTH)` again scales SI depths by 0.3048.

Before 7266536f the line was `stats_updateMaxNodeDepth(i, Node[i].newDepth * UCF(LENGTH))`, which is correct for every node but uses the depth at the end of the routing step rather than the period average.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-11_si-all.inp`](CRASH-11_si-all.inp) | J1 -> J2 -> J3 -> O1 (CMS, DYNWAVE), inflow at J1 with a 1-hour plateau of 0.5 m3/s, `AVERAGES YES`, all nodes reported |
| [`CRASH-11_si-subset.inp`](CRASH-11_si-subset.inp) | The same with `NODES J3` |
| [`CRASH-11_test.c`](CRASH-11_test.c) | Legacy toolkit: runs both decks and compares each junction's Reported Max Depth (.rpt) with the largest depth saved in the first run's .out (tolerance 0.01 m + 3 %). On the steady plateau the point and averaged depths agree, so this reference holds for every engine |
| [`CRASH-11_test6.c`](CRASH-11_test6.c) | The same through `swmm_engine_run()` |
| [`CRASH-11_common.h`](CRASH-11_common.h) | The checks both tests share |
| [`CRASH-11_swmm530.patch`](CRASH-11_swmm530.patch) | The fix |

```sh
tools/run-test.sh CRASH-11            # 5.2.4: PASS, 5.3.0: CRASH, 6.0.0: PASS
tools/run-test.sh CRASH-11 --patched  # 5.3.0 with the fix: PASS (6.0.0 has no patch)
```

**Without the fix**, 5.3.0:

```
1. all nodes reported
   Node  Reported Max Depth  Max depth in .out (m)
   J1                  0.09                  0.310  <-- wrong
   J2                  0.09                  0.310  <-- wrong
   J3                  0.09                  0.310  <-- wrong
2. only J3 reported (NODES J3)
=================================================================
==19846==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x502000000c18 at pc 0x7fb417d5d639 bp 0x7ffe01d391f0 sp 0x7ffe01d391e8
READ of size 8 at 0x502000000c18 thread T0
    #0 0x7fb417d5d638 in output_saveAvgResults .../src/legacy/engine/output.c:927:55
    #1 0x7fb417d5d638 in output_saveResults .../src/legacy/engine/output.c:493:30
CRASH-11 5.3.0 base: CRASH
```

5.2.4 and 6.0.0 print 0.31 for all six values and pass.

**With the fix**, 5.3.0:

```
1. all nodes reported
   Node  Reported Max Depth  Max depth in .out (m)
   J1                  0.31                  0.310
   J2                  0.31                  0.310
   J3                  0.31                  0.310
2. only J3 reported (NODES J3)
   Node  Reported Max Depth  Max depth in run 1 .out (m)
   J1                  0.31                        0.310
   J2                  0.31                        0.310
   J3                  0.31                        0.310
PASS: Reported Max Depth is the maximum saved depth, with all nodes and with a node subset
CRASH-11 5.3.0 patched: PASS
```

## The fix

Index the averages with their own counter, as `output_updateAvgResults()` does, and drop the second unit conversion. Unreported nodes have no averages; they keep the 5.2.4 rule (the node's depth at the reporting time), so their Reported Max Depth is not left at 0:

```diff
     // --- update each node's max depth and contribution to system storage
+    //     (AvgNodeResults holds reported nodes only, already in user units)
+    k = 0;
     for (i = 0; i < Nobjects[NODE]; i++)
     {
-        stats_updateMaxNodeDepth(i, AvgNodeResults[i].xAvg[NODE_DEPTH] * UCF(LENGTH) / Nsteps);
+        if ( Node[i].rptFlag ) stats_updateMaxNodeDepth(i,
+            AvgNodeResults[k++].xAvg[NODE_DEPTH] / (double)Nsteps);
+        else stats_updateMaxNodeDepth(i, Node[i].newDepth * UCF(LENGTH));
         SysResults[SYS_STORAGE] += (REAL4)(Node[i].newVolume * UCF(VOLUME));
     }
```

Effect on other models: only runs with `AVERAGES YES` change, and none of the 73 regression decks uses it. For US-unit projects that report every node, the new expression is the same double division as before (`UCF(LENGTH)` is 1), so their results are bit-identical.

6.0.0 needs no patch. Its Reported Max Depth with `AVERAGES YES` is the maximum of the interpolated depths at the reporting times, not of the period averages, so it can differ from patched 5.3.0 on a rapidly varying hydrograph; that is a documented difference in 6.0.0 (`SWMMEngine.cpp:6096`), not a defect.

A late `REPORT_START` with `AVERAGES YES` also inflates the first averaged period, and with it this maximum; that is [BND-18](../../3-boundary/BND-18-averages-not-reset-at-report-start/README.md).
