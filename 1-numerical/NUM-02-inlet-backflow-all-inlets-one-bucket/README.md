# NUM-02: A capture node's overflow is split across every inlet in the model

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | When a sewer node that receives inlet capture overflows, only part of the overflow returns to the street. The rest is removed from the flooding total but sent nowhere, so it disappears from the model. In the test, 50% of the overflow (1.70 of 3.40 cfs) is lost and the routing continuity error is 18.3%. EPA's Example 7 with inlets goes from 0.01% (5.2.4) to 3.64%. The only warning is the continuity error in the report. |
| **Reached from** | Any model with inlets on two or more conduits whose capture node overflows |
| **5.3.0** | `getBackflowRatios()` in [`src/legacy/engine/inlet.c:1836`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inlet.c#L1836) and [`:1853`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inlet.c#L1853) |
| **5.2.4** | Not affected: the same function uses a local `int n` as the node index ([`src/solver/inlet.c:1820`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/inlet.c#L1820)). EPA's `develop` branch is also unaffected. |
| **6.0.0** | Reproduces on purpose: `InletSolver::computeBackflowRatios()` keeps one model-wide bucket "to match the oracle" ([`src/engine/hydraulics/Inlet.cpp:1097`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Inlet.cpp#L1097)) |
| **Since** | 5.3.0, fork commit 5966f12d (4 April 2025, "Configuring cibuildwheel for portability") |
| **Fix** | Index the per-node accumulator by the capture node again: [`NUM-02_swmm530.patch`](NUM-02_swmm530.patch), [`NUM-02_swmm600.patch`](NUM-02_swmm600.patch) |

## The problem

An inlet sends captured street flow to a capture node, usually a sewer junction. If that node overflows, SWMM returns the overflow to the street as inlet *backflow*: each inlet draining to the node gets a share of its overflow, in proportion to its open area (standard inlets) or its inlet count (custom inlets). The shares of the inlets on one capture node add up to 1, and the overflow is removed from the system flooding total because it stays in the model.

In 5.3.0 the shares are computed over all inlets of the model instead of the inlets on each capture node. A capture node served by one of two equal grate inlets returns 50% of its overflow, one served by one of ten returns 10%. The flooding total is still reduced by the whole overflow, so the part that is not returned is neither flooding nor street flow: it is lost.

The test deck has two streets, each carrying 2 cfs to a 2 ft x 2 ft grate inlet. Street 1's inlet drains to sewer node CN1, street 2's to CN2. CN1 also receives 5 cfs of sewer inflow, more than its outlet pipe can take, so it overflows by 3.40 cfs. That overflow must all come back to street 1. In 5.3.0 and 6.0.0 only 1.70 cfs does; the flow below the inlet is 2.39 cfs instead of 4.09 cfs, and the run reports a routing continuity error of 18.3%.

## Why it happens

5.3.0 renamed the local node index of `getBackflowRatios()` from `n` to `nodeIndex` but did not change the two assignments. They now write the capture node's index into the file-scope Manning roughness `n`, and `nodeIndex` stays 0:

```c
// src/legacy/engine/inlet.c
static double n;             // Manning's roughness coeff.        (line 188)
...
void getBackflowRatios()
{
    ...
    int     nodeIndex = 0;
    ...
    for (inlet = FirstInlet; inlet != NULL; inlet = inlet->nextInlet)
    {
        n = inlet->nodeIndex;                         // should set nodeIndex
        inletNodes[nodeIndex].numInletLinks++;        // always slot 0
        area = getInletArea(inlet);
        if (area > 0.0)
        {
            inletNodes[nodeIndex].numStdInletLinks++;
            inletNodes[nodeIndex].totalInletArea += area;
        }
        ...
    }
    for (inlet = FirstInlet; inlet != NULL; inlet = inlet->nextInlet)
    {
        n = inlet->nodeIndex;
        f = (double) inletNodes[nodeIndex].numStdInletLinks /
            (double) inletNodes[nodeIndex].numInletLinks;
        ...
            inlet->backflowRatio = area / inletNodes[nodeIndex].totalInletArea * f;
    }
```

Every inlet adds to `inletNodes[0]`, so `totalInletArea` is the open area of all inlets in the model. During routing, each inlet's backflow is its own capture node's overflow times that ratio, while `inlet_adjustQualOutflows()` still removes the node's whole overflow from the flooding total:

```c
// src/legacy/engine/inlet.c, inlet_findCapturedFlows()
inlet->backflow = Node[m].overflow * inlet->backflowRatio;      // line 618
// src/legacy/engine/inlet.c, inlet_adjustQualOutflows()
StepFlowTotals.flooding -= q;                                    // line 735, q = Node[j].overflow
```

Overwriting `n` itself does no harm: `getConduitGeometry()` sets it again before every use.

6.0.0 had per-node grouping and then changed it to a single bucket to match 5.3.0. Its comment calls this "a bug, but it is the oracle's behaviour" (`Inlet.cpp:1097-1109`).

The review pass also flagged a second mechanism here (NUM-27, merged into this issue): the backflow uses the overflow from the previous routing step, while the flooding credit uses the current one. With correct ratios the two differ only by a one-step lag that cancels over an event. On a deck with six overflow pulses and variable steps, 5.2.4 returned 3.8 ft³ less than it credited out of 15,420 ft³ of overflow (0.006% of the inflow). The material loss comes from the ratios.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-02_two-capture-nodes.inp`](NUM-02_two-capture-nodes.inp) | Two streets with one grate inlet each, draining to sewer nodes CN1 and CN2. CN1 gets 5 cfs of extra sewer inflow and overflows for the whole 3-hour run. |
| [`NUM-02_test.c`](NUM-02_test.c) | Legacy toolkit (5.2.4, 5.3.0). At steady state, finds the inlet capture from the CN1 balance and the backflow from the J2 balance, and checks that backflow / CN1 overflow = 1 |
| [`NUM-02_test6.c`](NUM-02_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-02            # 5.2.4: PASS, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-02 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 returns all of the overflow:

```
  CN1 overflow                    3.401
  P1  (CN1 outlet pipe)           2.910
  S1a (street into inlet)         2.000
  S1b (street below inlet)        4.090
  capture by inlet S1a            1.311
  backflow into street 1          3.401
  backflow / CN1 overflow         1.000   (correct: 1.000)
  routing continuity error       -0.639 %
NUM-02 5.2.4 base: PASS
```

5.3.0 returns half of it (6.0.0 prints the same values):

```
  CN1 overflow                    3.401
  P1  (CN1 outlet pipe)           2.910
  S1a (street into inlet)         2.000
  S1b (street below inlet)        2.389
  capture by inlet S1a            1.311
  backflow into street 1          1.700
  backflow / CN1 overflow         0.500   (correct: 1.000)
  routing continuity error       18.286 %
FAIL: only 50.0% of CN1's overflow (1.700 of 3.401 cfs) returns to the street of the one inlet draining to it; routing continuity error 18.29%
NUM-02 5.3.0 base: FAIL
NUM-02 6.0.0 base: FAIL
```

**With the fix** both engines give the 5.2.4 result:

```
  S1b (street below inlet)        4.090
  capture by inlet S1a            1.311
  backflow into street 1          3.401
  backflow / CN1 overflow         1.000   (correct: 1.000)
  routing continuity error       -0.638 %
PASS: all of CN1's overflow returns to the street of the inlet draining to it (ratio 1.000)
NUM-02 5.3.0 patched: PASS
NUM-02 6.0.0 patched: PASS
```

The 6.0.0 test reads the continuity error from `swmm_get_routing_continuity_error()`, which prints 18.269% unpatched and -0.658% patched. The 6.0.0 report files show 18.286% and -0.638%, the same as 5.3.0.

## The fix

For 5.3.0, assign the capture node's index to the accumulator index, as 5.2.4 did:

```diff
     for (inlet = FirstInlet; inlet != NULL; inlet = inlet->nextInlet)
     {
-        n = inlet->nodeIndex;
+        nodeIndex = inlet->nodeIndex;
         inletNodes[nodeIndex].numInletLinks++;
```

(the same change at line 1853). The 6.0.0 patch replaces the single bucket in `computeBackflowRatios()` with per-node sums, accumulated in the same order as legacy, so both patched engines give the same ratios.

**Effect on other models.** Only models with more than one inlet change. From the regression suite (5.3.0 and 6.0.0 give the same numbers, patched and unpatched):

| Deck | 5.2.4 | 5.3.0 | 5.3.0 patched |
|---|---|---|---|
| `update_v52/Example7-Inlets.inp`: outflow (ac-ft), continuity error | 7.307, 0.013% | 7.042, 3.639% | 7.305, 0.034% |
| `update_v52/CoS-Reduced-Inlets.inp`: outflow (10^6 L), continuity error | 3.966, 0.240% | 3.787, 4.705% | 3.966, 0.239% |

The single-inlet decks `street_grate_inlet_7.inp` and `onsag_grate_inlet_11.inp` are unchanged.
