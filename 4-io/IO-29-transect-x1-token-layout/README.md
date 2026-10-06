# IO-29: An X1 line written as the manual documents it is misread

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A transect whose X1 line follows the manual's format loses its meander factor, and that factor is applied as a width multiplier instead. In the test (Lfactor 2.0) the channel is built twice as wide: full area 300 ft² instead of 150 ft², and full flow 1085.68 cfs instead of 571.65 cfs. No warning is given. |
| **Reached from** | `[TRANSECTS]` X1 lines with 11 items, as in the manual: `X1 Name Nsta Xleft Xright 0 0 0 Lfactor Wfactor Eoffset` |
| **5.3.0** | `transect_readParams()` / `setParams()` in [`src/legacy/engine/transect.c:154`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L154) and [`:376-382`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L376) |
| **5.2.4** | Same code, [`src/solver/transect.c:152`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/transect.c#L152) and [`:347-353`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/transect.c#L347) |
| **6.0.0** | Reproduces: [`handle_transects()`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/LinksHandler.cpp#L640) reads the same item positions. Its comment ([`:594`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/LinksHandler.cpp#L594)) says the manual's line is wrong, but the 6.0.0 engine manual still documents it. |
| **Since** | Every SWMM 5 release |
| **Fix** | Read the three factors one item later when the X1 line has 11 items: [`IO-29_swmm530.patch`](IO-29_swmm530.patch), [`IO-29_swmm600.patch`](IO-29_swmm600.patch). The manual should also show the 10-item line the GUI writes. |

## The problem

The `[TRANSECTS]` section of the engine manual ([`docs/manuals/engine/sections/Chapter2-InputFileReference.md`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/docs/manuals/engine/sections/Chapter2-InputFileReference.md)) gives the format of the X1 line as

```
X1  Name  Nsta  Xleft  Xright  0  0  0  Lfactor  Wfactor  Eoffset
```

which is 11 items with three placeholder zeros. The engine reads a different layout: two placeholder zeros, with Lfactor, Wfactor and Eoffset as items 8, 9 and 10. The GUI writes that layout, and so does 6.0.0's `InpWriter`. Every X1 line in the regression decks (240 lines) has 10 items.

An 11-item line passes the `ntoks < 10` check, and the parser then reads it one item early:

| Item | 8 | 9 | 10 | 11 |
|---|---|---|---|---|
| Manual's meaning | 0 | Lfactor | Wfactor | Eoffset |
| Engine reads it as | Lfactor | Wfactor | Eoffset | (ignored) |

So the meander factor becomes 0 (taken as 1.0, no meander), the user's Lfactor multiplies the station distances, the user's Wfactor is added to every elevation, and the user's Eoffset is dropped. In the test, a 40 ft wide channel with Lfactor 2.0 comes out 80 ft wide with twice the full area, nearly twice the full-flow capacity, and no meander adjustment of its roughness.

## Why it happens

```c
// src/legacy/engine/transect.c, transect_readParams(), X1 line
        if ( ntoks < 10 ) return error_setInpError(ERR_ITEMS, "");
        ...
        for ( i = 2; i < 10; i++ )
        {
            if ( ! getDouble(tok[i], &x[i]) )
                return error_setInpError(ERR_NUMBER, tok[i]);
        }

// setParams()
    Lfactor = x[7];                              // channel/bank length
    ...
    Xfactor = x[8];                              // station location multiplier
    ...
    Yfactor = x[9] / UCF(LENGTH);                // elevation offset
```

Items after the tenth are never looked at. The engine's layout matches the HEC-2 X1 card that the format comes from: fields 5 to 7 hold the left overbank, right overbank and channel reach lengths, and fields 8 and 9 hold the station multiplier and the elevation offset. SWMM leaves fields 5 and 6 at zero and puts its meander ratio in field 7, the channel length field. The manual's third zero is an extra item.

6.0.0 reads the same positions:

```cpp
// src/engine/input/handlers/LinksHandler.cpp, handle_transects()
            double lFactor = (tok.size() > 7) ? to_double(tok[7]) : 1.0;
            double xFactor = (tok.size() > 8) ? to_double(tok[8]) : 1.0;
            double yFactor = (tok.size() > 9) ? to_double(tok[9]) : 0.0;
```

## How to reproduce

| File | What it is |
|---|---|
| [`IO-29_x1-layouts.inp`](IO-29_x1-layouts.inp) | The same channel twice, with Lfactor 2.0: T1 (conduit C1) with the 10-item X1 line, T2 (conduit C2) with the manual's 11-item line |
| [`IO-29_test.c`](IO-29_test.c) | Runs the deck through the legacy toolkit (5.2.4, 5.3.0) and reads the Cross Section Summary |
| [`IO-29_test6.c`](IO-29_test6.c) | The same through `swmm_engine_run()` (6.0.0) |

The GR data is a trapezoid 5 ft deep, 20 ft wide at the bottom and 40 ft at the top, so both conduits should show a full area of 150 ft² and a width of 40 ft. The two transects are the same, so C2's hydraulic radius and full flow must equal C1's.

```sh
tools/run-test.sh IO-29            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-29 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0 and 6.0.0 print the same table; 5.2.4 gives full flows of 570.12 and 1082.77 cfs):

```
Link  X1 line              Depth    Area  HydRad   Width  FullFlow
                            (ft)   (ft2)    (ft)    (ft)     (cfs)
C1    10 items (GUI)        5.00  150.00    3.80   40.00    571.65
C2    11 items (manual)     5.00  300.00    3.51   80.00   1085.68
expected (both)             5.00  150.00           40.00   (C2 = C1)
FAIL: the X1 line written as documented gives full area 300.00 ft2 and width 80.00 ft (expected 150.00 and 40.00) and full flow 1085.68 cfs instead of 571.65
IO-29 5.3.0 base: FAIL
IO-29 6.0.0 base: FAIL
```

**With the fix** (both engines):

```
C1    10 items (GUI)        5.00  150.00    3.80   40.00    571.65
C2    11 items (manual)     5.00  150.00    3.80   40.00    571.65
expected (both)             5.00  150.00           40.00   (C2 = C1)
PASS: both X1 layouts give the same transect (area 150 ft2, width 40 ft, full flow 571.65 cfs)
IO-29 5.3.0 patched: PASS
IO-29 6.0.0 patched: PASS
```

## The fix

When the X1 line has 11 or more items, read Lfactor, Wfactor and Eoffset from items 9 to 11:

```diff
+        // --- the layout given in the manual has three placeholder zeros
+        //     before Lfactor (11 items) instead of two (10 items)
+        if ( ntoks >= 11 ) for ( i = 7; i < 10; i++ )
+        {
+            if ( ! getDouble(tok[i+1], &x[i]) )
+                return error_setInpError(ERR_NUMBER, tok[i+1]);
+        }
```

The 6.0.0 patch starts the three factors at item 9 instead of item 8 in the same case. Lines with 10 items, which is what the GUI, 6.0.0's writer and every regression deck use, are read exactly as before. **Effect on other models:** none of the 240 X1 lines in the regression decks has more than 10 items, so no regression result can change.

An 11-item line that was written in the engine's layout with something extra at the end would now be read differently. No known writer produces such lines. The alternative is to reject 11-item lines with an error, which would make every file written from the manual fail. The manual's format line should also be corrected to the 10-item layout and say that the 11-item form is accepted. That part is documentation and is not in the patches.
