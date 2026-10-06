# CON-18: The Preissmann slot area shrinks as the head rises and does not match the slot width

| | |
|---|---|
| **Category** | [Conceptual and formulation issues](../README.md) |
| **Impact** | Under `SURCHARGE_METHOD SLOT`, the water SWMM books in a surcharged conduit falls while the head rises from 1.29 to 1.78 diameters above the invert, and above that it holds much less than node continuity stored. In the test, a 3 ft pipe filled with 756 ft³ loses 151 ft³ of booked volume while its head rises, 458 ft³ (61%) of the inflow never appears in storage, and the routing continuity error is 5.77% in a model where nothing leaves. With the fix the booked storage matches the inflow to 0.1 ft³ and the error is 0.000%. Heads and flows do not change. No warning. |
| **Reached from** | `SURCHARGE_METHOD SLOT` with dynamic wave, any closed conduit surcharged above its crown |
| **5.3.0** | `getArea()` in [`src/legacy/engine/dwflow.c:697`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dwflow.c#L697), with the slot width from `getSlotWidth()` at [`dwflow.c:655`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dwflow.c#L655) |
| **5.2.4** | Same code, [`src/solver/dwflow.c:617`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dwflow.c#L617). In 5.2.4 the slot area also enters the momentum equation ([NUM-54](../../1-numerical/NUM-54-slot-velocity-uses-slot-area/)) |
| **6.0.0** | Reproduces with the same numbers: `DWSolver::momentumKernels()` in [`src/engine/hydraulics/DynamicWave.cpp:2435`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/DynamicWave.cpp#L2435) (and lines 2440, 2445) |
| **Since** | 5.1.013, when the Preissmann slot option was added |
| **Fix** | Integrate the slot width to get the slot area: [`CON-18_swmm530.patch`](CON-18_swmm530.patch), [`CON-18_swmm600.patch`](CON-18_swmm600.patch) |

## The problem

The Preissmann slot gives a surcharged closed conduit a narrow fictitious top width, so that water can be stored above the crown and the dynamic wave equations keep working. SWMM uses the Sjoberg width, `w = 0.5423·wMax·exp(-(y/yFull)^2.4)`, which narrows from 0.20·wMax at the crown to 0.01·wMax at 1.78·yFull and stays at 0.01·wMax above.

Two parts of the solver use this slot. Node continuity takes the slot width at each end and at mid-length as the conduit's contribution to the nodes' surface area, so the water stored for a rise `dh` is `w·L·dh`. The conduit's own volume, which the mass balance and the report book, is `getArea()·L`. For those to describe the same slot, the area must be the integral of the width. `getArea()` instead returns a rectangle: the full area plus the surcharge height times the width at the current depth,

```
A(y) = aFull + (y - yFull)·w(y)
```

Because `w` falls quickly, this rectangle falls with depth over part of the range: `dA/dy = w·[1 - 2.4·(η - 1)·η^1.4]`, which is negative for 1.29 < η < 1.78 (η = y/yFull). Below η = 1.78 it is always smaller than the integral of the width, so water that node continuity has stored is missing from the conduit's booked volume.

In the test, a horizontal 3 ft pipe, 1000 ft long, starts exactly full between two 20 ft² storage units, and 0.035 cfs enters for six hours. Nothing can leave, so all 756 ft³ must be stored.

| Time | Head / D | Conduit volume, unpatched | Inflow so far | Missing from storage, unpatched | Conduit volume, with the fix | Missing, with the fix |
|---|---|---|---|---|---|---|
| 3:00 | 1.27 | 7292.1 ft³ | 378.0 ft³ | 122.0 ft³ | 7414.0 ft³ | 0.1 ft³ |
| 4:00 | 1.44 | 7263.0 ft³ | 504.0 ft³ | 256.6 ft³ | 7519.5 ft³ | 0.1 ft³ |
| 5:00 | 1.82 | 7142.1 ft³ | 630.0 ft³ | 458.4 ft³ | 7600.4 ft³ | 0.1 ft³ |
| 6:00 | 2.42 | 7196.1 ft³ | 756.0 ft³ | 458.4 ft³ | 7654.4 ft³ | 0.1 ft³ |

The conduit volume peaks near η = 1.29 and then falls by 151 ft³ while the head keeps rising. By η = 1.78, 458 ft³ of the 630 ft³ that entered is missing; above that the rectangle's slope equals the width again, so the gap stays constant. The routing continuity error of this run, in which nothing leaves the system, is 5.77%.

The heads are right: node continuity uses the slot width and is not affected. What is wrong is everything that reads the conduit's volume: the Final Stored Volume and continuity error in the report, the conduit volume in the output file, and pollutant routing, which mixes a conduit's load into that volume. A user who switches from EXTRAN to SLOT to cure a continuity problem in a surcharged system can get a larger one, from the slot itself.

## Why it happens

```c
// src/legacy/engine/dwflow.c
double getSlotWidth(TXsect* xsect, double y)
{
    double yNorm = y / xsect->yFull;
    ...
    // --- for depth > 1.78 * pipe depth, slot width = 1% of max. width
    if (yNorm > 1.78) return 0.01 * xsect->wMax;

    // --- otherwise use the Sjoberg formula
    return xsect->wMax * 0.5423 * exp(-pow(yNorm, 2.4));
}

double getArea(TXsect* xsect, double y, double wSlot)
{
    if ( y >= xsect->yFull ) return xsect->aFull + (y - xsect->yFull) * wSlot;
    return xsect_getAofY(xsect, y);
}
```

`dwflow_findConduitFlow()` calls `getArea()` with `wSlot = getSlotWidth(xsect, y)` for both ends and the midpoint, and books `Link[].newVolume = aMid·length`. `findSurfArea()` gives the end nodes `(width1 + widthMid)·length/4` and `(widthMid + width2)·length/4`, with the widths from `getWidth()`, which returns the slot width under SLOT.

6.0.0 computes `area1_`, `area2_` and `area_mid_` with the same rectangle in `momentumKernels()`.

## How to reproduce

| File | What it is |
|---|---|
| [`CON-18_closed-pipe.inp`](CON-18_closed-pipe.inp) | 3 ft pipe, 1000 ft, horizontal, full at the start, between storage units J1 and J2 (20 ft² each); 0.035 cfs into J1 for 6 hours; the only outlet is a weir 15 ft above J2's invert, never reached; `SURCHARGE_METHOD SLOT` |
| [`CON-18_test.c`](CON-18_test.c) | Legacy toolkit (5.2.4, 5.3.0): runs the deck, reads the conduit volume and the storage heads at each 5-minute report period from the output file, and checks that the conduit volume never falls (1 ft³ tolerance) and that conduit + storage volume equals the initial volume plus the inflow (within 2% of the 756 ft³ inflow) |
| [`CON-18_test6.c`](CON-18_test6.c) | The same through the 6.0.0 C API, at every routing step |

```sh
tools/run-test.sh CON-18            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh CON-18 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 5.2.4 prints the same; 6.0.0 the same apart from sampling every routing step, which finds a fall of 154.0 ft³ and a continuity error of 5.770%):

```
Time   head / D   conduit volume   inflow so far   stored - initial   missing
                      (ft3)            (ft3)            (ft3)          (ft3)
0:30    1.034        7124.9            63.0              60.5           2.5
...
3:00    1.271        7292.1           378.0             256.0         122.0
3:30    1.345        7288.2           441.0             261.1         179.9
4:00    1.442        7263.0           504.0             247.4         256.6
4:30    1.580        7209.7           567.0             210.8         356.2
5:00    1.817        7142.1           630.0             171.6         458.4
5:30    2.117        7169.1           693.0             234.6         458.4
6:00    2.417        7196.1           756.0             297.6         458.4
Largest fall of the conduit volume below an earlier value: 150.7 ft3 (reached at 1.817 D)
Largest volume missing from storage: 458.4 ft3 (60.6% of the 6-hour inflow)
SWMM routing continuity error: 5.769%
FAIL: the conduit volume falls by 150.7 ft3 while the head rises, and up to 458.4 ft3 of the inflow is missing from storage (continuity error 5.769%)
CON-18 5.3.0 base: FAIL
```

**With the fix** (5.3.0; 6.0.0 prints the same values, continuity error 0.001%):

```
3:00    1.271        7414.0           378.0             377.9           0.1
...
6:00    2.417        7654.4           756.0             755.9           0.1
Largest fall of the conduit volume below an earlier value: 0.0 ft3 (reached at 1.005 D)
Largest volume missing from storage: 0.1 ft3 (0.0% of the 6-hour inflow)
SWMM routing continuity error: 0.000%
PASS: the conduit volume rises with the head and storage accounts for the inflow (continuity error 0.000%)
CON-18 5.3.0 patched: PASS
```

The heads are identical with and without the fix (J1 and J2 reach 7.25 ft in both).

## The fix

Make the slot area the integral of the slot width, from the crown to the current depth:

```diff
     if ( y >= xsect->yFull )
-        return xsect->aFull + (y - xsect->yFull) * wSlot;
+        return xsect->aFull + (wSlot > 0.0 ? getSlotArea(xsect, y) : 0.0);
```

with a new `getSlotArea()` that integrates the Sjoberg formula by Simpson's rule (8 intervals, between the crown and `min(y, 1.78·yFull)`) and adds `0.01·wMax` per foot above 1.78·yFull. The Sjoberg integral has no closed form; 8 intervals are accurate to better than 0.01% of the slot area, and the result grows monotonically with depth. The 6.0.0 patch adds the same function as `slotArea()` in `DynamicWave.cpp` and uses it for the three areas.

Nothing changes under EXTRAN or below the crown. Under SLOT, 5.3.0 and 6.0.0 already take the momentum terms from the conveyance area (`MIN(a, aFull)`, issue #144), so the flows do not change directly; what changes is the conduit volume, and through `Conduit[].a1` the variable time step.

Effect on other models: none of the 73 decks of the SWMM regression test suite uses `SURCHARGE_METHOD SLOT`. With SLOT switched on, extran1, extran8a, routing/test2 and user1 give identical reports with and without the patch (their conduits do not surcharge above the crown), and Example7-Final changes only in the last printed digit: routing continuity -0.226% to -0.227%, minimum time step 0.54 to 0.55 s, and a few depths, peak flows and flow-balance errors by one unit (0.01 ft, 0.01 cfs, 0.001%).

Not changed: between `CrownCutoff` (0.985·yFull) and the crown, `getWidth()` already gives node continuity the slot width (about 0.2·wMax) while `getArea()` still follows the circular section. For a circular pipe the two differ by about 0.0005·D² per unit length over that 1.5% of the depth. The test starts at the crown and does not cross that band.
