# NUM-13: In SI units, storage seepage uses a bottom area 10.8 times too small for FUNCTIONAL and geometric shapes

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | In SI models, a FUNCTIONAL, CYLINDRICAL, CONICAL or PYRAMIDAL storage unit with Green-Ampt seepage uses its bottom area in m² as if it were ft², so only 1/10.76 of the bottom seeps as bottom and the rest is treated as bank, which sees half the head. In the test a 100 m² vertical-walled basin loses 24.989 m³ in 6 hours instead of 31.544 m³ (21% too little); the same basin written as TABULAR, or in US units, gives 31.544 m³. No warning. |
| **Reached from** | `[STORAGE]` units with shape FUNCTIONAL, CYLINDRICAL, CONICAL or PYRAMIDAL and seepage parameters Psi, Ksat, IMD with IMD > 0, in a model with CMS, LPS or MLD flow units. With IMD = 0 (constant seepage rate) the bottom and bank errors cancel. |
| **5.3.0** | `exfil_initState()` in [`src/legacy/engine/exfil.c:135`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/exfil.c#L135) and [`:147`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/exfil.c#L147); the TABULAR case converts at [`:126`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/exfil.c#L126) |
| **5.2.4** | Same code, [`src/solver/exfil.c:135`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/exfil.c#L135) and [`:147`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/exfil.c#L147) |
| **6.0.0** | Reproduces in `ExfilSolver::init()`, [`src/engine/hydraulics/Exfiltration.cpp:200`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Exfiltration.cpp#L200), on purpose: the comment at [`:174`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Exfiltration.cpp#L174) calls it a parity quirk |
| **Since** | 5.1.007 for FUNCTIONAL units, when storage seepage was added (the TABULAR case got its unit conversion by 5.1.013, the FUNCTIONAL case never did); CYLINDRICAL, CONICAL and PYRAMIDAL copied it in 5.2.0 |
| **Fix** | Divide the bottom area by UCF(LENGTH)², as the TABULAR case does: [`NUM-13_swmm530.patch`](NUM-13_swmm530.patch), [`NUM-13_swmm600.patch`](NUM-13_swmm600.patch) |

## The problem

Storage seepage is split into flow through the bottom and flow through the banks. The bottom uses the bottom area and the full water depth as head; the banks use the surface area minus the bottom area and half the depth (`exfil_getLoss()`). For a vertical-walled unit the surface area equals the bottom area, so there is no bank and everything seeps through the bottom.

The storage shape coefficients A0, A1, A2 (FUNCTIONAL) and the derived ones of CYLINDRICAL, CONICAL and PYRAMIDAL units are kept in user units: ft² in US models, m² in SI models. `exfil_initState()` takes the bottom area straight from them, while the surface area passed to `exfil_getLoss()` is in ft². In an SI model a 100 m² bottom therefore counts as 100 ft² = 9.3 m², and 976 ft² of the 1076 ft² surface becomes bank.

The test basin, 100 m² with 2 m of water over soil with Psi 100 mm, Ksat 10 mm/hr and IMD 0.3, written three ways in each unit system:

| Shape | Seepage in 6 h, SI | Seepage in 6 h, US |
|---|---|---|
| FUNCTIONAL (A0 = 100 m²) | 24.989 m³ | 31.544 m³ |
| TABULAR (constant 100 m²) | 31.544 m³ | 31.544 m³ |
| CYLINDRICAL (11.2838 m diameter) | 24.989 m³ | 31.544 m³ |

The Storage Volume Summary of the SI run shows an exfiltration loss of 12.0% for SU1 and 15.1% for SU2, two descriptions of the same basin.

## Why it happens

```c
// src/legacy/engine/exfil.c, exfil_initState()
            case TABULAR:
                ...
                // --- convert from user units to internal units
                exfil->btmArea /= UCF(LENGTH) * UCF(LENGTH);
                exfil->bankMaxArea /= UCF(LENGTH) * UCF(LENGTH);
                ...
            // --- functional storage shape curve
            case FUNCTIONAL:
                exfil->btmArea = Storage[k].a0;          // m2 under SI
                if ( Storage[k].a2 == 0.0 )
                    exfil->btmArea +=Storage[k].a1;
                ...
            case CYLINDRICAL:
            case CONICAL:
            case PYRAMIDAL:
                exfil->btmArea = Storage[k].a0;          // m2 under SI
```

```c
// src/legacy/engine/exfil.c, exfil_getLoss()   (area is in ft2)
    exfilRate *= exfil->btmArea;
    ...
        area = MIN(area, exfil->bankMaxArea) - exfil->btmArea;
```

`storage_getSurfArea()` (node.c) shows the coefficients are user units: it evaluates `a0 + a1*d^a2` and divides by `UCF(LENGTH)²`. With a constant seepage rate (IMD = 0) the bottom and bank terms add up to Ksat × surface area whatever the split, so only Green-Ampt seepage gives a wrong total.

6.0.0 knowingly copies the legacy behaviour:

```cpp
// src/engine/hydraulics/Exfiltration.cpp, ExfilSolver::init()
            //     PARITY QUIRK: legacy does NOT unit-convert the FUNCTIONAL
            //     bottom area (its /UCF at exfil.c:126-129 is inside the
            //     TABULAR case only), ...
            soa_.btm_area[uk] = btm;
```

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-13_si.inp`](NUM-13_si.inp) | CMS: the same 100 m² vertical-walled basin as SU1 (FUNCTIONAL 0 0 100), SU2 (TABULAR, constant 100 m²) and SU3 (CYLINDRICAL 11.2838 × 11.2838 m), 2 m deep, seepage Psi 100 mm, Ksat 10 mm/hr, IMD 0.3, no inflow, outlets above the water, 6 hours |
| [`NUM-13_us.inp`](NUM-13_us.inp) | The same three units in CFS (1076.39 ft², 6.562 ft, Psi 3.937 in, Ksat 0.3937 in/hr) |
| [`NUM-13_test.c`](NUM-13_test.c) | Runs both decks through the legacy toolkit and takes each unit's seepage as the drop in its stored volume; all six must agree within 1% |
| [`NUM-13_test6.c`](NUM-13_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-13            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-13 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, all three engines print:

```
Seepage loss over 6 h (m3)
Shape          SI (CMS)   US (CFS)
FUNCTIONAL       24.989     31.544
TABULAR          31.544     31.544
CYLINDRICAL      24.989     31.544
FAIL: the same basin loses between 24.989 and 31.544 m3 (20.8% apart) depending on shape and units
NUM-13 5.3.0 base: FAIL
```

**With the fix**, 5.3.0 and 6.0.0 print:

```
Seepage loss over 6 h (m3)
Shape          SI (CMS)   US (CFS)
FUNCTIONAL       31.544     31.544
TABULAR          31.544     31.544
CYLINDRICAL      31.544     31.544
PASS: the same basin loses the same volume (31.544 m3) for every shape and unit system
NUM-13 5.3.0 patched: PASS
NUM-13 6.0.0 patched: PASS
```

## The fix

```diff
             case FUNCTIONAL:
                 exfil->btmArea = Storage[k].a0;
                 if ( Storage[k].a2 == 0.0 )
                     exfil->btmArea +=Storage[k].a1;
+                exfil->btmArea /= UCF(LENGTH) * UCF(LENGTH);
 ...
             case PYRAMIDAL:
-                exfil->btmArea = Storage[k].a0;
+                exfil->btmArea = Storage[k].a0 / (UCF(LENGTH) * UCF(LENGTH));
```

The 6.0.0 patch divides `btm` the same way in `ExfilSolver::init()` and replaces the "PARITY QUIRK" comment. US models are unchanged (the divisor is exactly 1). SI models with Green-Ampt seepage from these shapes seep more; with a constant seepage rate (IMD = 0) only the last bits of the result can change.

Effect on other models: the only regression decks with seepage from a non-tabular storage unit (`swc11.inp`, `swc17.inp`) are in CFS, so they are unaffected.
