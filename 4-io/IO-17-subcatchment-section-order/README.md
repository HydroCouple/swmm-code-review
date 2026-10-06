# IO-17: Subcatchment data placed above [SUBCATCHMENTS] is lost or misapplied

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Silent wrong runoff when sections are not in the usual order. In the test (1 in of rain on 10 ac, 50 % impervious) the correct infiltration is 0.469 in and runoff 0.492 in. With `[SUBAREAS]` above `[SUBCATCHMENTS]`, 5.2.4 and 5.3.0 run the subcatchment as fully pervious (0.842 / 0.158 in) and 6.0.0 ignores the `[SUBAREAS]` row (0.361 / 0.638 in). With `[ADJUSTMENTS]` above `[SUBCATCHMENTS]` all three drop the N-PERV, DSTORE and INFIL patterns (0.722 / 0.239 in). 6.0.0 also drops `[INFILTRATION]` rows placed above `[SUBCATCHMENTS]` (no infiltration at all). No error or warning in any case. |
| **Reached from** | Input files with `[SUBAREAS]`, `[ADJUSTMENTS]` (subcatchment rows) or, in 6.0.0, `[INFILTRATION]` above `[SUBCATCHMENTS]`; in 6.0.0 also `[ADJUSTMENTS]` above `[PATTERNS]`. 6.0.0's own .inp writer emits `[ADJUSTMENTS]` above both. The input reference says sections may appear in any order |
| **5.3.0** | `subcatch_readSubareaParams()` splits the area with the not yet read % impervious at [`src/legacy/engine/subcatch.c:268`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L268); `subcatch_readParams()` resets the adjustment patterns at [`subcatch.c:204`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/subcatch.c#L204) |
| **5.2.4** | Same code, [`src/solver/subcatch.c:248`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L248) and [`subcatch.c:181`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/subcatch.c#L181) |
| **6.0.0** | Reproduces with a different mechanism: `handle_subareas()` and `handle_infiltration()` skip rows naming a subcatchment not yet declared ([`src/engine/input/handlers/CatchmentHandler.cpp:216`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/CatchmentHandler.cpp#L216), [`:275`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/CatchmentHandler.cpp#L275)), and `handle_adjustments()` skips rows whose subcatchment or pattern is not yet declared ([`InfraHandler.cpp:413`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/InfraHandler.cpp#L413)) |
| **Since** | The sub-area split: every release in the repository (initial commit, 2014). The pattern reset: 5.1.013, which added subcatchment adjustments (2018) |
| **Fix** | 5.3.0: compute the sub-area fractions in `subcatch_validate()` and initialise the patterns once, at allocation: [`IO-17_swmm530.patch`](IO-17_swmm530.patch). 6.0.0: replay such rows after the last section, as it already does for `[XSECTIONS]`: [`IO-17_swmm600.patch`](IO-17_swmm600.patch) Apply after BND-14 (its `Requires:` line). |

## The problem

The input reference says: "The sections can appear in any arbitrary order in the input file". Both legacy readers and 6.0.0 nevertheless need some subcatchment sections to come after `[SUBCATCHMENTS]`.

The test model is one 10-ac subcatchment, 50 % impervious, 25 % of the impervious area without depression storage, 50 % of the impervious runoff routed over the pervious area, Horton infiltration, and `[ADJUSTMENTS]` rows that halve its pervious n, depression storage and infiltration rate. It gets 1 in of rain. The same data in four section orders gives (inches):

| Order | 5.2.4 / 5.3.0 infiltration, runoff | 6.0.0 infiltration, runoff |
|---|---|---|
| every section below `[SUBCATCHMENTS]` | 0.469, 0.492 | 0.469, 0.492 |
| `[SUBAREAS]` above `[SUBCATCHMENTS]` | 0.842, 0.158 | 0.361, 0.638 |
| `[INFILTRATION]` above `[SUBCATCHMENTS]` | 0.469, 0.492 | 0.000, 0.899 |
| `[ADJUSTMENTS]` above `[SUBCATCHMENTS]`, `[PATTERNS]` last | 0.722, 0.239 | 0.722, 0.239 |

(5.2.4 prints 0.493 for the runoff in the first and third rows.) The last order is the one 6.0.0's .inp writer (`InpWriter.cpp`) uses: it emits `[ADJUSTMENTS]` before `[RAINGAGES]` and `[SUBCATCHMENTS]` and `[PATTERNS]` near the end, so a model with subcatchment adjustments that 6.0.0 saves loses them when it is read back by any of the three engines.

## Why it happens

**Legacy, `[SUBAREAS]`.** The sub-area fractions are computed when the `[SUBAREAS]` row is read, from the subcatchment's fraction impervious:

```c
// src/legacy/engine/subcatch.c, subcatch_readSubareaParams()
    Subcatch[j].subArea[IMPERV0].fArea  = Subcatch[j].fracImperv * x[4] / 100.0;
    Subcatch[j].subArea[IMPERV1].fArea  = Subcatch[j].fracImperv * (1.0 - x[4] / 100.0);
    Subcatch[j].subArea[PERV].fArea     = (1.0 - Subcatch[j].fracImperv);
    ...
    k = (int)x[5];
    if ( Subcatch[j].fracImperv == 0.0
    ||   Subcatch[j].fracImperv == 1.0 ) k = TO_OUTLET;
```

`fracImperv` is set by `subcatch_readParams()` from the `[SUBCATCHMENTS]` row. The read pass resolves the subcatchment name through the hash table built in the count pass, so a `[SUBAREAS]` row above `[SUBCATCHMENTS]` is accepted, but `fracImperv` is still 0: the impervious fractions become 0, the pervious fraction 1, and the PERVIOUS/IMPERVIOUS routing is replaced by OUTLET. The later `[SUBCATCHMENTS]` row sets `fracImperv` but nothing recomputes the fractions.

**Legacy, `[ADJUSTMENTS]`.** `climate_readAdjustments()` stores the pattern indexes in `Subcatch[i].nPervPattern`, `dStorePattern` and `infilPattern`. `subcatch_readParams()` uses these fields' only initialisation:

```c
// src/legacy/engine/subcatch.c, subcatch_readParams()
    Subcatch[subcatchIndex].nPervPattern  = -1;
    Subcatch[subcatchIndex].dStorePattern = -1;
    Subcatch[subcatchIndex].infilPattern  = -1;
```

so any adjustment row read before the subcatchment's own row is erased.

**6.0.0.** The handlers look the subcatchment up by name and skip the row if it is not there yet:

```cpp
// src/engine/input/handlers/CatchmentHandler.cpp, handle_subareas() and handle_infiltration()
        const int idx = ctx.subcatch_names.find(tok[0]);
        if (idx < 0) continue;

// src/engine/input/handlers/InfraHandler.cpp, handle_adjustments()
            const int si = ctx.subcatch_names.find(tok[1]);
            const int pi = ctx.patterns.find(tok[2]);
            if (si >= 0 && pi >= 0) { ... }
```

6.0.0 already has the mechanism for this: `[XSECTIONS]`, `[LOSSES]` and `[GROUNDWATER]` rows that name an object declared further down are stashed in `ctx.deferred_section_rows` and replayed by `InputReader::replay_deferred_rows()` after the last section ([IO-16](../IO-16-link-properties-reset-by-section-order/)). These three handlers do not use it. The `[SUBAREAS]` and `[INFILTRATION]` rows of the subcatchment are dropped entirely, so it runs with the default parameters; legacy SWMM would report an undefined subcatchment as ERROR 209, 6.0.0 says nothing.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-17_normal-order.inp`](IO-17_normal-order.inp) | The reference: every subcatchment section below `[SUBCATCHMENTS]` |
| [`IO-17_subareas-first.inp`](IO-17_subareas-first.inp) | The same model with `[SUBAREAS]` above `[SUBCATCHMENTS]` |
| [`IO-17_infiltration-first.inp`](IO-17_infiltration-first.inp) | With `[INFILTRATION]` above `[SUBCATCHMENTS]` |
| [`IO-17_adjustments-first.inp`](IO-17_adjustments-first.inp) | With `[ADJUSTMENTS]` above `[SUBCATCHMENTS]` and `[PATTERNS]` at the end, as 6.0.0 writes them |
| [`IO-17_test.c`](IO-17_test.c) | Runs the four decks through the legacy toolkit and compares each report's Runoff Quantity Continuity (infiltration, runoff, final storage) with the reference order |
| [`IO-17_test6.c`](IO-17_test6.c) | The same check through the 6.0.0 API |

```sh
tools/run-test.sh IO-17            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-17 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.3.0 (5.2.4 is the same apart from 0.493 for the reference runoff):

```
Runoff continuity of S1 (inches):
  deck                  infiltration        runoff final storage
  normal-order                 0.469         0.492         0.038
  subareas-first               0.842         0.158         0.000
  infiltration-first           0.469         0.492         0.038
  adjustments-first            0.722         0.239         0.038
  (correct: every row equals normal-order)
FAIL: the results depend on the section order: subareas-first, adjustments-first differ from normal-order by up to 0.373 in
IO-17 5.3.0 base: FAIL
```

6.0.0:

```
  normal-order                 0.469         0.492         0.038
  subareas-first               0.361         0.638         0.001
  infiltration-first           0.000         0.899         0.102
  adjustments-first            0.722         0.239         0.038
  (correct: every row equals normal-order)
FAIL: the results depend on the section order: subareas-first, infiltration-first, adjustments-first differ from normal-order by up to 0.469 in
IO-17 6.0.0 base: FAIL
```

**With the fix**, both patched engines print the same table:

```
  normal-order                 0.469         0.492         0.038
  subareas-first               0.469         0.492         0.038
  infiltration-first           0.469         0.492         0.038
  adjustments-first            0.469         0.492         0.038
  (correct: every row equals normal-order)
PASS: the subcatchment gives the same results whatever the section order
IO-17 5.3.0 patched: PASS
IO-17 6.0.0 patched: PASS
```

## The fix

**5.3.0.** `subcatch_readSubareaParams()` keeps PctZero in a new field, `Subcatch[].pctZero`, and no longer applies the "0 % or 100 % impervious" routing override; `subcatch_validate()`, which runs after all sections are read, computes the fractions with the same expressions and applies the override:

```diff
-    Subcatch[j].subArea[IMPERV0].fArea  = Subcatch[j].fracImperv * x[4] / 100.0;
-    Subcatch[j].subArea[IMPERV1].fArea  = Subcatch[j].fracImperv * (1.0 - x[4] / 100.0);
-    Subcatch[j].subArea[PERV].fArea     = (1.0 - Subcatch[j].fracImperv);
+    Subcatch[j].pctZero = x[4];
 ...
+    // subcatch_validate()
+    f = Subcatch[subcatchIndex].fracImperv;
+    pctZero = Subcatch[subcatchIndex].pctZero;
+    if ( pctZero >= 0.0 )
+    {
+        Subcatch[subcatchIndex].subArea[IMPERV0].fArea = f * pctZero / 100.0;
+        Subcatch[subcatchIndex].subArea[IMPERV1].fArea = f * (1.0 - pctZero / 100.0);
+        Subcatch[subcatchIndex].subArea[PERV].fArea    = (1.0 - f);
+        if ( f == 0.0 || f == 1.0 ) for (i = IMPERV0; i <= PERV; i++) { ...TO_OUTLET... }
+    }
```

`pctZero` starts at −1 so that a subcatchment without a `[SUBAREAS]` row keeps the all-zero sub-areas it has today. The three pattern indexes move from `subcatch_readParams()` to the subcatchment defaults in `project.c` (`createObjects()`), where `outNode`, `infil` and the scale factors are already initialised.

**6.0.0.** Each of the three handlers stashes a row it cannot resolve yet, as `handle_xsections()` does:

```diff
         const int idx = ctx.subcatch_names.find(tok[0]);
-        if (idx < 0) continue;
+        if (idx < 0) {
+            ctx.deferred_section_rows.emplace_back("SUBAREAS", line);
+            continue;
+        }
```

(the same for `INFILTRATION`, and an `else` branch for the N-PERV, DSTORE and INFIL rows of `[ADJUSTMENTS]`). A row that still names no subcatchment or pattern after the replay is reported as ERROR 209, as legacy SWMM reports it at once; before the fix 6.0.0 ignored such rows.

**Effect on other models.** Decks in the usual order give the same results. The 43 regression decks with a `[SUBAREAS]` section were run with the base and the patched 5.3.0 and 6.0.0 command-line programs: all 84 runs that complete produced byte-identical .out files (`update_v5111/ncdc_format.inp` stops with ERROR 317 in both builds, with identical reports).

**Related.** 6.0.0's `[COVERAGES]`, `[LOADINGS]` and `[LID_USAGE]` handlers skip rows in the same way (`QualityHandler.cpp:263`, `:478`, `HydrologyHandler.cpp:659`), and `[COVERAGES]` returns early when no subcatchment or land use has been read yet. With `[COVERAGES]` above `[SUBCATCHMENTS]` a deck's 19.4 lb of washoff becomes 0 in 6.0.0, while 5.3.0 is unaffected. These handlers need their arrays sized before a replay, so they are not part of this fix.
