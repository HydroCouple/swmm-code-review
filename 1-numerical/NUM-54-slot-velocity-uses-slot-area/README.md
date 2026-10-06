# NUM-54: With the Preissmann slot, 5.2.4's friction term uses the slot area for the velocity

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | 5.2.4 only. Under `SURCHARGE_METHOD SLOT`, a surcharged conduit carries too much flow for its head loss, by about the fraction the slot adds to the flow area: +5.1% for a pipe whose mid-length head is five diameters above its invert in the test (60.75 cfs instead of 57.80 cfs), up to 3% below two diameters, 13% at eleven and 39% at about thirty. Force mains, which run full, are affected for as long as they are full. EXTRAN is not affected. No warning. |
| **Reached from** | `SURCHARGE_METHOD SLOT` with dynamic wave, any closed conduit whose mid-length depth is above its crown |
| **5.3.0** | Not affected: fixed in the vendored 5.3.0 by OpenSWMM issue #144 (fork commit [323c9e4c](https://github.com/HydroCouple/Stormwater-Management-Model/commit/323c9e4c8bed14eb265a409de91fb9bf4439aff4)), which divides by the conveyance area, [`src/legacy/engine/dwflow.c:207`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dwflow.c#L207) and `getConveyArea()` at [`dwflow.c:703`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/dwflow.c#L703). Whether EPA's own 5.3 has it was not checked. |
| **5.2.4** | `dwflow_findConduitFlow()` in [`src/solver/dwflow.c:183`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dwflow.c#L183), with `getArea()` at [`dwflow.c:609`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/dwflow.c#L609) |
| **6.0.0** | Not affected: `DWSolver` uses `conveyArea()` for the momentum velocity, [`src/engine/hydraulics/DynamicWave.cpp:2755`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/DynamicWave.cpp#L2755) |
| **Since** | 5.1.013, when the Preissmann slot option was added |
| **Fix** | None needed here; 5.3.0 and 6.0.0 already have it |

## The problem

The Preissmann slot is a narrow fictitious gap above the crown of a closed conduit. It gives a full pipe a finite top width so that the dynamic wave equations still work once the pipe pressurizes. It stores water; it carries none.

In 5.2.4, the momentum equation divides the flow by the slot-inclusive area to get the velocity that goes into the friction term, but uses the full-pipe hydraulic radius, which excludes the slot. Since the friction term is linear in that velocity, friction is underestimated by the slot's share of the area, and a surcharged pipe carries more flow for a given head difference than the Manning equation allows.

The test pipe is horizontal, 3 ft in diameter, n = 0.015 and 1000 ft long, and runs full from a very large storage unit at 20 ft to an outfall with a fixed stage of 10 ft. Its mid-length head is 15 ft, five diameters (η = 5); at that depth `getArea()` adds 5.1% to the full area. At steady state all head loss is friction, so the flow must be the Manning full-pipe flow for a friction slope of 10/1000, 57.80 cfs, whatever surcharge method is used.

| | SLOT | EXTRAN |
|---|---|---|
| 5.2.4 | 60.748 cfs (+5.09%) | 57.804 cfs |
| 5.3.0 (as vendored), 6.0.0 | 57.804 cfs | 57.804 cfs |

The error is the slot's share of the area, `(η - 1)·D·w/A_full` with the slot width `w` from `getSlotWidth()`. For a circular pipe that is 1.12% at η = 1.05, 3.17% at η = 1.29, 0.99% at η = 1.78 (where the Sjoberg width is replaced by a fixed 1% of the pipe width), 5.09% at η = 5 and 12.73% at η = 11; it is not monotonic in depth (see [CON-18](../../2-conceptual/CON-18-slot-area-not-monotonic/)).

## Why it happens

```c
// src/solver/dwflow.c (5.2.4), dwflow_findConduitFlow()
    wSlot = getSlotWidth(xsect, yMid);
    aMid = getArea(xsect, yMid, wSlot);       // includes the slot above the crown
    ...
    // --- compute velocity from last flow estimate
    v = qLast / aMid;
    ...
    else dq1 = dt * Conduit[k].roughFactor / pow(rWtd, 1.33333) * fabs(v);
```

```c
// src/solver/dwflow.c (5.2.4)
double getArea(TXsect* xsect, double y, double wSlot)
{
    if ( y >= xsect->yFull ) return xsect->aFull + (y - xsect->yFull) * wSlot;
    return xsect_getAofY(xsect, y);
}

double getHydRad(TXsect* xsect, double y)
{
    if ( y >= xsect->yFull ) return xsect->rFull;   // slot excluded
    return xsect_getRofY(xsect, y);
}
```

In a full pipe at steady state the flow update reduces to `dq1·Q = -dq2` (the inertial terms are switched off for a surcharged closed conduit), i.e. `g·(n/1.486)²·Q·|Q| / (R^(4/3)·aMid) = g·aWtd·Δh/L`. A full pipe uses `aWtd = aMid`, so `Q = aMid·(1.486/n)·R^(2/3)·√(Δh/L)`: the Manning full-pipe flow scaled by `aMid/aFull`, which is 1.0509 at η = 5, the +5.09% the test measures.

The vendored 5.3.0 and 6.0.0 introduce a conveyance area, `MIN(a, aFull)` for closed conduits, for the velocity, the upstream-weighted area, the inertial terms and the local losses, and keep the slot-inclusive area for node continuity and link volume.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-54_slot.inp`](NUM-54_slot.inp) | 3 ft pipe, n = 0.015, 1000 ft, horizontal, from a 10^9 ft² storage unit starting at 20 ft to a FIXED 10 ft outfall; `SURCHARGE_METHOD SLOT`; 2 hours |
| [`NUM-54_extran.inp`](NUM-54_extran.inp) | The same with `SURCHARGE_METHOD EXTRAN` |
| [`NUM-54_test.c`](NUM-54_test.c) | Legacy toolkit (5.2.4, 5.3.0): runs both decks and compares the final pipe flow with the Manning full-pipe flow computed from the final heads at both ends, tolerance 1% |
| [`NUM-54_test6.c`](NUM-54_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-54   # 5.2.4: FAIL; 5.3.0, 6.0.0: PASS
```

**5.2.4:**

```
Surcharge   head T1   head O1   Manning full-pipe Q   SWMM Q    error
method        (ft)      (ft)         (cfs)            (cfs)
SLOT        20.000    10.000         57.804           60.748    +5.09%
EXTRAN      20.000    10.000         57.804           57.804    +0.00%
FAIL: a full pipe under a fixed head difference carries 60.748 cfs with SURCHARGE_METHOD SLOT, +5.09% off the Manning full-pipe flow 57.804 cfs (EXTRAN +0.00%)
NUM-54 5.2.4 base: FAIL
```

**5.3.0 and 6.0.0** (both print the same):

```
SLOT        20.000    10.000         57.804           57.804    +0.00%
EXTRAN      20.000    10.000         57.804           57.804    +0.00%
PASS: the surcharged pipe carries the Manning full-pipe flow with both SLOT and EXTRAN
NUM-54 5.3.0 base: PASS
```

## The fix

No patch: the defect is only in 5.2.4, which this review does not patch. The fix in the vendored 5.3.0 (commit 323c9e4c) adds `getConveyArea()`, which returns `MIN(a, aFull)` for closed conduits, and uses it in the momentum terms:

```c
    a1Conv   = getConveyArea(xsect, a1);
    a2Conv   = getConveyArea(xsect, a2);
    aMidConv = getConveyArea(xsect, aMid);
    aOldConv = getConveyArea(xsect, aOld);
    ...
    v = qLast / aMidConv;
```

Models run with `SURCHARGE_METHOD SLOT` in 5.2.4 (or EPA builds with the same code) will see less flow through surcharged pipes in 5.3.0 and 6.0.0. Anyone comparing calibrated SLOT models across versions should expect that difference.
