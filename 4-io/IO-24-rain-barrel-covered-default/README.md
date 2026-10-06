# IO-24: Rain barrel Covrd token: the manual's default is not the parser's, and 6.0.0 ignores the token

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A rain barrel whose STORAGE line has no Covrd token is uncovered (it collects the rain on its own top), while the input reference says such a barrel is covered. In 6.0.0 an explicit `YES` is ignored as well: every barrel is uncovered. In the test, a barrel declared covered receives 2.00 in of rain in 6.0.0 where 5.2.4 and 5.3.0 give it 0.00 in, so it has less room for roof runoff and 6.0.0's results differ from 5.3.0's. No warning in either case. |
| **Reached from** | `[LID_CONTROLS]` STORAGE lines of RB controls: without the Covrd value (manual vs. code, all versions), or with `YES` (6.0.0) |
| **5.3.0** | `readStorageData()` in [`src/legacy/engine/lid.c:721`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lid.c#L721) starts from `covered = FALSE` and sets it only for `YES` ([line 735](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/lid.c#L735)). The code is right; the input reference is wrong |
| **5.2.4** | Same code, [`src/solver/lid.c:717`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/lid.c#L717) |
| **6.0.0** | Reproduces in a worse form: `handle_lid_controls()` in [`src/engine/input/handlers/HydrologyHandler.cpp:614`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/HydrologyHandler.cpp#L614) reads only the four numbers, and `stor_covered` keeps its initial 0 ([`src/engine/hydrology/LID.cpp:72`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/LID.cpp#L72)) |
| **Since** | 5.2.0, which added the Covrd option with the code default NO; 6.0.0 since its STORAGE parser was written |
| **Fix** | Correct the input reference (text below). 6.0.0: read the token, [`IO-24_swmm600.patch`](IO-24_swmm600.patch) |

## The problem

5.2.0 added an optional fifth value to the STORAGE line of a rain barrel:

```
Name STORAGE  Height Vratio Seepage Vclog (Covrd)
```

A covered barrel receives no direct rainfall; an uncovered one collects the rain falling on its top in addition to the runoff it captures. The input reference (`docs/manuals/engine/sections/Chapter2-InputFileReference.md`, line 613, copied from the EPA 5.2 manual) says:

```
Covrd    YES (the default) if a rain barrel is covered, NO if it is not.
```

Every release parses the opposite default: a line without the token gives an uncovered barrel. The reference's own example, `RB12 STORAGE 36 0 0 0` (line 663), and `examples/Example4.inp` (`RainBarrels STORAGE 48 1 0 0`) are therefore uncovered barrels, while a reader of the manual expects covered ones.

6.0.0 goes further and does not read the token at all, so a barrel declared `YES` collects rain too. The covered-barrel branch that 6.0.0 ported from 5.3.0 (`SWMMEngine::stepRunoff()`, [`SWMMEngine.cpp:2493`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L2493)) never runs.

The test deck has three identical 10 ac subcatchments, each with 100 barrels of 50 ft2 that capture no runoff (FromImp = 0) and have no drain, under 2.0 in of rain. The only water a barrel can hold is the rain on its own top:

| Barrel | STORAGE line ends with | 5.2.4 and 5.3.0 | 6.0.0 |
|---|---|---|---|
| RB_YES | `YES` | 0.00 in | 2.00 in |
| RB_NO | `NO` | 2.00 in | 2.00 in |
| RB_DEF | (nothing) | 2.00 in | 2.00 in |

## Why it happens

The legacy parser starts from "uncovered" and changes it only for `YES`:

```c
// src/legacy/engine/lid.c, readStorageData()
//    LID_ID STORAGE  Thickness  VoidRatio  Ksat  ClogFactor  (YES/NO)
    int    covered = FALSE;
    ...
    //... check if rain barrel is covered
    if (ntoks > 6)
    {
        if (match(toks[6], w_YES))
            covered = TRUE;
    }
```

This keeps the behaviour of decks written before 5.2.0: 5.1 had no Covrd option and added the rainfall to every LID unit, rain barrels included (`lidInflow = lidInflow + Subcatch[j].rainfall` in 5.1.015's `lid_getRunoff()`). Changing the code to match the manual would change the results of every existing token-less deck, Example4 included, so the manual is the side to correct.

6.0.0 stores four numbers per STORAGE line and nothing else:

```cpp
// src/engine/input/handlers/HydrologyHandler.cpp, handle_lid_controls()
else if (layer == "STORAGE") {
    for (std::size_t i = 0; i < 4 && (i + 2) < tok.size(); ++i)
        ctx.lid_controls.storage[idx][i] = to_double(tok[2 + i]);
}
```

`LIDSolver::init()` copies those four values into the unit's storage layer and never touches `stor_covered`, which `LIDGroupSoA::resize()` initialises to 0.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-24_covrd.inp`](IO-24_covrd.inp) | Three 10 ac subcatchments (50 % impervious, no infiltration), each with 100 rain barrels of 50 ft2, FromImp = 0, no drain; STORAGE lines ending in `YES`, `NO` and nothing; 1 in/hr for 2 h |
| [`IO-24_test.c`](IO-24_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0) and reads each barrel's Total Inflow from the LID Performance Summary |
| [`IO-24_test6.c`](IO-24_test6.c) | The same with the 6.0.0 engine API |

```sh
tools/run-test.sh IO-24            # 5.2.4 and 5.3.0: PASS; 6.0.0: FAIL
tools/run-test.sh IO-24 --patched  # 6.0.0 with the fix: PASS (5.3.0 has no patch)
```

**Without the fix**, 6.0.0 fills the covered barrel with rain:

```
barrel   Covrd    barrel inflow (in)
                  expected  reported
RB_YES   YES        0.00      2.00
RB_NO    NO         2.00      2.00
RB_DEF   (none)     2.00      2.00
FAIL: 1 of 3 barrels ignore their Covrd setting (RB_YES 2.00 in, RB_NO 2.00 in, no token 2.00 in)
IO-24 6.0.0 base: FAIL
```

5.2.4 and 5.3.0 read the token, and treat a missing token as NO:

```
RB_YES   YES        0.00      0.00
RB_NO    NO         2.00      2.00
RB_DEF   (none)     2.00      2.00
PASS: YES gives a covered barrel (0.00 in), NO and no token an uncovered one (2.00 in)
IO-24 5.2.4 base: PASS
IO-24 5.3.0 base: PASS
```

**With the fix**, 6.0.0 prints the 5.3.0 values:

```
RB_YES   YES        0.00      0.00
RB_NO    NO         2.00      2.00
RB_DEF   (none)     2.00      2.00
PASS: YES gives a covered barrel (0.00 in), NO and no token an uncovered one (2.00 in)
IO-24 6.0.0 patched: PASS
```

Its runoff continuity table and Subcatchment Runoff Summary for this deck are then identical to 5.3.0's (S1 total runoff 1.98 in, continuity error -0.011 %).

## The fix

**Documentation.** In the input reference (`[LID_CONTROLS]`, storage layer parameters) replace

```
Covrd    YES (the default) if a rain barrel is covered, NO if it is not.
```

with

```
Covrd    YES if a rain barrel is covered, NO (the default) if it is not.
```

**6.0.0.** Keep the flag in a vector beside the STORAGE parameters (so the parameter array, which the unit tests and the API fill, keeps its size), set it the way `readStorageData()` does, and copy it to the unit:

```diff
     /** @brief STORAGE layer params (up to 4 values). */
     std::vector<std::array<double, 4>> storage;
+
+    /** @brief 1 if a rain barrel's storage is covered (STORAGE ... Covrd = YES). */
+    std::vector<int> storage_covered;
 ...
             for (std::size_t i = 0; i < 4 && (i + 2) < tok.size(); ++i)
                 ctx.lid_controls.storage[idx][i] = to_double(tok[2 + i]);
+            // Covrd (rain barrels): legacy readStorageData, match(tok, "YES")
+            ctx.lid_controls.storage_covered[idx] =
+                (tok.size() > 6 && Tokenizer::to_upper(tok[6]).rfind("YES", 0) == 0) ? 1 : 0;
 ...
             g.stor_clog[us]  = p[3];
+            g.stor_covered[us] = (uli < ctx.lid_controls.storage_covered.size() &&
+                                  ctx.lid_controls.storage_covered[uli]) ? 1 : 0;   // Covrd
```

The `.inp` writer also writes `YES` back, so a model saved by 6.0.0 keeps its covered barrels. The GeoPackage reader and writer still carry only the four numbers.

With the flag read, 6.0.0 runs the covered-barrel branch it ported from 5.3.0, and with it the two defects of that branch: the rain returned to the pervious area is computed over the subcatchment's whole LID area ([CON-04](../../2-conceptual/CON-04-covered-barrel-returns-rain-over-all-lids/)), and it is lost when the subcatchment has no pervious area ([CON-05](../../2-conceptual/CON-05-covered-barrel-rain-lost/)). Their 6.0.0 patches require this one.

**Effect on other models.** Only decks with `YES` on a rain barrel's STORAGE line change. No regression deck has one; `examples/Example4.inp` (a token-less rain barrel and five other LID types) gives byte-identical report and output files with the patched 6.0.0 command-line program.
