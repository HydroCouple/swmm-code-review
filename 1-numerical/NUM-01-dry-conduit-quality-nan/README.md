# NUM-01: Every dry conduit gets a NaN pollutant concentration, including in EPA's Example1

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Any 5.3.0 run with pollutants under KINWAVE or STEADY routing writes NaN concentrations for conduits that hold no water, and the NaN spreads downstream. The Quality Routing Continuity table shows `nan` for External Inflow, Exfiltration Loss and Final Stored Mass, the Outfall Loading and Link Pollutant Load summaries are `nan`, and mass reaching the outfall is lost from External Outflow (all 2.810 lb in the test network, 0.308 lb in Example1). The reported continuity error is 0.000 %, so nothing warns the user. |
| **Reached from** | Any input file with a `[POLLUTANTS]` section and a conduit that is empty with no inflow during a step (every conduit before the first rain, every unused branch). No API call is needed. |
| **5.3.0** | The link API pollutant-flux block of `findLinkQual()`, [`src/legacy/engine/qualrout.c:377`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L377), and the same block in `findSFLinkQual()`, [`qualrout.c:453`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L453) |
| **5.2.4** | Not affected: the link pollutant flux does not exist ([`src/solver/qualrout.c`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/qualrout.c)) |
| **6.0.0** | Not affected: there is no link-level flux; the only API flux is the node flux of `swmm_node_set_quality_mass_flux()` ([`src/engine/quality/QualityRouting.cpp:423`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/quality/QualityRouting.cpp#L423)) |
| **Since** | 5.3.0, fork commit 5b87a2b5 (10 December 2024, "WIP API bindings for pollutants #204, #162, #163, #184, #180") |
| **Fix** | Apply the flux only when it is non-zero and the link has water: [`NUM-01_swmm530.patch`](NUM-01_swmm530.patch) |

## The problem

EPA's Example1.inp (KINWAVE routing, TSS and Lead), run with 5.3.0, ends its report like this:

```
  **************************           TSS          Lead
  Quality Routing Continuity           lbs           lbs
  **************************    ----------    ----------
  Dry Weather Inflow .......         0.000         0.000
  Wet Weather Inflow .......       431.274         0.086
  Groundwater Inflow .......         0.000         0.000
  RDII Inflow ..............         0.000         0.000
  External Inflow ..........          -nan          -nan
  External Outflow .........       409.386         0.082
  Flooding Loss ............        22.228         0.004
  Exfiltration Loss ........          -nan          -nan
  Mass Reacted .............         0.000         0.000
  Initial Stored Mass ......         0.000         0.000
  Final Stored Mass ........          -nan          -nan
  Continuity Error (%) .....         0.000         0.000
...
  Outfall Node           Pcnt       CFS       CFS    10^6 gal           lbs           lbs
  ---------------------------------------------------------------------------------------
  18                    72.92      2.71     19.59       1.914          -nan          -nan
...
  Link                           lbs           lbs
  ------------------------------------------------
  1                             -nan          -nan
  4                             -nan          -nan
  ...                           (all 13 conduits)
```

The deck has no external inflows, no exfiltration and no API calls, yet those lines are `nan`. In total 87 lines of the report contain `nan` (Example1 reports all nodes and links), as do 138 of the 10,728 values in the binary output: TSS and Lead in all 13 conduits at the first reporting time, conduit 5 for 21 of the 36 periods, and node 21 for 20 periods. 5.2.4 and 6.0.0 write finite values everywhere and a continuity error of -0.153 %.

The NaN is not cosmetic. A NaN concentration in a dry branch is carried into the junction it joins (as 0 cfs × NaN), and from there either downstream as NaN (STEADY) or, under KINWAVE, turned into 0 by the `MAX(c, 0.0)` clamp of the mixing function. Either way the mass that reaches the outfall is not counted as outflow. In Example1, External Outflow is 409.386 lb instead of 409.694 lb. In the minimal network of this folder, where one branch (C3) never carries water, the whole 2.810 lb of TSS that leaves the outfall is missing. Under KINWAVE the outfall reports 0 mg/L while 3.758 cfs of 10 mg/L runoff passes through it:

| Time | C1 flow, TSS (5.3.0) | C3 and J2 TSS (5.3.0) | O1 inflow | O1 TSS, 5.3.0 | O1 TSS, with the fix |
|---|---|---|---|---|---|
| 0:15 – 2:00 | 0 cfs, NaN | NaN | 0 cfs | NaN | 0 mg/L |
| 2:15 | 3.997 cfs, 10 mg/L | NaN | 3.758 cfs | **0 mg/L** | 10 mg/L |
| 3:00 | 0.145 cfs, 10 mg/L | NaN | 0.164 cfs | **0 mg/L** | 10 mg/L |
| 6:00 | 0.004 cfs, 10 mg/L | NaN | 0.005 cfs | **0 mg/L** | 10 mg/L |

(values read from the `.out` files of the KINWAVE deck run with the 5.3.0 CLI, before and after the patch; with the fix C3 reads 0 throughout and J2 0 before the rain and 10 mg/L after)

Nothing warns the user. `massbal_getQualError()` sets the error to 0 and then tests `fabs(in - out) < 0.001`, `in > 0` and `out > 0`. Every comparison with NaN is false, so the error stays 0.000 %. That is the number printed, the number `swmm_getMassBalErr()` returns, and the number `massbal_report()` compares with its 10 % limit to decide whether to print the table at all when `CONTINUITY NO` is set ([`massbal.c:307`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L307)).

## Why it happens

5.3.0 added an API pollutant mass flux for links (`swmm_setValueExpanded(..., LINK_POLLUTANT_LATMASS_FLUX, ...)`). `findLinkQual()` applies it to every conduit, for every pollutant, at every step, after the conduit's new concentration has been set. The flux is converted to a concentration over the volume `v1 + qIn*tStep` (start-of-step volume plus inflow volume):

```c
// src/legacy/engine/qualrout.c, findLinkQual()
        // --- set concen. to zero if remaining volume is negligible
        if (v2 < ZeroVolume || Link[i].newDepth <= ZeroDepth)
        {
            massbal_addToFinalStorage(p, c2 * v2);
            c2 = 0.0;
        }

        // --- Calculate bounded externally provided api pollutant flux and update mass balance
        cOut = Link[i].apiExtQualMassFlux[p];

        if (cOut < 0.0)
        {
            ...
        }
        else
        {
            cOut = cOut * tStep / (v1 + qIn * tStep);      // 0 * tStep / 0 = NaN
            c2 += cOut;                                     // link concentration = NaN

            cOut = cOut * (v1 + qIn * tStep);
            Link[i].totalLoad[p] += cOut;                   // Link Pollutant Load Summary = NaN

            cOut = cOut/ tStep;
            massbal_addInflowQual(EXTERNAL_INFLOW, p, cOut); // External Inflow = NaN
        }
```

The flux is 0 unless an API caller sets it, so the `else` branch runs for every conduit. For a conduit that is empty and receives nothing (`v1 = 0`, `qIn = 0`) it computes 0/0. The NaN then travels:

- into the seepage term at the next step, `massbal_addSeepageLoss(p, qSeep * c1)` with `c1 = NaN` ([`qualrout.c:355`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L355)), so Exfiltration Loss is NaN even with no seepage;
- into Final Stored Mass, which sums `c × V` over all links at the end;
- into the downstream node, `w = qLink * Link[i].oldQual[p]; Node[j].newQual[p] += w;` in `findLinkMassFlow()` ([`qualrout.c:216`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L216)), where 0 cfs × NaN is NaN. A conduit that stays dry keeps doing this at every step, so the node stays NaN;
- out of the system ledger: `massbal_addOutflowQual()` books a mass rate as outflow only `if ( w >= 0.0 )` and otherwise subtracts it from external inflow ([`massbal.c:482`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/massbal.c#L482)). A NaN outflow fails the test, so it disappears from External Outflow.

Downstream of a NaN node, `getMixedQual()` ([`qualrout.c:173`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/qualrout.c#L173)) ends with `c = MIN(c, cMax); c = MAX(c, 0.0);`. The macros are `((x)>=(y)) ? (x) : (y)`, so `MAX(NaN, 0.0)` returns 0.0: under KINWAVE the conduit leaving the NaN node carries 0 mg/L, not the true concentration. That is how the outfall of the test network reads 0 mg/L. A conduit that gets water again recovers through the same clamp, which is why most NaNs in Example1 last only one reporting period.

`findSFLinkQual()` (STEADY routing) has a copy of the block with the same divisor (`v1` is the link's old volume, 0 for a dry conduit), so the same happens there. Under DYNWAVE none of the decks tried writes a NaN (Example1 converted to DYNWAVE and the two regression decks with pollutants), because a DYNWAVE conduit's start-of-step volume and adjusted inflow are not both exactly zero there. The guard below covers that case too.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-01_dry-branch-kw.inp`](NUM-01_dry-branch-kw.inp) | 5 ac impervious subcatchment → J1 → C1 → J2 → C2 → outfall O1, plus branch J3 → C3 → J2 that never gets water. Rain from 2:00 to 3:00 at 10 mg/L TSS, KINWAVE, 6 h |
| [`NUM-01_dry-branch-sf.inp`](NUM-01_dry-branch-sf.inp) | The same network with STEADY routing |
| [`NUM-01_example1.inp`](NUM-01_example1.inp) | EPA's Example1.inp, unchanged (from the EPA example-network regression suite) |
| [`NUM-01_test.c`](NUM-01_test.c) | Runs the three decks through the legacy toolkit (5.2.4 and 5.3.0), counts non-finite values in the `.out` file and reads the Quality Routing Continuity table from the `.rpt` |
| [`NUM-01_test6.c`](NUM-01_test6.c) | The same through the 6.0.0 C API |

The test asks for what any run must satisfy: every value in the binary output is finite, and the continuity table balances. It recomputes the error from the table's own lines, 100 × (inflow − outflow) / inflow, and requires it to be within 1 % (correct runs give −0.2 % to +0.2 %; the bug gives NaN).

```sh
tools/run-test.sh NUM-01            # 5.2.4: PASS, 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh NUM-01 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0 writes NaN in all three decks. The two minimal decks book no outflow at all, and the engine's own continuity error reads 0.000 %:

```
Deck                        NaN values   Wet weather  External  External  Final    Error from  Reported
                            in .out      inflow       outflow   inflow    stored   the table   error
                                         (lb, first pollutant)                     (%)         (%)
NUM-01_dry-branch-kw.inp      72/1680         2.810     0.000      -nan     -nan        -nan     0.000  <-- wrong
NUM-01_dry-branch-sf.inp     104/1680         2.810     0.000      -nan     -nan        -nan     0.000  <-- wrong
NUM-01_example1.inp          138/10728      431.274   409.386      -nan     -nan        -nan     0.000  <-- wrong
FAIL: in 3 of 3 decks dry conduits get NaN concentrations and the quality mass balance holds NaN (while the reported continuity error looks fine)
NUM-01 5.3.0 base: FAIL
```

5.2.4 and 6.0.0 pass unpatched:

```
NUM-01_dry-branch-kw.inp       0/1680         2.806     2.806     0.000    0.001      -0.036    -0.027
NUM-01_dry-branch-sf.inp       0/1680         2.806     2.803     0.000    0.000       0.107     0.120
NUM-01_example1.inp            0/10728      431.367   409.775     0.000    0.003      -0.152    -0.153
NUM-01 5.2.4 base: PASS
...
NUM-01_dry-branch-kw.inp       0/1680         2.810     2.808     0.000    0.004      -0.071    -0.051
NUM-01_dry-branch-sf.inp       0/1680         2.810     2.807     0.000    0.004      -0.036     0.001
NUM-01_example1.inp            0/10728      431.274   409.702     0.000    0.002      -0.153    -0.153
NUM-01 6.0.0 base: PASS
```

**With the fix**, 5.3.0 writes no NaN, and the outflow and continuity error agree with 6.0.0 to within the existing small differences between the two engines:

```
NUM-01_dry-branch-kw.inp       0/1680         2.810     2.810     0.000    0.004      -0.142    -0.122
NUM-01_dry-branch-sf.inp       0/1680         2.810     2.807     0.000    0.000       0.107     0.126
NUM-01_example1.inp            0/10728      431.274   409.694     0.000    0.003      -0.151    -0.151
PASS: all output values are finite and the quality mass balance closes in every deck
NUM-01 5.3.0 patched: PASS
```

## The fix

Apply the flux only when there is one and the link has water to carry it, in both functions:

```diff
         cOut = Link[i].apiExtQualMassFlux[p];
 
-        if (cOut < 0.0)
+        // --- a link with no water (v1 + qIn*tStep = 0) takes no flux
+        //     (the division below would give 0/0 = NaN)
+        if (cOut < 0.0 && v1 + qIn * tStep > 0.0)
         {
...
-        else
+        else if (cOut > 0.0 && v1 + qIn * tStep > 0.0)
         {
```

A zero flux added exactly 0 to the concentration, the load and the ledger before, so every link that had water gives bit-identical results. A non-zero flux set on a link with no water is now ignored instead of producing NaN or infinity; it is not booked as inflow, so the mass balance stays closed.

Effect on other models: with the patched 5.3.0 CLI, Example1 changes as shown above (NaN gone, External Outflow 409.386 → 409.694 lb, continuity error −0.151 %), and so does Example1 converted to STEADY (17 report lines with `nan` → none, External Outflow 408.668 → 408.905 lb). The DYNWAVE decks with pollutants (`events_example.inp` from the regression suite, the 6.0.0 site-drainage `Example1.inp`, and Example1 converted to DYNWAVE) give byte-identical `.out` files.

Under STEADY routing the same block also indexes `Link[]` with a node index and dilutes the flux by the link volume ([CRASH-12](../../5-crashes/CRASH-12-steady-link-quality-node-index/), [NUM-50](../NUM-50-steady-link-api-flux-mass-lost/)). Those patches change the same lines of `findSFLinkQual()`, so they are written to apply after this one (their `Requires:` lines).
