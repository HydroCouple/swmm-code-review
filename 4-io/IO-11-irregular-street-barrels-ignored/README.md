# IO-11: The Barrels item of an IRREGULAR cross section is ignored

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A natural channel conduit written with 2 or more barrels runs with one, without a message, so it carries all the flow in a single channel. In the test, two barrels carrying 20 cfs leave the inlet junction at 0.995 ft instead of 0.696 ft. 6.0.0 reads the value, so the same input file gives different results in 5.3.0 and 6.0.0. |
| **Reached from** | `[XSECTIONS]` lines `Link IRREGULAR Tsect 0 0 0 Barrels` with Barrels > 1 (the GUI writes IRREGULAR lines in this layout); the same for STREET lines (see the end) |
| **5.3.0** | `link_readXsectParams()` in [`src/legacy/engine/link.c:199`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L199) (IRREGULAR) and [`:209`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L209) (STREET) |
| **5.2.4** | Same code, [`src/solver/link.c:196`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L196) |
| **6.0.0** | Not affected: [`handle_xsections()`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/LinksHandler.cpp#L495) reads Barrels for every shape |
| **Since** | Every SWMM 5 release: 5.0 and 5.1 read Barrels only in the branch for regular shapes; 5.2 kept that with an early `return 0` |
| **Fix** | Read Barrels on IRREGULAR lines as on the others: [`IO-11_swmm530.patch`](IO-11_swmm530.patch) |

## The problem

The manual defines Barrels for every `[XSECTIONS]` line as the "number of barrels (i.e., number of parallel pipes of equal size, slope, and roughness) associated with a conduit (default is 1)". Its format line for natural channels is the short form `Link IRREGULAR Tsect`. The GUI, however, writes IRREGULAR lines with the full column layout, `Link IRREGULAR Tsect 0 0 0 Barrels`. The regression decks contain 231 such lines, all with Barrels = 1.

The legacy reader stops reading an IRREGULAR line after the transect name. A Barrels value of 2 or more on that line is ignored, and nothing in the report says so: the Cross Section Summary just lists 1 barrel. The conduit then carries the whole flow through one channel, and the water upstream stands higher than it should.

6.0.0 reads the value. A model with a multi-barrel natural channel therefore runs with one barrel in 5.2.4 and 5.3.0 and with the number given in 6.0.0.

## Why it happens

```c
// src/legacy/engine/link.c, link_readXsectParams()
    // --- assign default number of barrels to conduit
    if ( Link[j].type == CONDUIT ) Conduit[Link[j].subIndex].barrels = 1;
    ...
    if ( k == IRREGULAR )
    {
        i = project_findObject(TRANSECT, tok[2]);
        if ( i < 0 ) return error_setInpError(ERR_NAME, tok[2]);
        Link[j].xsect.type = k;
        Link[j].xsect.transect = i;
        return 0;                               // Barrels (tok[6]) is never read
    }
    ...
        // --- parse number of barrels if present
        if ( Link[j].type == CONDUIT && ntoks >= 7 )
        ...
```

Everything downstream of the reader already handles barrels for any shape: flows, areas, volumes and losses are computed per barrel and multiplied by `Conduit[k].barrels`.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-11_irregular-barrels.inp`](IO-11_irregular-barrels.inp) | Two identical channels on the same transect: C1 with 2 barrels and 20 cfs, C2 with 1 barrel and 10 cfs |
| [`IO-11_test.c`](IO-11_test.c) | Runs the deck through the legacy toolkit (5.2.4, 5.3.0), reads the barrel count from the report and the junction depths at the end of the run |
| [`IO-11_test6.c`](IO-11_test6.c) | The same through the 6.0.0 API |

With 2 barrels, each barrel of C1 carries 10 cfs, exactly what C2's single barrel carries, so J1 and J2 must end the 2-hour run at the same depth. The test allows 0.005 ft. Ignoring the second barrel makes the difference 0.3 ft.

```sh
tools/run-test.sh IO-11            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-11 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.3.0; 5.2.4 gives 0.997 and 0.697 ft):

```
Link  Barrels  Barrels in  Flow    Flow per      Inlet depth
      entered  report      (cfs)   barrel (cfs)  at end (ft)
C1          2           1   20.00         20.00        0.995
C2          1           1   10.00         10.00        0.696
FAIL: C1 was given 2 barrels but has 1; J1 stands at 0.995 ft instead of 0.696 ft (the depth of one barrel carrying 10 cfs)
IO-11 5.3.0 base: FAIL
```

**6.0.0, and 5.3.0 with the fix** (same numbers):

```
Link  Barrels  Barrels in  Flow    Flow per      Inlet depth
      entered  report      (cfs)   barrel (cfs)  at end (ft)
C1          2           2   20.00         10.00        0.696
C2          1           1   10.00         10.00        0.696
PASS: C1 has 2 barrels and each carries the same flow at the same depth as C2
IO-11 5.3.0 patched: PASS
IO-11 6.0.0 base: PASS
```

## The fix

Read the Barrels item in the IRREGULAR branch, with the same check as for the other shapes:

```diff
         Link[j].xsect.type = k;
         Link[j].xsect.transect = i;
+
+        // --- parse number of barrels if present (Tsect 0 0 0 Barrels)
+        if ( Link[j].type == CONDUIT && ntoks >= 7 )
+        {
+            i = atoi(tok[6]);
+            if ( i <= 0 ) return error_setInpError(ERR_NUMBER, tok[6]);
+            Conduit[Link[j].subIndex].barrels = i;
+        }
         return 0;
```

The value is assigned without the `(char)` cast used for the other shapes, so the line stays correct once [IO-10](../IO-10-barrels-stored-as-char/) widens the field to `int`. Short-form lines (`Link IRREGULAR Tsect`) and lines with Barrels = 1 read as before. **Effect on other models:** all 231 IRREGULAR lines with a Barrels item in the regression decks have Barrels = 1, so no regression result changes. An IRREGULAR line with Barrels 0 or a non-number in that column is now rejected with ERROR 211, as it already is for every other shape.

**STREET is left as it is.** STREET lines return early in the same way (`link.c:209`), and 6.0.0 reads Barrels for them too, so the two engines also differ there. A street conduit is not a plain open channel, though. Its inlets (`inlet.c`) compute gutter spread and capture from the conduit's total flow on the one street cross-section, and the `[STREETS]` section already describes one- or two-sided streets with its Sides item. A street with 2 barrels has no consistent meaning in that model. The better fix is to reject Barrels > 1 on STREET lines in both engines, not to read it. That needs a decision from the maintainers and is not in this patch.
