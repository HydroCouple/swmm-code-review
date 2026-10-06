# CRASH-07: Cross-section table lookups convert an unchecked ratio to an int index

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Segmentation fault at the start of routing, with an empty report. Reached by a conduit whose cross-section size or initial flow is `nan` (for example a deck generated from data with missing values), and by an absurdly small but valid size: a circular conduit 1e-30 ft across. 6.0.0 has the same unguarded conversion in its critical-depth code (undefined behaviour from a `nan` initial flow) |
| **Reached from** | `[XSECTIONS]` sizes and `[CONDUITS]` initial flows that make a depth, area or flow ratio NaN or very large |
| **5.3.0** | `lookup()` in [`src/legacy/engine/xsect.c:1492`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L1492); the same conversion in `tabular_getdSdA()` at [`xsect.c:1445`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L1445) and `getYcritEnum()` at [`xsect.c:1655`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L1655) |
| **5.2.4** | Same code, [`src/solver/xsect.c:1488`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/xsect.c#L1488), [`:1441`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/xsect.c#L1441), [`:1651`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/xsect.c#L1651) |
| **6.0.0** | `lookup()` is guarded ([`src/engine/hydraulics/XSectKernels.hpp:417`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/XSectKernels.hpp#L417), fork commit 70719ad5) and a `nan` size is rejected, but `getYcrit()` ([`XSectKernels.hpp:1438`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/XSectKernels.hpp#L1438)) and `tabular_getdSdA()` ([`:567`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/XSectKernels.hpp#L567)) still convert unchecked values; a `nan` initial flow reaches the first |
| **Since** | Every release in the repository (initial commit, 2014) |
| **Fix** | Check the ratio before converting it, NaN-safe, in the three places: [`CRASH-07_swmm530.patch`](CRASH-07_swmm530.patch), [`CRASH-07_swmm600.patch`](CRASH-07_swmm600.patch) |

## The problem

SWMM's standard shapes are described by tables of area, width, hydraulic radius and section factor at equally spaced values of a normalised depth or area between 0 and 1. `lookup()` finds the segment that holds a value by converting it to an index, and only checks the index against the top of the table. A value that is NaN, or so large that it does not fit in an `int`, makes the conversion undefined; on x86-64 it returns `INT_MIN`, which passes the check, and the table is read 16 GiB before its start.

Three one-conduit decks reach it at the start of the run (`swmm_start()`):

| Deck | Value | Where it crashes (5.3.0) |
|---|---|---|
| `C1 CIRCULAR 1e-30` | initial depth ratio y/yFull of about 5e27 | `lookup()` ← `xsect_getAofY()` ← `initLinks()` |
| `C1 CIRCULAR nan` | NaN full depth | `lookup()` ← `xsect_getAofY()` ← `initLinks()` |
| `C1 ... InitFlow nan` | NaN flow ratio | `lookup()` ← `circ_getYofA()` ← `conduit_initState()` |

5.2.4 and 5.3.0 report the conversion and then a segmentation fault in all three. The diameter of 1e-30 ft is absurd but it is a valid number that the input checks accept (`xsect_setParams()` only rejects sizes `<= 0`), so this is not only a question of NaN input. The NaN cases depend on the reader accepting the token `nan` as a number, which is [IO-01](../../4-io/IO-01-nan-inf-accepted-as-numbers/).

6.0.0 guarded its own `lookup()` after a segmentation fault on x64 (fork commit 70719ad5, "guard non-finite input in lookup/getAofY") and rejects a `nan` diameter with ERROR 211, but a `nan` initial flow reaches the same kind of conversion in `getYcrit()`, followed by a signed integer overflow.

## Why it happens

```c
// src/legacy/engine/xsect.c, lookup()
    // --- find which segment of table contains x
    delta = 1.0 / ((double)nItems-1);
    i = (int)(x / delta);
    if ( i >= nItems - 1 ) return table[nItems-1];
    ...
    y = table[i] + (x - x0) * (table[i+1] - table[i]) / delta;
```

Converting a floating-point value outside the range of `int` (NaN included) is undefined behaviour in C (C17 6.3.1.4). The same pattern is in `tabular_getdSdA()` (`i = (int)(alpha / delta); if ( i >= nItems - 1 ) i = nItems - 2;`), and in `getYcritEnum()`, which converts the critical-depth estimate `y0` of `xsect_getYcrit()`:

```c
// src/legacy/engine/xsect.c, xsect_getYcrit() and getYcritEnum()
        y = 1.01 * pow(q2g / xsect->yFull, 1./4.);
        if (y >= xsect->yFull) y = 0.97 * xsect->yFull;   // NaN passes
    ...
    i1 = (int)(y0 / dy);
```

With a NaN flow, `y` is NaN, the cap does not apply, and the conversion is undefined. 6.0.0's `getYcrit()` and `tabular_getdSdA()` are line-for-line ports of these two.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-07_tiny-diameter.inp`](CRASH-07_tiny-diameter.inp) | J1 drains through C1 (CIRCULAR, 1e-30 ft) to O1 with a 1-cfs inflow |
| [`CRASH-07_nan-diameter.inp`](CRASH-07_nan-diameter.inp) | The same with a diameter of `nan` |
| [`CRASH-07_nan-initflow.inp`](CRASH-07_nan-initflow.inp) | A 1.5-ft C1 with an initial flow of `nan` |
| [`CRASH-07_test.c`](CRASH-07_test.c) | Runs the three decks through the legacy toolkit; correct is that all three run to the end (with or without an error code) |
| [`CRASH-07_test6.c`](CRASH-07_test6.c) | The same through the 6.0.0 API |

```sh
tools/run-test.sh CRASH-07            # 5.2.4, 5.3.0, 6.0.0: CRASH
tools/run-test.sh CRASH-07 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.3.0 stops on the first deck (5.2.4 the same at xsect.c:1488 and 1496):

```
running tiny-diameter    ... ../src/src/legacy/engine/xsect.c:1492:9: runtime error: 5e+27 is outside the range of representable values of type 'int'
    #0 0x7f07d9beb13c in lookup .../src/legacy/engine/xsect.c:1492:9
    #1 0x7f07d9bf0706 in xsect_getAofY .../src/legacy/engine/xsect.c
    #2 0x7f07d9ad4f38 in initLinks .../src/legacy/engine/flowrout.c:502:29
==20129==ERROR: AddressSanitizer: SEGV on unknown address 0x7f03d9c77420 (pc 0x7f07d9beafc6 bp 0x7fff05ff6e20 sp 0x7fff05ff6df0 T0)
SUMMARY: AddressSanitizer: SEGV .../src/legacy/engine/xsect.c:1500 in lookup
CRASH-07 5.3.0 base: CRASH
```

6.0.0 runs the first deck, rejects the second (error 5, `ERROR 211: invalid number`), and stops on the third:

```
running tiny-diameter    ... finished, error code 0
running nan-diameter     ... finished, error code 5
running nan-initflow     ... ../src/src/engine/hydraulics/XSectKernels.hpp:1438:47: runtime error: nan is outside the range of representable values of type 'int'
    #0 0x7fb768c5eced in openswmm::xsect::XsectEval::getYcrit(openswmm::XSectParams const&, double) const .../XSectKernels.hpp:1438:47
    #1 0x7fb768c14dd1 in openswmm::outfall::setAllOutfallDepths(...) .../Outfall.cpp:280:21
../src/src/engine/hydraulics/XSectKernels.hpp:1452:41: runtime error: signed integer overflow: -2147483648 - 1 cannot be represented in type 'int'
CRASH-07 6.0.0 base: CRASH
```

**With the fix**, no sanitizer report:

```
---- CRASH-07 on 5.3.0 (patched) ----
running tiny-diameter    ... finished, error code 0
running nan-diameter     ... finished, error code 0
running nan-initflow     ... finished, error code 0
PASS: all three decks ran to the end without a crash
---- CRASH-07 on 6.0.0 (patched) ----
running tiny-diameter    ... finished, error code 0
running nan-diameter     ... finished, error code 5
running nan-initflow     ... finished, error code 14
PASS: all three decks ran to the end without a crash
```

The patched 5.3.0 runs the NaN decks to the end with NaN results (the report's flow routing continuity error for the `nan` initial flow is `nan`); 6.0.0 stops the third with `ERROR 14: the routing solution diverged at node 'J1' (head nan)`. Rejecting `nan` in the input, so that these runs never start, is [IO-01](../../4-io/IO-01-nan-inf-accepted-as-numbers/)'s fix; this one removes the undefined behaviour that a bad value can reach. The 1e-30-ft deck runs in both patched engines with the same routing continuity error, 3.479 %.

## The fix

Check the ratio before converting it, with comparisons that are false for NaN, and use the end of the table it would have been clamped to:

```diff
     // --- find which segment of table contains x
+    //     (x outside 0-1, or NaN, cannot be converted to a table index)
     delta = 1.0 / ((double)nItems-1);
+    if ( !(x > 0.0) ) return table[0];
+    if ( x > 1.0 ) return table[nItems-1];
     i = (int)(x / delta);
```

`tabular_getdSdA()` gets the same two checks, setting `i` to 0 or `nItems - 2`, and the cap on the critical-depth estimate in `xsect_getYcrit()` becomes `if ( !(y < xsect->yFull) )`, so a NaN estimate is replaced by 0.97 yFull like a too-large one. The 6.0.0 patch makes the same two changes in `XSectKernels.hpp`; its `lookup()` already has these checks.

For any value between 0 and 1, the code path, and so every result, is unchanged; values at or above 1 already took the top end of the table. Values at or below 0 now return the first table entry: for a value between −delta and 0 the old code extrapolated the first segment and clamped negative results to 0, and below −delta it read before the table.

**Effect on other models.** All 73 regression decks were run with the base and the patched 5.3.0 and 6.0.0 command-line programs. The 144 runs that complete produced byte-identical .out files; `update_v5111/ncdc_format.inp` stops with an input error in both builds of both engines, with identical reports.
