# IO-10: The number of barrels is stored in a char and wraps above 127

**Status: not fixed. PR #192 was closed without merging on 16 November 2026 and issue #177 closed as not planned: the defect is in the bundled legacy 5.3.0 solver only, which is kept at parity with EPA SWMM 5.2.4 (same behavior); 6.0.0 is not affected. Separately, 6.0.0 now stops with ERROR 211 on a Barrels item below 1 or not a number, as legacy does ([issue #209](https://github.com/HydroCouple/openswmm.engine/issues/209), [PR #210](https://github.com/HydroCouple/openswmm.engine/pull/210), merge 85a0cb90).** [Issue #177](https://github.com/HydroCouple/openswmm.engine/issues/177) · [Engine PR #192](https://github.com/HydroCouple/openswmm.engine/pull/192) · reviewed head `4cc91c9d50132bda5085b15171cc55c7c12e600a` (validated 28 October 2026). OpenSWMM 6.0.0 is not affected.

The reproduction below records the original reviewed revisions; the follow-up validation at the end records the submitted PR.

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A conduit with more than 127 barrels gets a different number with no warning: 300 barrels become 44, so the conduit carries 15 % of its capacity (in the test the inlet junction surcharges to 3.88 ft instead of flowing 0.47 ft deep). 128 to 255 barrels become negative where `char` is signed (x86-64, and Windows and macOS on any CPU) and the run stops with `ERROR 114: invalid number of barrels`, but they are kept where `char` is unsigned (ARM Linux), so the same file behaves differently on different machines. |
| **Reached from** | `[XSECTIONS]` Barrels item (7th) above 127 |
| **5.3.0** | `link_readXsectParams()` in [`src/legacy/engine/link.c:258`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L258); field in [`objects.h:734`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/objects.h#L734) |
| **5.2.4** | Same code, [`src/solver/link.c:255`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L255), [`objects.h:715`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/objects.h#L715) |
| **6.0.0** | Not affected: the count is an `int` ([`LinkSubtypes.hpp:92`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/data/LinkSubtypes.hpp#L92)) |
| **Since** | Every SWMM 5 release (5.0.022 declares the field `char`) |
| **Fix** | Declare the field `int`: [`IO-10_swmm530.patch`](IO-10_swmm530.patch) |

## The problem

The manual describes Barrels as the "number of barrels (i.e., number of parallel pipes of equal size, slope, and roughness) associated with a conduit" and gives no upper limit. More than 127 identical pipes is unusual for a single structure, but it is a reasonable way to lump many small parallel pipes or drains into one conduit, and the reader accepts it without a message. The value is then stored in a `char`:

- 300 is stored as 44 (300 − 256). The conduit has 44 barrels for the whole run. Every flow, area and volume per barrel is multiplied by 44, and the Cross Section Summary in the report shows 44.
- 200 is stored as −56 on platforms where `char` is signed. `link_validate()` then reports `ERROR 114: invalid number of barrels for Conduit C1.` for a count the user entered as 200. Where `char` is unsigned, as on ARM Linux, 200 is kept and the model runs.

In the test, 100 cfs enters a conduit of 300 one-foot pipes on a 0.1 % slope (1.13 cfs each when full). With 300 barrels the junction runs 0.47 ft deep. With the 44 barrels the engine keeps, the total capacity is 49.7 cfs, the pipes surcharge and the junction stands at 3.88 ft.

## Why it happens

```c
// src/legacy/engine/objects.h, TConduit
   char          barrels;         // number of barrels

// src/legacy/engine/link.c, link_readXsectParams()
            i = atoi(tok[6]);
            if ( i <= 0 ) return error_setInpError(ERR_NUMBER, tok[6]);
            else Conduit[Link[j].subIndex].barrels = (char)i;
```

The conversion of an out-of-range `int` to a signed `char` is implementation-defined in C (C17 6.3.1.3), so the result depends on the compiler and platform. It is not undefined behaviour, and the sanitizers do not report it.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-10_300-barrels.inp`](IO-10_300-barrels.inp) | One conduit of 300 one-foot pipes, 1000 ft at 0.1 %, carrying 100 cfs |
| [`IO-10_200-barrels.inp`](IO-10_200-barrels.inp) | The same with 200 barrels |
| [`IO-10_test.c`](IO-10_test.c) | Runs both decks through the legacy toolkit (5.2.4, 5.3.0) and reads the barrel count back from the report's Cross Section Summary |
| [`IO-10_test6.c`](IO-10_test6.c) | The same through the 6.0.0 API |

```sh
tools/run-test.sh IO-10            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-10 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0, x86-64):

```
Barrels   error  Barrels in  Full flow per  Total capacity  J1 depth
entered          report      barrel (cfs)   (cfs)           at end (ft)
    300       0          44           1.13            49.7       3.88
    200     114  (rejected)
FAIL: 300 barrels became 44, 200 barrels rejected (error 114)
IO-10 5.3.0 base: FAIL
```

**6.0.0, and 5.3.0 with the fix** (same numbers):

```
Barrels   error  Barrels in  Full flow per  Total capacity  J1 depth
entered          report      barrel (cfs)   (cfs)           at end (ft)
    300       0         300           1.13           339.0       0.47
    200       0         200           1.13           226.0       0.59
PASS: the conduits have the number of barrels that was entered
IO-10 5.3.0 patched: PASS
IO-10 6.0.0 base: PASS
```

## The fix

```diff
-   char          barrels;         // number of barrels
+   int           barrels;         // number of barrels
```
```diff
-            else Conduit[Link[j].subIndex].barrels = (char)i;
+            else Conduit[Link[j].subIndex].barrels = i;
```

Every other use of the field is arithmetic (`flow / barrels`, `barrels * seepLossRate`, `(double)barrels`) or a `%d` format, so counts of 1 to 127 give identical results. **Effect on other models:** `user/user2.inp` and `extran/extran8a.inp` give byte-identical `.rpt` files (apart from the run timestamps) and identical `.out` files with the patched 5.3.0.

## Fix validation (28 October 2026)

[PR #192](https://github.com/HydroCouple/openswmm.engine/pull/192), head `4cc91c9d50132bda5085b15171cc55c7c12e600a`, widens `TConduit.barrels` to `int` and removes the narrowing cast in the reader, matching the review patch. Other uses of the field continue to use integer arithmetic, floating-point scaling or integer report formatting.

Validation used isolated copies of PR base `fbd1d7b03c036d7a80e9bb64c9781dc5ccf464c2` plus the submitted changes, on macOS arm64 / Apple Clang 21.0.0, with `-O1 -g`, AddressSanitizer and UndefinedBehaviorSanitizer. Production source blob hashes were checked against GitHub's PR file metadata.

- The PR's Google Test and the original `IO-10_test.c` fail on the uncorrected base: 300 barrels are reported as 44 and 200 barrels are rejected with ERROR 114.
- Both tests pass with PR #192, without sanitizer errors. The original reproducer reports **300 / 200 barrels**, total full-flow capacities **339.0 / 226.0 cfs** (from the report's rounded per-barrel flow), and inlet depths **0.47 / 0.59 ft**, respectively.
- Five existing legacy test programs (`test_solver_api`, `test_solver_errors`, `test_solver_shapes`, `test_solver_storage_shapes`, `test_solver_transect_nc`) pass **81 Google Tests** on both the base and PR #192. The broader runs use the review's existing `tools/ubsan.supp` for the unrelated CRASH-01 datetime conversion; the focused reproducers run without it.
- The PR test and original reproducer also pass with PRs #191–#193 combined. Widening the shared field is compatible with IO-11's integer assignment; IO-11's missing guard is recorded separately.

The implementation is confirmed for 5.3.0, pending merge. 6.0.0 already uses an integer count and was not rebuilt for this follow-up. The original patch and historical reproduction results are preserved.

## Outcome (16 November 2026)

[PR #192](https://github.com/HydroCouple/openswmm.engine/pull/192) was closed without merging and [issue #177](https://github.com/HydroCouple/openswmm.engine/issues/177) closed as not planned by the maintainers: the defect is in the bundled legacy 5.3.0 solver only, which is kept at parity with EPA SWMM 5.2.4 (same behavior); 6.0.0 is not affected (checked on `swmm6_rel` 87664bf5 with AddressSanitizer and UBSan, per the closing comments). The validation above stands as the record of the submitted correction.
