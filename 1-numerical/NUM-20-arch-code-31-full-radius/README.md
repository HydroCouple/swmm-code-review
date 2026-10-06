# NUM-20: Standard arch size code 31 has the full hydraulic radius of code 30

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A conduit declared as standard arch code 31 (corrugated steel 3 x 1 in, 36 x 46 in) gets Rfull = 0.773 ft instead of 0.897 ft. Its full-flow capacity is 9.4 % low (15.50 instead of 17.12 cfs in the test), and since the hydraulic radius at every depth is Rfull times a relative table, friction is 22 % too high at every depth, in kinematic and dynamic wave. No warning. |
| **Reached from** | `[XSECTIONS]` `ARCH` with size code 31 |
| **5.3.0** | `Rfull_Arch[]` in [`src/legacy/engine/xsect.dat:466`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.dat#L466), used by `xsect_setParams()` at [`xsect.c:622`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/xsect.c#L622) |
| **5.2.4** | Same table, [`src/solver/xsect.dat:466`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/xsect.dat#L466) |
| **6.0.0** | Reproduces: the table was copied into [`src/engine/hydraulics/xsect_tables.hpp:545`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/xsect_tables.hpp#L545). The reference manual's Table E-1 ([`Appendix.md:247`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/docs/manuals/reference/hydraulics/sections/Appendix.md#L247)) was transcribed from the code and shows the same value |
| **Since** | 5.0.022 or earlier (in the first commit of the EPA repository, 2014). Release 5.1.008 corrected the matching duplicate in `Yfull_Arch[]` but left this one |
| **Fix** | Replace the duplicate with 0.897: [`NUM-20_swmm530.patch`](NUM-20_swmm530.patch), [`NUM-20_swmm600.patch`](NUM-20_swmm600.patch) |

## The problem

Standard arch pipes are given by a size code (Geom3 in `[XSECTIONS]`), and SWMM takes their rise, span, full area and full hydraulic radius from four tables. Codes 30 to 44 are corrugated-steel pipe arches with 3 x 1 in corrugations. Codes 30, 31 and 32 are the same shape at three scales:

| Code | Rise x span (in) | Span / rise | Afull (ft²) | A / (rise x span) | Rfull in the table (ft) |
|---|---|---|---|---|---|
| 30 | 31 x 40 | 1.29 | 7.0 | 0.813 | 0.773 |
| 31 | 36 x 46 | 1.28 | 9.4 | 0.817 | **0.773** |
| 32 | 41 x 53 | 1.29 | 12.3 | 0.815 | 1.022 |

For similar shapes the hydraulic radius grows in proportion to the size, so code 31 must have a larger Rfull than code 30. It has the same one. Every other entry in the group is 0.2991 x rise to three decimals (the same ratio `xsect_setParams()` uses for non-standard arches), which gives 0.897 ft for code 31.

The tables show how it happened. Before release 5.1.008 the rise table read `31,31,41,...`, i.e. code 31 was a copy of code 30 in both the rise and the hydraulic radius. 5.1.008 corrected the rise and marked it `// 2nd value corrected`; the hydraulic radius two tables further down still reads `0.773,0.773,1.022`. The full area (9.4 ft²) was never duplicated.

`xsect_setParams()` sets `rFull` and `sFull = aFull * rFull^(2/3)` from the table. `xsect_getRofY()` returns `rFull` times the relative R_Arch table, and `xsect_getSofA()` computes A R^(2/3) from that same R, so the 14 % error in Rfull scales the hydraulic radius at every depth: conveyance is (0.773 / 0.897)^(2/3) = 0.906 of what it should be, in kinematic wave, dynamic wave, normal-flow limiting and the full-flow capacity.

## Why it happens

```c
// src/legacy/engine/xsect.dat
     /* Corrugated Steel (3 x 1 inch Corrugation) */
     31,36,41,46,51,55,59,63,67,71,75,79,83,87,91,  // 2nd value corrected
...
double Rfull_Arch[102] =
{    0.25,0.3,0.36,0.45,0.56,0.68,0.8,0.9,1.01,1.13,1.35,
     1.57,1.77,1.92,2.17,2.42,2.65,0.324,0.374,0.449,0.499,0.598,0.723,
     0.823,0.947,1.072,1.171,1.296,1.421,0.773,0.773,1.022,1.147,1.271,
```

```c
// src/legacy/engine/xsect.c, xsect_setParams(), case ARCH
            xsect->aFull = Afull_Arch[index];
            xsect->rFull = Rfull_Arch[index];
        ...
        xsect->sFull = xsect->aFull * pow(xsect->rFull, 2./3.);
```

6.0.0's `xsect_tables.hpp` is a copy of the same table.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-20_arch-codes.inp`](NUM-20_arch-codes.inp) | Three 1000 ft conduits, n = 0.024, slope 0.001, standard arch codes 30, 31 and 32 |
| [`NUM-20_test.c`](NUM-20_test.c) | Legacy toolkit (5.2.4, 5.3.0): reads each conduit's full-flow capacity, backs out Rfull with the tabulated area, and checks that Rfull/rise of code 31 is within 3 % of codes 30 and 32 |
| [`NUM-20_test6.c`](NUM-20_test6.c) | 6.0.0: the same check, reading Yfull, Afull and Rfull of each link's section directly (`swmm_link_create_xsect()`, `swmm_xsect_full_properties()`) |

```sh
tools/run-test.sh NUM-20            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-20 --patched  # 5.3.0, 6.0.0: PASS
```

**Without the fix** (5.2.4 and 5.3.0 print the same; 6.0.0 the same without the Qfull column):

```
Code  Rise x span (in)  Afull (ft2)  A/(rise*span)  Qfull (cfs)  Rfull (ft)  Rfull/rise
  30      31 x 40            7.0         0.813        11.54       0.773       0.299
  31      36 x 46            9.4         0.817        15.50       0.773       0.258
  32      41 x 53           12.3         0.815        24.44       1.022       0.299
Code 31: Rfull/rise 0.258 vs 0.299 for codes 30 and 32 (-13.9 %); Rfull 0.773 ft, 0.898 ft by the same ratio
FAIL: code 31 has Rfull = 0.773 ft, 14 % below the 0.898 ft its shape implies; Qfull 15.50 cfs
NUM-20 5.2.4 base: FAIL
NUM-20 5.3.0 base: FAIL
NUM-20 6.0.0 base: FAIL
```

**With the fix**:

```
Code  Rise x span (in)  Afull (ft2)  A/(rise*span)  Qfull (cfs)  Rfull (ft)  Rfull/rise
  30      31 x 40            7.0         0.813        11.54       0.773       0.299
  31      36 x 46            9.4         0.817        17.12       0.897       0.299
  32      41 x 53           12.3         0.815        24.44       1.022       0.299
Code 31: Rfull/rise 0.299 vs 0.299 for codes 30 and 32 (-0.1 %); Rfull 0.897 ft, 0.898 ft by the same ratio
PASS: codes 30-32 have the same Rfull/rise, as similar shapes must
NUM-20 5.3.0 patched: PASS
NUM-20 6.0.0 patched: PASS
```

## The fix

```diff
-     0.823,0.947,1.072,1.171,1.296,1.421,0.773,0.773,1.022,1.147,1.271,
+     0.823,0.947,1.072,1.171,1.296,1.421,0.773,0.897,1.022,1.147,1.271,
```

The 6.0.0 patch changes the same entry in `xsect_tables.hpp`; both patched engines report Rfull = 0.897 ft. Only conduits with size code 31 change: their capacity rises by 10.4 % at every depth.

0.897 ft follows the table's own rule. Manufacturers' published values for pipe arches of this corrugation are a few percent below 0.2991 x rise (for example 1.104 ft for the 60 x 46 in size, code 33, in the [Contech HEL-COR pipe-arch table](https://www.conteches.com/media/4xllkjs1/hel-cor-pipe-arch-bro.pdf), where SWMM has 1.147 ft); 46 x 36 in is not in the catalogues checked. Either way, 0.773 ft is 10 to 14 % too small for this size.

The reference manual's Table E-1 should get the same correction. Its headings also give the full area and hydraulic radius in in² and in, while the values are in ft² and ft.

**Effect on other models.** One of the 73 regression decks in `regsuite` uses an ARCH conduit, with size code 58, so no regression result changes.
