# NUM-23: In SI units a transect width factor can make SWMM miss the bank stations

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | In an SI model, a transect with a width factor (Wfactor) other than 0, 1 or a power of 2 often loses its overbank/channel split: the whole section is computed as one subsection with the right overbank's n. In the test the full flow drops by 66 % (37.19 instead of 109.85 m³/s) and the depth at 5 m³/s nearly doubles (0.800 instead of 0.411 m). No warning, although the manual promises one when a bank station does not match a station. |
| **Reached from** | `[TRANSECTS]` in a model with SI flow units (CMS, LPS, MLD), where the X1 line has a Wfactor and bank stations that do not survive the two roundings alike (with Wfactor 1.5, 41 of the whole-number stations 1 to 100 m) |
| **5.3.0** | `setParams()` in [`src/legacy/engine/transect.c:380`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L380) scales the banks differently from `addStation()` at [`transect.c:403`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L403); `getFlow()` compares them with `==` at [`transect.c:567`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L567) |
| **5.2.4** | Same code, [`src/solver/transect.c:351`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/transect.c#L351) and [`transect.c:374`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/transect.c#L374) |
| **6.0.0** | Reproduces: `transect::buildFromStore()` copies both expressions for parity ([`Transect.cpp:453`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Transect.cpp#L453)-[`456`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Transect.cpp#L456)) and its `getFlow` compares with `==` ([`Transect.cpp:112`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Transect.cpp#L112)) |
| **Since** | 5.0.010, which applied the width factor to the bank stations |
| **Fix** | Scale the banks with the same expression as the stations: [`NUM-23_swmm530.patch`](NUM-23_swmm530.patch), [`NUM-23_swmm600.patch`](NUM-23_swmm600.patch) |

## The problem

A transect's X1 line gives the stations that end the left overbank and begin the right overbank, and SWMM computes the conveyance of the two overbanks and the channel separately, each with its own n. The optional Wfactor multiplies all station distances, including the bank stations, to widen or narrow the section.

In the test, T1 has stations 0/10/30/40 m, banks at 10 and 30 m, n = 0.10/0.10/0.03 and Wfactor 1.5. That is exactly the section T2 describes directly: stations 0/15/45/60 m with banks at 15 and 45 m. By the input manual the two are the same transect, and the analytic full flow, the sum of the Manning flows of the two overbank triangles and the 30 m channel at 2 m depth, is 109.83 m³/s at slope 0.001. 5.2.4, 5.3.0 and 6.0.0 give T2 109.85 m³/s, but T1 only 37.19 m³/s:

```
Conduit  Transect                       Full flow (m3/s)  Depth at 5 m3/s (m)
C1       T1 0/10/30/40 m, Wfactor 1.5          37.19            0.800
C2       T2 0/15/45/60 m                      109.85            0.411
```

The test's report shows a full hydraulic radius of 0.25 m for T1 against 1.25 m for T2 in its Cross Section Summary. T1 is computed as a single subsection with the right overbank's n of 0.10, so the channel's lower n is lost.

## Why it happens

SWMM works in feet internally. The stations are multiplied by the width factor and then converted; the bank stations are converted first and then multiplied:

```c
// src/legacy/engine/transect.c, addStation()
    Station[Nstations] = x * Xfactor / UCF(LENGTH);

// src/legacy/engine/transect.c, setParams()
    Xleftbank = x[3] / UCF(LENGTH);              // left overbank location
    Xrightbank = x[4] / UCF(LENGTH);             // right overbank location
    ...
    Xleftbank *= Xfactor;                        // adjusted left bank
    Xrightbank *= Xfactor;                       // adjusted right bank
```

In US units `UCF(LENGTH)` is 1 and the order does not matter. In SI it is 0.3048, and `10 * 1.5 / 0.3048` and `(10 / 0.3048) * 1.5` differ in the last bit. `getFlow()`, which decides where one subsection ends and the next begins, looks for the banks with exact equality and picks n from the slice positions:

```c
// src/legacy/engine/transect.c, getFlow()
        else if ( Station[k] == Xleftbank )
        ...
        else if ( Station[k] == Xrightbank )
        ...
        n = Nchannel;
        if ( Station[k-1] < Xleftbank ) n = Nleft;
        if ( Station[k] > Xrightbank )  n = Nright;
```

When neither bank matches, the only subsection boundary left is the last station, and the whole section is evaluated there with the right overbank's n. For Wfactor 1.5 this happens for 41 of the station values 1, 2, ..., 100 m; for 1.2, 43; for 0.9, 41; for 2.0 or 0.5, none. 6.0.0 reproduces both expressions on purpose, with a "PARITY" comment.

The manual says that if a bank station does not match a station, "a warning will be issued and the program will assume that no overbank area exists". Neither engine issues that warning.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-23_si-wfactor.inp`](NUM-23_si-wfactor.inp) | Two 300 m conduits at slope 0.001 (CMS): T1 with Wfactor 1.5 and T2 with the stations pre-multiplied, 5 m³/s each, dynamic wave |
| [`NUM-23_test.c`](NUM-23_test.c) | Legacy toolkit (5.2.4, 5.3.0): compares both full flows with the analytic subsection sum (tolerance 2 %) and the steady depths at J1 and J2 (tolerance 1 %) |
| [`NUM-23_test6.c`](NUM-23_test6.c) | 6.0.0: the same checks; the full flows are read from the report's Cross Section Summary |

```sh
tools/run-test.sh NUM-23            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-23 --patched  # 5.3.0, 6.0.0: PASS
```

**Without the fix** (5.3.0; 6.0.0 prints the same numbers, 5.2.4 gives 37.09 and 109.55 m³/s because of its 1.49 constant, see [NUM-57](../NUM-57-transect-radius-1-49-524/)):

```
Conduit  Transect                       Full flow (m3/s)  Depth at 5 m3/s (m)
C1       T1 0/10/30/40 m, Wfactor 1.5          37.19            0.800
C2       T2 0/15/45/60 m                      109.85            0.411
Analytic full flow: 109.83 m3/s
FAIL: with Wfactor 1.5 the full flow is 37.19 m3/s (-66 % from 109.83) and the depth 0.800 m vs 0.411 m for the same stations entered directly
NUM-23 5.2.4 base: FAIL
NUM-23 5.3.0 base: FAIL
NUM-23 6.0.0 base: FAIL
```

**With the fix** (both patched engines print the same):

```
Conduit  Transect                       Full flow (m3/s)  Depth at 5 m3/s (m)
C1       T1 0/10/30/40 m, Wfactor 1.5         109.85            0.411
C2       T2 0/15/45/60 m                      109.85            0.411
Analytic full flow: 109.83 m3/s
PASS: the bank stations are found and Wfactor gives the same section as scaled stations
NUM-23 5.3.0 patched: PASS
NUM-23 6.0.0 patched: PASS
```

## The fix

Compute the bank stations with the same expression as the stations, so a bank that equals a station as entered also equals it after scaling:

```diff
-    Xleftbank *= Xfactor;                        // adjusted left bank
-    Xrightbank *= Xfactor;                       // adjusted right bank
+    Xleftbank = x[3] * Xfactor / UCF(LENGTH);    // adjusted left bank
+    Xrightbank = x[4] * Xfactor / UCF(LENGTH);   // adjusted right bank
```

The 6.0.0 patch makes the same change in `transect::buildFromStore()` and corrects its parity comment. In US units, and in SI without a width factor, the bank values are bit-identical to before.

The patch does not add the warning the manual describes for a bank station that matches no station; such a transect is still computed as one subsection with the right overbank's n, rather than as having no overbanks.

**Effect on other models.** None of the six regression decks in `regsuite` with transects uses a width factor. The patched 5.3.0 and 6.0.0 command-line programs give the same report (apart from run times) and byte-identical output files as the unpatched ones for `CoS-Reduced-Outlets.inp` (CMS, 10 transects) and `user2.inp` (CFS, 43 transects).
