# NUM-16: Closing a V-notch weir by 0.1% removes almost all of its flow

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A V-notch weir whose setting is below 1 (from a control rule or the API) loses the flow through its notch. At setting 0.999 a weir that passed 2 cfs at 0.91 ft of head passes 0.08 cfs with the storage full, and the storage floods 0.547 million gallons in 12 hours. Flow is not monotonic in the setting: 0.999 floods, 0.95 holds 4.27 ft, 0.5 holds 2.54 ft. No warning. |
| **Reached from** | `[WEIRS]` of type `V-NOTCH` with a `[CONTROLS]` rule or toolkit call that sets the weir's setting to a value between 0 and 1, when Cd2 is left at its default (0) |
| **5.3.0** | `weir_getFlow()` in [`src/legacy/engine/link.c:2401`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L2401) and [`:2438`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L2438) |
| **5.2.4** | Same code, [`src/solver/link.c:2360`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L2360) and [`:2397`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L2397) |
| **6.0.0** | Reproduces with the same numbers in `StructureSolver::computeWeirFlowK()`, [`src/engine/hydraulics/HydStructures.cpp:937`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/HydStructures.cpp#L937) and [`:964`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/HydStructures.cpp#L964) |
| **Since** | 5.0.011 (the line carries the comment `//(5.0.011 - LR)` in the oldest version in the repository) |
| **Fix** | Use the V-notch coefficient for the notch part of a partly open V-notch weir: [`NUM-16_swmm530.patch`](NUM-16_swmm530.patch), [`NUM-16_swmm600.patch`](NUM-16_swmm600.patch) |

## The problem

A weir's setting raises its crest: with setting s, the crest sits (1 − s)·Height above its normal position. For a V-notch weir the remaining opening is a trapezoid, a rectangular strip of width W((1 − s)·Height) in the middle with the original notch on either side. Whatever s is, that opening contains a complete V-notch above the raised crest, so the weir passes at least the V-notch flow Cd·slope·h<sup>2.5</sup> at head h.

SWMM computes the two sides of the trapezoid with Cd2, the coefficient for the triangular ends of a TRAPEZOIDAL weir. The input manual defines Cd2 only for TRAPEZOIDAL weirs, and the parser sets it to 0 when it is not given. So at any setting below 1 the notch passes nothing and only the rectangular strip is left, which is a few thousandths of a foot wide near s = 1.

The test deck has seven identical storage units (1000 ft²), each receiving 2 cfs and draining over a 90° V-notch weir (TRIANGULAR, 2 ft high, 4 ft wide, Cd 2.5, crest 1 ft) with its setting fixed by a rule:

| Setting | Steady depth, 5.2.4 / 5.3.0 / 6.0.0 | Weir flow | With the fix |
|---|---|---|---|
| 1.000 | 1.915 ft | 2.000 cfs | 1.915 ft |
| 0.999 | 10.000 ft (full, floods 0.547 MG) | 0.080 cfs | 1.915 ft |
| 0.990 | 10.000 ft (full, floods 0.332 MG) | 0.792 cfs | 1.919 ft |
| 0.950 | 4.266 ft | 2.000 cfs | 1.940 ft |
| 0.900 | 2.787 ft | 2.000 cfs | 1.974 ft |
| 0.750 | 2.362 ft | 2.000 cfs | 2.124 ft |
| 0.500 | 2.543 ft | 2.000 cfs | 2.471 ft |

Closing the weir by 1% is far more restrictive than closing it by half. A control rule that throttles a V-notch weir by a small amount does the opposite of what the modeller intends. `weir_setSetting()` derives the surcharge (orifice) coefficient from the same flow, so it collapses too: the 0.95 unit rises above the weir's 3 ft crown and drains through that reduced orifice.

## Why it happens

```c
// src/legacy/engine/link.c, weir_getFlow()
    // --- use appropriate formula for weir flow
    wType = Weir[k].type;
    if ( wType == VNOTCH_WEIR &&
         Link[j].setting < 1.0 ) wType = TRAPEZOIDAL_WEIR;
    switch (wType)
    {
      ...
      case VNOTCH_WEIR:
        *q1 = cDisch1 * Weir[k].slope * pow(h, 2.5);
        break;

      case TRAPEZOIDAL_WEIR:
        y = (1.0 - Link[j].setting) * Link[j].xsect.yFull;
        length = xsect_getWofY(&Link[j].xsect, y) * UCF(LENGTH);
        *q1 = cDisch1 * length * pow(h, 1.5);
        *q2 = Weir[k].cDisch2 * Weir[k].slope * pow(h, 2.5);
    }
```

For a fully open V-notch the notch flow is `q1` with `cDisch1`. Once the setting drops below 1 the same flow becomes `q2`, with `cDisch2` in place of `cDisch1`. `weir_readParams()` initialises `x[5]` (Cd2) to 0, and Cd2 is the ninth field of a `[WEIRS]` line, which most V-notch weirs leave blank. (The manual says Cd2 defaults to Cd; the code does not do that for any weir type.)

6.0.0 ports the same branch (`if (wType == 2 && setting < 1.0) wType = 3;` and `q2_out = cd2 * slope * h^2.5`).

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-16_vnotch-settings.inp`](NUM-16_vnotch-settings.inp) | Seven storage units S1–S7 (1000 ft², 2 cfs inflow each), each draining over a V-notch weir W1–W7 (90°, 2 ft × 4 ft, Cd 2.5, crest 1 ft, Cd2 not given) into a free outfall. Rules set the weirs to 1, 0.999, 0.99, 0.95, 0.9, 0.75 and 0.5. Dynamic wave, 12 hours. |
| [`NUM-16_test.c`](NUM-16_test.c) | Runs the deck through the legacy toolkit and checks each unit's final depth against the V-notch bound 1 + (1 − s)·2 + (2/2.5)<sup>0.4</sup> ft, and that a wider-open weir never holds a higher level |
| [`NUM-16_test6.c`](NUM-16_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-16            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-16 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, all three engines print the same table:

```
Setting  Final depth (ft)  Bound (ft)  Weir flow (cfs)
  1.000             1.915       1.915            2.000
  0.999            10.000       1.917            0.080   <-- above bound
  0.990            10.000       1.935            0.792   <-- above bound
  0.950             4.266       2.015            2.000   <-- above bound
  0.900             2.787       2.115            2.000   <-- above bound
  0.750             2.362       2.415            2.000
  0.500             2.543       2.915            2.000
FAIL: 4 of 7 settings hold the storage above the V-notch bound; 3 times a wider-open weir gives a higher level
NUM-16 5.3.0 base: FAIL
```

**With the fix**, 5.3.0 and 6.0.0 print identical numbers:

```
Setting  Final depth (ft)  Bound (ft)  Weir flow (cfs)
  1.000             1.915       1.915            2.000
  0.999             1.915       1.917            2.000
  0.990             1.919       1.935            2.000
  0.950             1.940       2.015            2.000
  0.900             1.974       2.115            2.000
  0.750             2.124       2.415            2.000
  0.500             2.471       2.915            2.000
PASS: a partly open V-notch weir still passes the V-notch flow and the level falls as the weir opens
NUM-16 5.3.0 patched: PASS
NUM-16 6.0.0 patched: PASS
```

## The fix

The notch part of a partly open V-notch weir uses the V-notch coefficient (Cd, or the value from its Cd curve):

```diff
         *q1 = cDisch1 * length * pow(h, 1.5);
-        *q2 = Weir[k].cDisch2 * Weir[k].slope * pow(h, 2.5);
+
+        // --- a partly open V-notch keeps its own coeff. for the notch
+        *q2 = (Weir[k].type == VNOTCH_WEIR ? cDisch1 : Weir[k].cDisch2) *
+              Weir[k].slope * pow(h, 2.5);
```

Flow is now continuous at s = 1 (the strip width goes to 0 and `q2` is the V-notch formula) and decreases steadily as the weir closes. `weir_getdqdh()` already treats a V-notch with `q2 > 0` as partly open (1.5·q1/h + 2.5·q2/h), and `weir_setSetting()` picks up the corrected flow for the surcharge coefficient. The 6.0.0 patch makes the same change in `computeWeirFlowK()`, which also feeds its surcharge coefficient.

The rectangular strip keeps using the V-notch Cd, as before. A rectangular-weir coefficient would be more accurate for it, but SWMM has no input for one on a V-notch weir; that is a modelling refinement, not part of this defect.

Fully open V-notch weirs, TRAPEZOIDAL weirs and all other weir types are unchanged. None of the 73 regression decks has a V-notch weir, so none changes.
