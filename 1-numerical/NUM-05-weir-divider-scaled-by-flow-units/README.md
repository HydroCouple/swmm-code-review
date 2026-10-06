# NUM-05: A WEIR divider's flow depends on the flow units chosen

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | The same physical WEIR divider diverts 95% of a 3 cfs inflow in CFS and CMS, but 76% in MGD, 11% in MLD, 5% in GPM and 3% in LPS. With a non-zero Qmin the GPM model is rejected with ERROR 137. No warning. In 6.0.0 a divider with Qmin > Cd·Ht<sup>1.5</sup> is not rejected at all: it computes NaN flows and reports the whole inflow as flooding. |
| **Reached from** | Any `[DIVIDERS]` entry of type `WEIR` in a model whose flow units are GPM, MGD, LPS or MLD |
| **5.3.0** | `divider_validate()` and `divider_getOutflow()` in [`src/legacy/engine/node.c:1227`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L1227) and [`:1287`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L1287) |
| **5.2.4** | Same code, [`src/solver/node.c:1241`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L1241) and [`:1301`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L1301) |
| **6.0.0** | Reproduces in `divider::getOutflow()`, [`src/engine/hydraulics/Divider.cpp:80`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Divider.cpp#L80), and ERROR 137 is defined but never raised, so an invalid divider runs with NaN flows |
| **Since** | Every release (the code is the same in 5.0.022, the oldest version in the repository) |
| **Fix** | Convert Cd·Ht<sup>1.5</sup> from CFS/CMS, as weir links do; 6.0.0 also gets the ERROR 137 check: [`NUM-05_swmm530.patch`](NUM-05_swmm530.patch), [`NUM-05_swmm600.patch`](NUM-05_swmm600.patch) |

## The problem

A WEIR divider sends flow into its diversion link with the weir equation

Q<sub>div</sub> = C<sub>W</sub> (f H<sub>W</sub>)<sup>1.5</sup>, f = (Q<sub>in</sub> − Q<sub>min</sub>) / (Q<sub>max</sub> − Q<sub>min</sub>), Q<sub>max</sub> = C<sub>W</sub> H<sub>W</sub><sup>1.5</sup>

The Reference Manual (Vol. II, Kinematic Wave chapter and symbol list) gives c<sub>W</sub> in ft and sec units, so C<sub>W</sub> H<sub>W</sub><sup>1.5</sup> is a flow in cfs. The input manual only says "discharge coefficient for a WEIR divider", and defines the coefficients of weir links as "for CFS if using US flow units or CMS if using metric flow units".

The code instead takes C<sub>W</sub> H<sub>W</sub><sup>1.5</sup> to be in the model's own flow units. The same divider then gives a different split for each flow unit. In the test below, 3 cfs enters a divider with H<sub>W</sub> = 1 ft and C<sub>W</sub> = 3.33 (H<sub>W</sub> = 0.3048 m and C<sub>W</sub> = 0.5604 in the SI decks); the weir equation diverts 94.9% of it:

| Flow units | Q<sub>max</sub> the code uses | Diverted |
|---|---|---|
| CFS | 3.33 cfs | 94.9% |
| GPM | 3.33 gpm = 0.0074 cfs | 5.0% |
| MGD | 3.33 MGD = 5.15 cfs | 76.3% |
| CMS | 0.0943 cms = 3.33 cfs | 94.9% |
| LPS | 0.0943 L/s = 0.0033 cfs | 3.3% |
| MLD | 0.0943 MLD = 0.039 cfs | 11.3% |

When Q<sub>max</sub> comes out below Q<sub>min</sub>, 5.2.4 and 5.3.0 reject the model with ERROR 137 (`Weir Divider D1 has invalid parameters`). That happens to the GPM deck as soon as Q<sub>min</sub> is 1 cfs (448.8 gpm), although the divider is valid in cfs (Q<sub>max</sub> = 3.33 cfs).

6.0.0 copies the formula and has no ERROR 137 check. In the same GPM deck f is negative, `pow(f*Ht, 1.5)` is NaN, both outlet conduits carry 0 and the report books all 0.743 ac-ft of inflow as flooding at D1. A divider that is invalid in any unit (Q<sub>min</sub> 4 cfs, Q<sub>max</sub> 3.33 cfs, inflow 5 cfs) also runs, floods 1.238 ac-ft and reports a continuity error of 0.000%.

## Why it happens

Q<sub>min</sub> is converted from flow units to cfs when it is read, H<sub>W</sub> and C<sub>W</sub> are kept as entered, and the weir equation divides by the flow-unit factor:

```c
// src/legacy/engine/node.c, divider_validate()
            // --- find flow when weir is full
            Divider[k].qMax = Divider[k].cWeir * pow(Divider[k].dhMax, 1.5)
                              / UCF(FLOW);
...
// src/legacy/engine/node.c, divider_getOutflow()
            else qOut = Divider[i].cWeir *
                        pow(f*Divider[i].dhMax, 1.5) / UCF(FLOW);
```

`UCF(FLOW)` is 448.831 for GPM, 0.64632 for MGD, 28.317 for LPS and 2.4466 for MLD, so C<sub>W</sub> H<sub>W</sub><sup>1.5</sup> is read as gallons per minute, million gallons per day, and so on. Weir links do this right: `weir_getFlow()` evaluates the equation with lengths in ft or m and divides only by `M3perFT3` when `UnitSystem == SI` (link.c:2444).

6.0.0 does the same with `ucf::Qcf[flow_units]`:

```cpp
// src/engine/hydraulics/Divider.cpp, divider::getOutflow()
            const double qMax = cWeir * std::pow(dhMax, 1.5) / ucf_flow;
            ...
                const double f = (qIn - qMin) / (qMax - qMin);
                if (f > 1.0) qOut = qMax * std::sqrt(f);
                else qOut = cWeir * std::pow(f * dhMax, 1.5) / ucf_flow;
```

and `resolve_cross_references()` checks a divider's diversion link (ERROR 136) but not its weir parameters, so `ERR_WEIR_DIVIDER` (137) is never raised. With Q<sub>in</sub> > Q<sub>min</sub> > Q<sub>max</sub>, f is negative and `std::pow` returns NaN.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-05_cfs.inp`](NUM-05_cfs.inp), [`_gpm`](NUM-05_gpm.inp), [`_mgd`](NUM-05_mgd.inp), [`_cms`](NUM-05_cms.inp), [`_lps`](NUM-05_lps.inp), [`_mld`](NUM-05_mld.inp) | The same system in each flow unit: 3 cfs into WEIR divider D1 (Q<sub>min</sub> 0, H<sub>W</sub> 1 ft or 0.3048 m, C<sub>W</sub> 3.33 or 0.5604), main link CM, diversion link CD, kinematic wave, 3 hours |
| [`NUM-05_gpm_qmin.inp`](NUM-05_gpm_qmin.inp) | GPM deck with Q<sub>min</sub> = 448.831 gpm (1 cfs) |
| [`NUM-05_invalid.inp`](NUM-05_invalid.inp) | CFS, Q<sub>min</sub> 4 cfs > Q<sub>max</sub> 3.33 cfs, inflow 5 cfs: must be rejected with ERROR 137 |
| [`NUM-05_test.c`](NUM-05_test.c) | Runs every deck through the legacy toolkit and compares the diverted fraction CD/(CM+CD) at the end with the weir equation evaluated in cfs; checks that the invalid deck gives error 137 |
| [`NUM-05_test6.c`](NUM-05_test6.c) | The same through the 6.0.0 C API; the invalid deck must fail to open |

```sh
tools/run-test.sh NUM-05            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh NUM-05 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print the same table:

```
Deck        Qmax(cfs)  expected  diverted  error
cfs           3.3300    0.9492    0.9492      0
gpm           3.3300    0.9492    0.0497      0
mgd           3.3300    0.9492    0.7631      0
cms           3.3302    0.9491    0.9492      0
lps           3.3302    0.9491    0.0333      0
mld           3.3302    0.9491    0.1133      0
gpm_qmin      3.3300    0.8827       nan    137
invalid: Qmin 4 cfs > Qmax 3.33 cfs, inflow 5 cfs -> error 137 (expected 137)
FAIL: 5 of 7 decks do not divert the weir-equation fraction of the inflow
NUM-05 5.3.0 base: FAIL
```

6.0.0 gives the same fractions, runs the GPM deck with Q<sub>min</sub> into NaN, and runs the invalid divider:

```
gpm_qmin      3.3300    0.8827      -nan      0
invalid: Qmin 4 cfs > Qmax 3.33 cfs, inflow 5 cfs -> ran, diverted fraction -nan (expected: rejected)
FAIL: 5 of 7 decks do not divert the weir-equation fraction of the inflow; the invalid divider was run instead of rejected
NUM-05 6.0.0 base: FAIL
```

**With the fix**, 5.3.0 prints the following, and 6.0.0 the same fractions:

```
Deck        Qmax(cfs)  expected  diverted  error
cfs           3.3300    0.9492    0.9492      0
gpm           3.3300    0.9492    0.9492      0
mgd           3.3300    0.9492    0.9492      0
cms           3.3302    0.9491    0.9491      0
lps           3.3302    0.9491    0.9491      0
mld           3.3302    0.9491    0.9491      0
gpm_qmin      3.3300    0.8827    0.8827      0
invalid: Qmin 4 cfs > Qmax 3.33 cfs, inflow 5 cfs -> error 137 (expected 137)
PASS: the same WEIR divider diverts the same fraction in every flow unit, and an invalid one is rejected
```

6.0.0 now stops the invalid deck:

```
  invalid: rc 5,   ERROR 137: Weir Divider D1 has invalid parameters.
invalid: Qmin 4 cfs > Qmax 3.33 cfs, inflow 5 cfs -> rejected, rc 5
PASS: the same WEIR divider diverts the same fraction in every flow unit, and an invalid one is rejected
```

## The fix

Evaluate C<sub>W</sub> H<sub>W</sub><sup>1.5</sup> as cfs (US) or cms (SI) and convert it as `weir_getFlow()` does:

```diff
             Divider[k].qMax = Divider[k].cWeir * pow(Divider[k].dhMax, 1.5)
-                              / UCF(FLOW);
+                              / (UnitSystem == SI ? M3perFT3 : 1.0);
...
             else qOut = Divider[i].cWeir *
-                        pow(f*Divider[i].dhMax, 1.5) / UCF(FLOW);
+                        pow(f*Divider[i].dhMax, 1.5) /
+                        (UnitSystem == SI ? M3perFT3 : 1.0);
```

The 6.0.0 patch makes the same change in `Divider.cpp` and adds legacy's check to `resolve_cross_references()`: a WEIR divider with H<sub>W</sub> ≤ 0, C<sub>W</sub> ≤ 0 or Q<sub>min</sub> > C<sub>W</sub> H<sub>W</sub><sup>1.5</sup> raises ERROR 137.

What changes for users: CFS models are unchanged. CMS models change by the ratio of SWMM's two cubic-metre factors (`UCF(FLOW)` = 0.02832, `M3perFT3` = 0.028317), +0.011% in Q<sub>max</sub>, which moves the diverted fraction of the CMS deck from 0.9492 to 0.9491. Models in GPM, MGD, LPS or MLD whose divider coefficient was tuned to the old behaviour need C<sub>W</sub> divided by 448.831 (GPM), 0.64632 (MGD), 1000 (LPS) or 86.4 (MLD) to keep their results. The manual's entry for Cd should say "for CFS if using US flow units or CMS if using metric flow units", as it does for weir links.

Effect on other models: none of the 73 regression decks has a divider node, so none changes.
