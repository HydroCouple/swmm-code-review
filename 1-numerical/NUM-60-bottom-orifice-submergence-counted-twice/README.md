# NUM-60: A bottom orifice below tailwater is corrected for submergence twice

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A bottom orifice whose tailwater stands above its crest passes too little flow in its weir regime, and at the head H* where the weir and orifice formulas should meet its flow jumps by the inverse of Villemonte's factor (+43% in the test, +50% on a 28,000-node sewer model). An inflow inside the jump has no steady state: the upstream level hunts and dynamic-wave steps fail to converge. In the test a unit that should hold 1.243 ft holds 1.320 ft, and one that should hold 1.354 ft never settles. No warning. |
| **Reached from** | `[ORIFICES]` of type `BOTTOM` whose downstream water level rises above the orifice crest, under dynamic wave (kinematic wave and steady flow use the upstream invert as the tailwater, so it cannot be above the crest) |
| **5.3.0** | `orifice_getInflow()` in [`src/legacy/engine/link.c:1866`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1866) and [`:1930`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L1930) |
| **5.2.4** | Same code, [`src/solver/link.c:1863`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L1863) and [`:1927`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/link.c#L1927) |
| **6.0.0** | Reproduces with the same numbers in `StructureSolver::computeOrificeFlowK()`, [`src/engine/hydraulics/HydStructures.cpp:629`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/HydStructures.cpp#L629) and [`:737`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/HydStructures.cpp#L737). Since 19 November 2026 `swmm6_rel` applies the fix under `NODE_CONTINUITY SEMI_IMPLICIT` only; the default `EXPLICIT` keeps the legacy law for parity (see [The fix](#the-fix)). |
| **Since** | 5.1.014 at the latest (the oldest version in the repository has the same code) |
| **Fix** | Apply Villemonte's factor to side orifices only: [`NUM-60_swmm530.patch`](NUM-60_swmm530.patch), [`NUM-60_swmm600.patch`](NUM-60_swmm600.patch) |

## The problem

SWMM treats an orifice whose opening is not fully submerged as a weir. For a bottom orifice, `orifice_getWeirCoeff()` places the switch at a head H* = (Cd / 0.414) · area / perimeter and chooses the weir coefficient so that the two formulas give the same flow there: cWeir = Cd · A · √(2g) · √H*. Below H* the flow is cWeir · (h / H*)<sup>1.5</sup>, at and above it Cd · A · √(2g h), and the two meet at h = H*.

When the downstream water level h2 is above the crest, the head h on a bottom orifice is the head difference h1 − h2 (reference manual Equation 6-6), and SWMM uses that head both to decide whether the orifice runs as a weir and in the weir formula. The tailwater is already accounted for. SWMM then multiplies the weir-regime flow by Villemonte's submergence factor as well,

F = [1 − ((h2 − crest) / (h1 − crest))<sup>1.5</sup>]<sup>0.385</sup>,

which corrects a weir flow computed from the head over the crest for a submerged downstream side. Applied to a flow computed from the head difference, it counts the submergence a second time. The weir-regime flow is too small by F, and at h = H* the flow jumps from F · cWeir to cWeir.

The test deck has two storage units (1000 ft², crest at their invert) draining through identical bottom orifices (circular, 1 ft, Cd 0.65, H* = 0.3925 ft, cWeir = 2.567 cfs) to fixed outfalls whose stage is 1 ft above the crests. Just below H* the legacy flow is 1.789 cfs, at H* 2.567 cfs:

| Unit | Inflow | Correct steady depth (h = H* (Q / cWeir)<sup>2/3</sup>) | 5.2.4 / 5.3.0 / 6.0.0 | With the fix |
|---|---|---|---|---|
| S1 | 1.25 cfs | 1.2430 ft | 1.3203 ft | 1.2430 ft |
| S2 | 2.2 cfs (inside the jump) | 1.3542 ft | about 1.3926 ft (= 1 ft + H*), never steady: the orifice flow switches between 1.799 and 2.567 cfs | 1.3542 ft |

S1 settles where the doubly reduced flow passes 1.25 cfs, 0.077 ft too high. No depth passes S2's 2.2 cfs: below H* the orifice passes at most 1.789 cfs and at H* 2.567 cfs, so the level stays at H* and the orifice flow switches between the two for the whole run.

On a real network such an orifice is a node whose dynamic-wave iteration never converges. In a 28,000-node sewer model a force-main discharge chamber is drained into a tank by one flap-gated bottom orifice, with the tank level above the crest. Three pumps deliver 54 cfs, the orifice jumps from about 51 to 77 cfs at H*, and the chamber was the most frequent non-converging node of the run (8.5% of routing steps under `NODE_CONTINUITY SEMI_IMPLICIT`).

## Why it happens

```c
// src/legacy/engine/link.c, orifice_getInflow()
    // --- orifice is a bottom orifice (oriented in horizontal plane)
    if ( Orifice[k].type == BOTTOM_ORIFICE )
    {
        // --- compute crest elevation
        hcrest = Node[n1].invertElev + Link[j].offset1;

        // --- compute head on orifice
        if (h1 < hcrest) head = 0.0;
        else if (h2 > hcrest) head = h1 - h2;      // tailwater above the crest
        else head = h1 - hcrest;

        // --- find fraction of critical height for which weir flow occurs
        f = head / Orifice[k].hCrit;
    }
    ...
    // --- find flow through the orifice
    q = dir * orifice_getFlow(j, k, head, f, Link[j].hasFlapGate);   // cWeir * f^1.5 for f < 1

    // --- apply Villemonte eqn. to correct for submergence
    if ( f < 1.0 && h2 > hcrest )                                    // bottom orifices too
    {
        ratio = (h2 - hcrest) / (h1 - hcrest);
        q *= pow( (1.0 - pow(ratio, 1.5)), 0.385);
    }
```

For a side orifice in its weir regime the head is h1 − crest (the opening is not full), so the factor is the right correction. For a bottom orifice with the tailwater above the crest the head is h1 − h2, and the same block applies the factor again. The reference manual (Section 6.2.3) describes the bottom-orifice weir regime with the head over the crest, H1 − Z<sub>O</sub>, followed by the factor; the code switches to the head difference but keeps the factor.

6.0.0 ports the same branches (`else if (h2 > hcrest) head = h1 - h2;` and the unguarded `if (f < 1.0 && h2 > hcrest)` block) and gives the same numbers.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-60_bottom-orifice.inp`](NUM-60_bottom-orifice.inp) | Storage units S1 and S2 (1000 ft², start at 1.3 ft) receive 1.25 and 2.2 cfs and drain through bottom orifices OR1 and OR2 (circular, 1 ft, Cd 0.65, crest at the invert) to FIXED outfalls at a stage 1 ft above the crests. Dynamic wave, 5 s steps, 2 hours. |
| [`NUM-60_test.c`](NUM-60_test.c) | Runs the deck through the legacy toolkit and checks each unit's final depth against 1 ft + H* (Q / cWeir)<sup>2/3</sup> (tolerance 0.005 ft) and that it holds still over the last hour (range below 0.001 ft) |
| [`NUM-60_test6.c`](NUM-60_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-60            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-60 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print the table below; 6.0.0 prints the same except that S2's final depth is 1.3925 ft (the level hunts across H*, and the last step catches it at another point):

```
Orifice flow with the tailwater 1 ft above the crest: 1.789 cfs just below H* = 0.3925 ft, 2.567 cfs at H*
Unit  Inflow (cfs)  Final depth (ft)  Expected (ft)  Depth range (ft)  Orifice flow range (cfs)
S1           1.250            1.3203         1.2430            0.0000        1.250 - 1.250        <--
S2           2.200            1.3927         1.3542            0.0008        1.799 - 2.567        <--
FAIL: 2 of 2 units do not settle at the head difference the bottom orifice's own weir and orifice formulas give
NUM-60 5.3.0 base: FAIL
```

**With the fix**, 5.3.0 and 6.0.0 print identical numbers:

```
Orifice flow with the tailwater 1 ft above the crest: 1.789 cfs just below H* = 0.3925 ft, 2.567 cfs at H*
Unit  Inflow (cfs)  Final depth (ft)  Expected (ft)  Depth range (ft)  Orifice flow range (cfs)
S1           1.250            1.2430         1.2430            0.0000        1.250 - 1.250
S2           2.200            1.3542         1.3542            0.0000        2.200 - 2.200
PASS: below tailwater each bottom orifice passes its inflow at the head difference its weir and orifice formulas give
NUM-60 5.3.0 patched: PASS
NUM-60 6.0.0 patched: PASS
```

## The fix

Apply the factor to side orifices only:

```diff
     // --- apply Villemonte eqn. to correct for submergence
-    if ( f < 1.0 && h2 > hcrest )
+    //     (a bottom orifice below tailwater already uses the head
+    //     difference h1 - h2, so it is not corrected a second time)
+    if ( f < 1.0 && h2 > hcrest && Orifice[k].type == SIDE_ORIFICE )
```

The bottom orifice's flow is then continuous at H*, where cWeir = Cd · A · √(2g H*) by construction, and increases steadily with the head difference. `orifice_getFlow()`'s dQ/dH, 1.5 · q / (f · H*), is already the derivative of the corrected flow. Side orifices, bottom orifices whose tailwater is below the crest, and every orifice running full are unchanged. The 6.0.0 patch makes the same change in `computeOrificeFlowK()`.

Following the manual instead, with the weir regime on the head over the crest and the factor kept, would also remove the double count, but the flow would still jump at the switch to the orifice formula (whose head is the head difference); this fix makes the bottom orifice's own two formulas meet.

**In OpenSWMM.** On 19 November 2026 the project owner chose to keep the legacy law for now in the bundled 5.3.0 solver and under 6.0.0's default `NODE_CONTINUITY EXPLICIT`, so that 6.0.0 keeps matching the legacy solver bit for bit. `swmm6_rel` (commit 5b2b673e, not yet pushed) applies this fix under `NODE_CONTINUITY SEMI_IMPLICIT` (an option with no legacy counterpart), together with two other changes that let that iteration converge on regulators near equal heads (hydraulics reference manual, Section 3.5; engine test `test_engine_si_structure_laws`). The patches here make the change in both engines and both continuity options.
