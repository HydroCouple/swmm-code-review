# NUM-07: A force main's full flow uses the Hazen-Williams radius exponent with Manning's equation

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Every quantity SWMM derives from a FORCE_MAIN's full section factor is too large by R^-0.037: +5.2% for a 1 ft main, +7.9% for 0.5 ft, nothing at 4 ft. Under dynamic wave this is the reported full flow (Cross Section Summary, Max/Full Flow, hours above full normal flow) and the normal depth used for initial depths, NORMAL outfalls and flow classification. Under kinematic wave and steady flow it is the pipe's whole conveyance: a 1 ft force main passes 1.185 cfs where the Manning equation with its n gives 1.127 cfs. No warning. |
| **Reached from** | Any conduit with a FORCE_MAIN cross-section, any routing method |
| **5.3.0** | `xsect_setParams()` in [`src/legacy/engine/xsect.c:261`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L261); used by `conduit_validate()` in [`src/legacy/engine/link.c:1138`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1138) |
| **5.2.4** | Same code, [`src/solver/xsect.c:257`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/xsect.c#L257) |
| **6.0.0** | Reproduces with the same numbers: `xsect::setParams()` in [`src/engine/hydraulics/XSection.cpp:268`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/XSection.cpp#L268) |
| **Since** | 5.0.010, when force mains were added |
| **Fix** | Use `A*R^(2/3)` and `sMax = 1.08*sFull`, as for CIRCULAR: [`NUM-07_swmm530.patch`](NUM-07_swmm530.patch), [`NUM-07_swmm600.patch`](NUM-07_swmm600.patch) |

## The problem

SWMM describes a conduit's conveyance by its section factor `A·R^(2/3)` and multiplies it by `β = 1.486·√S / n` (Manning) to get flow. The full flow `Qfull = β·sFull` is the Manning normal flow at full depth (Hydraulics Reference Manual, eq. 3-23).

A FORCE_MAIN is a circular pipe whose pressurized friction uses Hazen-Williams or Darcy-Weisbach under dynamic wave. For everything else the manual says the Manning equation is used: under dynamic wave with an equivalent n (eq. 7-35, `n = 1.067/C·(D/S)^0.04`) chosen so that the Manning full flow equals the Hazen-Williams full flow (eq. 7-34), and under kinematic wave or steady flow with the n from [CONDUITS].

`xsect_setParams()` instead gives a FORCE_MAIN the section factor `A·R^0.63`, the Hazen-Williams radius exponent, and `sMax = 1.06949·sFull`. Multiplied by the Manning β, this is neither the Manning nor the Hazen-Williams full flow. It is too large by `R^(0.63 - 2/3) = R^-0.037`, which is 1 at R = 1 ft (D = 4 ft) and grows as pipes get smaller, so the usual test case of a large main does not show it.

Full flow of Hazen-Williams force mains (C = 130, 1000 ft at 0.1%), dynamic wave:

| D | Manning full flow with eq. 7-35 n | Hazen-Williams full flow | SWMM | with the fix |
|---|---|---|---|---|
| 0.5 ft | 0.2192 cfs | 0.2177 cfs | 0.2365 cfs (+7.92%) | 0.2192 cfs |
| 1 ft | 1.3537 cfs | 1.3479 cfs | 1.4243 cfs (+5.21%) | 1.3537 cfs |
| 4 ft | 51.6328 cfs | 51.6498 cfs | 51.6328 cfs (0%) | 51.6328 cfs |

Under dynamic wave the pressurized flow itself is right, because full-pipe friction uses Hazen-Williams directly and does not involve `sFull`. What is wrong is what SWMM derives from the full flow: the Full Flow in the Cross Section Summary, the Max/Full Flow ratio (0.42 instead of 0.46 for the 0.5 ft main carrying 0.1 cfs in the test deck) and the hours above full normal flow, all of which understate how close a force main is to capacity; and the normal depth from `link_getYnorm()` (initial depths of conduits with an initial flow, NORMAL outfalls, the critical/normal depth test of the flow classification) and the velocity used to lengthen short conduits.

Under kinematic wave and steady flow `sFull` scales the whole flow-area relation, so a force main carries more than a circular pipe of the same size and n. In the test, 2 cfs is fed into a 1 ft FORCE_MAIN and a 1 ft CIRCULAR pipe with n = 0.013; the circular pipe passes its Manning full flow, 1.1267 cfs, and the force main 1.1854 cfs.

## Why it happens

```c
// src/legacy/engine/xsect.c, xsect_setParams()
    case FORCE_MAIN:
        xsect->yFull = p[0]/ucf;
        xsect->wMax  = xsect->yFull;
        xsect->aFull = PI / 4.0 * xsect->yFull * xsect->yFull;
        xsect->rFull = 0.2500 * xsect->yFull;
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 0.63);
        xsect->sMax  = 1.06949 * xsect->sFull;
```

```c
// src/legacy/engine/link.c, conduit_validate()
    roughness = Conduit[k].roughness;
    if ( RouteModel == DW && Link[j].xsect.type == FORCE_MAIN )
    {
        roughness = forcemain_getEquivN(j, k);
    }
    ...
    else Conduit[k].beta = PHI * sqrt(fabs(slope)) / roughness;
    Link[j].qFull = Link[j].xsect.sFull * Conduit[k].beta;
    Conduit[k].qMax = Link[j].xsect.sMax * Conduit[k].beta;
```

The partly-full section factor of a FORCE_MAIN is the circular table scaled by `sFull` (`circ_getSofA()`, `circ_getYofS()`), so normal depths and kinematic-wave flows inherit the same factor. The equivalent n from `forcemain_getEquivN()` already absorbs the Hazen-Williams exponent (eq. 7-34 equates `(1.486/n)·R^(2/3)·√S` with `1.318·C·R^0.63·S^0.54`), so using 0.63 again in `sFull` applies it twice.

6.0.0's `xsect::setParams()` has the same two lines.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-07_dw-hw.inp`](NUM-07_dw-hw.inp) | Dynamic wave, H-W: force mains of 1, 0.5 and 4 ft, C = 130, 1000 ft at 0.1%, 0.1 cfs each |
| [`NUM-07_kw.inp`](NUM-07_kw.inp) | Kinematic wave: 2 cfs into a 1 ft FORCE_MAIN and into a 1 ft CIRCULAR pipe, both n = 0.013, 1000 ft at 0.1% |
| [`NUM-07_test.c`](NUM-07_test.c) | Legacy toolkit (5.2.4, 5.3.0): reads each force main's full flow (`swmm_LINK_FULLFLOW`) and the flow the overfed kinematic-wave pipes pass at the end of the run |
| [`NUM-07_test6.c`](NUM-07_test6.c) | The same through the 6.0.0 C API; the full flow is the link flow divided by `swmm_link_get_capacities_bulk()` (q/q_full) |

Both tests require each full flow to be the Manning full flow (with the eq. 7-35 n under dynamic wave, with n = 0.013 under kinematic wave) within 1%.

```sh
tools/run-test.sh NUM-07            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-07 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 5.2.4 and 6.0.0 print the same):

```
Dynamic wave, H-W force mains (C = 130, S = 0.1%)
Link   D (ft)   equiv. n   Manning full   H-W full   SWMM full   error
FM1     1.00    0.01082      1.3537       1.3479     1.4243    +5.21%
FM05    0.50    0.01052      0.2192       0.2177     0.2365    +7.92%
FM4     4.00    0.01144     51.6328      51.6498    51.6328    +0.00%

Kinematic wave, 2 cfs into 1 ft pipes (n = 0.013, S = 0.1%)
Manning full flow                  1.1267 cfs
CIRCULAR   flow at end of run      1.1267 cfs  (+0.00%)
FORCE_MAIN flow at end of run      1.1854 cfs  (+5.21%)
FAIL: force-main full flow is not the Manning full flow: dynamic wave up to +7.9%, kinematic wave 1.1854 cfs instead of 1.1267 cfs (+5.2%)
NUM-07 5.3.0 base: FAIL
```

**With the fix** (5.3.0 and 6.0.0 print the same):

```
FM1     1.00    0.01082      1.3537       1.3479     1.3537    +0.00%
FM05    0.50    0.01052      0.2192       0.2177     0.2192    +0.00%
FM4     4.00    0.01144     51.6328      51.6498    51.6328    +0.00%
...
CIRCULAR   flow at end of run      1.1267 cfs  (+0.00%)
FORCE_MAIN flow at end of run      1.1267 cfs  (+0.00%)
PASS: force-main full flow is the Manning full flow under dynamic and kinematic wave
NUM-07 5.3.0 patched: PASS
```

With the fix the dynamic-wave full flow is within 0.7% of the Hazen-Williams full flow at all three sizes; the remaining difference comes from the rounded exponent 0.04 in eq. 7-35.

## The fix

```diff
-        xsect->sFull = xsect->aFull * pow(xsect->rFull, 0.63);
-        xsect->sMax  = 1.06949 * xsect->sFull;
+        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
+        xsect->sMax  = 1.08 * xsect->sFull;
```

These are the CIRCULAR values; a force main is a circular pipe for every purpose except pressurized friction, which `dwflow.c` computes from the C-factor or roughness height and does not change. The 6.0.0 patch makes the same change in `XSection.cpp`.

Effect on other models: only FORCE_MAIN conduits change. None of the 73 decks of the SWMM regression test suite has a FORCE_MAIN cross-section (several set `FORCE_MAIN_EQUATION`, which has no effect without one). In the dynamic-wave test deck the depths, flows and continuity are unchanged; only the full flows and Max/Full Flow ratios change. Users of small force mains under dynamic wave will see higher Max/Full Flow ratios; under kinematic wave and steady flow, small force mains now carry 5-8% less, the same as a CIRCULAR pipe with the same n.
