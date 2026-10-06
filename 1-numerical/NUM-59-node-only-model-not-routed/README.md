# NUM-59: 5.2.4 never routes the inflows of a model that has nodes but no links

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | In 5.2.4 every inflow to a link-less model (a pond or infiltration basin modelled as a lone storage unit) is booked and then lost: storage stays empty, no exfiltration is computed, and the flow routing continuity error is 100 %. The error is printed in the report but nothing explains it. Under DYNWAVE all three versions stop with ERROR 145 instead, so this is a KINWAVE/STEADY case. |
| **Reached from** | Any input file with nodes and inflows but no links, with KINWAVE or STEADY routing |
| **5.3.0** | Fixed: `routeFlow()` in [`src/legacy/engine/routing.c:517`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/routing.c#L517) routes when there is at least one node |
| **5.2.4** | `routeFlow()` in [`src/solver/routing.c:414`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/routing.c#L414) only routes when there is at least one link; EPA's `develop` branch still has this test |
| **6.0.0** | Not affected: routing runs whenever there are nodes ([`SWMMEngine.cpp:1312`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L1312)) |
| **Since** | Every 5.x release up to 5.2.4; fixed in the fork by commit 6bcd52c2 ("Work in progress addressing #149", November 2023) |
| **Fix** | None needed for 5.3.0 and 6.0.0 |

## The problem

A model of a single storage unit with an inflow and exfiltration is a legitimate SWMM model: there is nothing to convey, but the unit should fill and lose water through its bottom. In 5.2.4 it does not fill. The test deck has two storage units of constant area 10,000 ft² and no links, each receiving 1 cfs for 6 hours (21,600 ft³). S1 is sealed; S2 exfiltrates at Ksat 0.5 in/hr. 5.2.4 ends with both empty and reports:

```
  Flow Routing Continuity        acre-feet      10^6 gal
  External Inflow ..........         0.991         0.323
  Exfiltration Loss ........         0.000         0.000
  Final Stored Volume ......         0.000         0.000
  Continuity Error (%) .....       100.000
```

5.3.0 and 6.0.0 end with 21,595 ft³ in S1 (2.159 ft) and 19,097 ft³ in S2 (1.910 ft), an exfiltration loss of 0.057 ac-ft and a continuity error of 0.000 %.

## Why it happens

5.2.4 adds every inflow to the nodes and to the mass balance, but the step that updates node volumes is skipped when there are no links:

```c
// src/solver/routing.c, routeFlow()   (5.2.4)
for (j = 0; j < Nobjects[NODE]; j++)
    node_initFlows(j, routingStep);

// --- route flow through the drainage network
if ( Nobjects[LINK] > 0 )
{
    stepCount = flowrout_execute(SortedLinks, routingModel, routingStep);
}
```

Routing itself is on (`DoRouting` is true whenever there are nodes), so the inflow is booked as External Inflow, while storage volume, depth and exfiltration never change. The whole inflow ends up as continuity error.

Fork commit 6bcd52c2 changed the test to `if ( Nobjects[LINK] > 0 || Nobjects[NODE] > 0)` ("Route flows when there is at least one node/link to ensure nodal seepage is applied even with node only models"), which is what 5.3.0 has. 6.0.0 gates routing on the number of nodes only.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-59_storage-only.inp`](NUM-59_storage-only.inp) | Two storage units (10,000 ft², one sealed, one exfiltrating), 1 cfs each for 6 hours, no links, KINWAVE |
| [`NUM-59_test.c`](NUM-59_test.c) | Legacy toolkit (5.2.4 and 5.3.0): runs the deck, reads the final volumes and the flow routing continuity error |
| [`NUM-59_test6.c`](NUM-59_test6.c) | The same for 6.0.0 |

The test expects S1 to hold the 21,600 ft³ it received to within 0.5 % (the trapezoidal first step admits 5 ft³ instead of 10, so it ends with 21,595 ft³), S2 to hold less than S1 but more than nothing, and a continuity error below 0.5 %.

```sh
tools/run-test.sh NUM-59   # 5.2.4: FAIL; 5.3.0 and 6.0.0: PASS
```

```
---- 5.2.4 ----
Inflow volume per node           21600.0 ft3
S1 (sealed) final volume             0.0 ft3   depth  0.000 ft   (expected 21600.0 ft3, 2.160 ft)
S2 (exfiltrates) final volume        0.0 ft3   depth  0.000 ft   (expected between 0 and S1)
Flow routing continuity error    100.000 %
FAIL: the inflow to the link-less model is not routed: S1 holds 0.0 of 21600.0 ft3, continuity error 100.000 %

---- 5.3.0 ----
S1 (sealed) final volume         21595.0 ft3   depth  2.159 ft   (expected 21600.0 ft3, 2.160 ft)
S2 (exfiltrates) final volume    19096.7 ft3   depth  1.910 ft   (expected between 0 and S1)
Flow routing continuity error      0.000 %
PASS: the storage units hold the inflow they received (continuity error 0.000 %)

---- 6.0.0 ----
S1 (sealed) final volume         21595.0 ft3   depth  2.159 ft   (expected 21600.0 ft3, 2.160 ft)
S2 (exfiltrates) final volume    19096.7 ft3   depth  1.910 ft   (expected between 0 and S1)
Flow routing continuity error      0.022 %
PASS: the storage units hold the inflow they received (continuity error 0.022 %)
```

6.0.0's report prints 0.000 % for this run; the 0.022 % comes from `swmm_get_routing_continuity_error()`.

## The fix

None needed: 5.3.0 and 6.0.0 route the inflows. Backporting the one-line change of 6bcd52c2 fixes 5.2.4 and EPA's `develop` branch.

The report side was not updated with the 5.3.0 change, and 6.0.0 copies it: for a link-less model both still print `Flow Routing ........... NO` in the analysis options, although the flows were routed, and omit the Node Depth, Node Inflow and Storage Volume summaries, because the node statistics are only allocated, updated and written when there is at least one link (`stats.c:157`, `routing.c:263`, `statsrpt.c:117`, `report.c:288`). The numbers in the `.out` file and the continuity table are right. Making the report cover link-less models touches all of those places and is not done here.
