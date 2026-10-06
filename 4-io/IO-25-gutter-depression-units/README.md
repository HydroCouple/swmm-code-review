# IO-25: The input reference gives gutter depressions in inches or mm; the engine reads feet or metres

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | A gutter depression entered as the input reference says is 12 times too deep in US units and 1000 times in SI. A 1-inch depression entered as `1` becomes a 1-ft gutter: in HEC-22 Example 4-9 the curb inlet then captures 100% of the flow instead of 87%. Nothing checks the value against the curb height, so there is no warning. OpenSWMM's own inlet example has a 2-ft deep gutter under a 0.5-ft curb for this reason. |
| **Reached from** | `[STREETS]` parameter `a`, `[INLET_USAGE]` and `[INLET_JUNCTIONS]` parameter `aLocal` |
| **5.3.0** | Code reads ft/m: `street_readParams()` in [`src/legacy/engine/street.c:129`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/street.c#L129) and `inlet_readUsageParams()` in [`src/legacy/engine/inlet.c:453`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inlet.c#L453). The defect is in the input reference, [`docs/manuals/engine/sections/Chapter2-InputFileReference.md:1224`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/docs/manuals/engine/sections/Chapter2-InputFileReference.md#L1224), `:1302` and `:1845` |
| **5.2.4** | Same code, [`src/solver/street.c:129`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/street.c#L129) |
| **6.0.0** | Code reads ft/m too ([`src/engine/input/PostParseResolver.cpp:3179`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/PostParseResolver.cpp#L3179)). Its example `examples/inlets/street_inlet_junction.inp` enters `a = 2.0` under a 0.5-ft curb. |
| **Since** | 5.2.0, when streets and inlets were added |
| **Fix** | Correct the manual to "(ft or m)": [`IO-25_docs.patch`](IO-25_docs.patch). No engine patch. |

## The problem

The input reference lists every street dimension in "ft or m" except one:

```
[STREETS]
Name Tcrown Hcurb Sx nRoad (a W)(Sides Tback Sback nBack)
...
Hcurb     curb height (ft or m)
a         gutter depression height (in or mm) (default = 0)
W         depressed gutter width (ft or m) (default = 0)
```

`[INLET_USAGE]` and 6.0.0's `[INLET_JUNCTIONS]` describe the local depression `aLocal` the same way. All three engines divide these values by the length conversion factor, exactly like `Hcurb` and `W`, so they are read in feet or metres:

```c
// src/legacy/engine/street.c, street_readParams()
street->curbHeight = x[2] / UCF(LENGTH);
...
street->gutterDepression = x[5] / UCF(LENGTH);
// src/legacy/engine/inlet.c, inlet_readUsageParams()
inlet->localDepress = aLocal / UCF(LENGTH);
```

The code comments say "(ft or m)" too, and EPA's verification decks for HEC-22 Example 4-9 enter the 1-inch (25-mm) depression as `0.0833` in `street_curb_inlet_9a-CFS.inp` and `0.025` in its SI twin `street_curb_inlet_9a.inp`. So the engines and EPA's decks agree, and the manual is wrong.

A user who follows the manual and types `1` for a 1-inch depression gets a gutter 1 ft deep. The street section then holds the flow in a narrow, deep gutter channel, and every inlet in it captures far more than it should. The 6.0.0 example `examples/inlets/street_inlet_junction.inp` shows that this happens in practice: its street `ST_MAIN` has `a = 2.0` and `W = 2.0` under a 0.5-ft curb, a gutter sloping at 45 degrees.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-25_hec22-example-4-9.inp`](IO-25_hec22-example-4-9.inp) | HEC-22 Example 4-9 (9.84-ft curb opening, Sx = 0.02, SL = 0.01, n = 0.016, 2-ft gutter depressed 1 inch, 1.77 cfs) on two streets: A enters the depression as `0.0833`, B as `1` |
| [`IO-25_test.c`](IO-25_test.c) | Legacy toolkit (5.2.4, 5.3.0). Measures each inlet's capture at steady state and checks street A against HEC-22's published answer, 1.55 cfs |
| [`IO-25_test6.c`](IO-25_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh IO-25            # 5.2.4, 5.3.0 and 6.0.0: PASS
```

The test passes in every engine, which is the evidence that the engines read feet. All three print:

```
HEC-22 Example 4-9, depressed gutter: 1.55 cfs of 1.77 cfs captured
Street  a entered       Q (cfs)  captured (cfs)  capture %
A       0.0833 (ft)       1.770           1.545      87.27
B       1      (in?)      1.770           1.770      99.99
PASS: the engine reads the gutter depression in feet (a = 0.0833 gives HEC-22's 1.545 cfs); a = 1 is read as a 1-ft gutter (1.770 cfs captured)
IO-25 5.2.4 base: PASS
IO-25 5.3.0 base: PASS
IO-25 6.0.0 base: PASS
```

Street B is what a user following the manual gets: the inlet captures all of the flow.

The 6.0.0 example deck, run with the 6.0.0 command-line program as shipped (`a = 2.0`) and with the 2 inches it was evidently meant to have (`a = 0.1667`):

| `examples/inlets/street_inlet_junction.inp` | a = 2.0 (as shipped) | a = 0.1667 |
|---|---|---|
| `ST_A` maximum spread / maximum depth (ft) | 1.452 / 1.511 | 11.416 / 0.623 |
| `Combo1` peak flow capture (%) | 99.00 | 81.48 |
| `ST_B` peak flow (cfs) | 0.131 | 2.153 |
| `ST_C` peak flow (cfs) | 0.011 | 0.982 |

The example's README asks the reader to compare `ST_A`'s peak spread with the 20-ft `Tcrown`; as shipped it is 1.45 ft. The application manual (`docs/manuals/application/sections/Chapter5-StreetInlets.md`) and its figure deck (`docs/figures/decks/street_inlets/street_inlet_junction.inp`) quote and plot the `a = 2.0` results ("street flow falls from 11.9 cfs to 0.13 cfs to 0.012 cfs").

## The fix

The engines are consistent with each other and with EPA's decks, so the manual should change, not the code. [`IO-25_docs.patch`](IO-25_docs.patch) corrects the three lines of the input reference:

```diff
-a     gutter depression height (in or mm) (default = 0)
+a     gutter depression height (ft or m) (default = 0)
 ...
-aLocal     height of local gutter depression (in or mm).
+aLocal     height of local gutter depression (ft or m).
 ...
-aLocal    height of a local gutter depression (in or mm).
+aLocal    height of a local gutter depression (ft or m).
```

It touches only `docs/`, which the review harness does not build, so it is named `_docs.patch` rather than `_swmm600.patch` and `tools/run-test.sh --patched` does not apply it.

Not included, for the maintainers: set `a = 0.1667` in `examples/inlets/street_inlet_junction.inp` and its copy under `docs/figures/decks/street_inlets/`, then regenerate the application manual's Chapter 5 numbers and figures; and consider a warning when a gutter depression is not smaller than the curb height, which catches a value entered in inches or millimetres in almost every real street.
