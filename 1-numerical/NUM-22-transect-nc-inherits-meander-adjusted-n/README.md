# NUM-22: An NC line of zeros inherits the previous transect's meander-adjusted n

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A transect whose NC line says "no change" (0) for the channel n gets the previous transect's channel n multiplied by the square root of that transect's meander factor. In the test (meander factor 2.0 on the previous transect) the full flow drops by 25 % (404.22 instead of 538.37 cfs) and the depth at 20 cfs rises from 0.995 to 1.219 ft. With several meandering transects in a row the factors compound. No warning. |
| **Reached from** | `[TRANSECTS]`: an `NC` line with Nchanl = 0 after a transect whose X1 line has a meander factor other than 0 or 1 |
| **5.3.0** | `transect_validate()` in [`src/legacy/engine/transect.c:232`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L232) adjusts the file-scope `Nchannel` in place; `setManning()` at [`transect.c:354`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/transect.c#L354) keeps it for a zero entry |
| **5.2.4** | Same code, [`src/solver/transect.c:230`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/transect.c#L230) and [`transect.c:326`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/transect.c#L326) |
| **6.0.0** | Not affected: the parser keeps the NC values as entered ([`LinksHandler.cpp:560`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/LinksHandler.cpp#L560)) and the meander adjustment is applied to a local copy per transect ([`Transect.cpp:58`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Transect.cpp#L58)) |
| **Since** | 5.0.015, which added the meander adjustment |
| **Fix** | Restore the unadjusted n at the end of `transect_validate()`: [`NUM-22_swmm530.patch`](NUM-22_swmm530.patch) |

## The problem

The input manual defines the `NC` line of a transect as `NC Nleft Nright Nchanl`, with "use 0 if no change from previous NC line" for each value. The X1 line's meander factor `Lfactor` is the ratio of the main channel's length to the overbank length; SWMM uses the shorter overbank length and raises the main channel's n by sqrt(Lfactor) to compensate. That adjustment belongs to the one transect whose X1 line carries it.

In 5.2.4 and 5.3.0 it leaks into the next transect. In the test, T1 has n = 0.05 / 0.05 / 0.03 and a meander factor of 2.0. T2 follows with `NC 0 0 0` and no meander factor, so by the manual it has n = 0.05 / 0.05 / 0.03. T3 is the same transect with those values written out. All three have the same stations and elevations. The analytic full flow of T2 and T3, the sum of the Manning flows of the two overbanks and the channel at full depth, is 538.37 cfs:

```
Conduit  Transect                 Full flow (cfs)  Depth at 20 cfs (ft)
C1       T1 meander 2.0                571.65           1.219
C2       T2 NC 0 0 0 (inherits)        404.22           1.219
C3       T3 NC 0.05 0.05 0.03          538.37           0.995
```

T2 behaves as if its channel n were 0.03 x sqrt(2) = 0.0424 and its meander factor were 1. Its full flow is 25 % low and its depth at 20 cfs is 23 % higher than T3's. The conduit's roughness is also set to 0.0424, so every quantity that uses it (normal flow, the Cross Section Summary, capacity) carries the error. If T3 had also said `NC 0 0 0` and T2 had its own meander factor, T3 would get both factors.

## Why it happens

The channel n is held in a file-scope variable while the transects are read. `transect_validate()` builds the current transect's tables with the adjusted value and leaves the variable adjusted:

```c
// src/legacy/engine/transect.c, transect_validate()
    double oldNchannel = Nchannel;
    ...
    // --- adjust main channel's Mannings n to make its equivalent
    //     length equal to that of entire flood plain
    Nchannel = Nchannel * sqrt(Lfactor);
    ...
    createTables(&Transect[j], ymin, ymax);

    // --- save unadjusted main channel roughness 
    Transect[j].roughness = oldNchannel;
}
```

The next NC line is read by `setManning()`, which only overwrites a value that is positive:

```c
// src/legacy/engine/transect.c, setManning()
    if ( n[3] > 0.0 ) Nchannel = n[3];
```

So `NC 0 0 0` keeps the adjusted value. The next `transect_validate()` takes it as `oldNchannel`, multiplies it by its own sqrt(Lfactor), and stores it as the transect's roughness, which `link.c:1024` copies into the conduit.

6.0.0 stores the NC values per transect as entered and applies `nChannel *= std::sqrt(lFactor)` to a local variable when it builds each transect's tables.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-22_transects.inp`](NUM-22_transects.inp) | Three 1000 ft conduits at slope 0.001 with transects T1 (meander 2.0), T2 (`NC 0 0 0`) and T3 (n written out), 20 cfs each, dynamic wave |
| [`NUM-22_test.c`](NUM-22_test.c) | Legacy toolkit (5.2.4, 5.3.0): compares the full flow of C2 and C3 with the analytic subsection sum (tolerance 2 %), and the steady depths at J2 and J3 (tolerance 1 %) |
| [`NUM-22_test6.c`](NUM-22_test6.c) | 6.0.0: the same checks; the full flows are read from the report's Cross Section Summary |

```sh
tools/run-test.sh NUM-22            # 5.2.4, 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh NUM-22 --patched  # 5.3.0: PASS (6.0.0 has no patch and still PASSes)
```

**Without the fix**, 5.3.0:

```
Conduit  Transect                 Full flow (cfs)  Depth at 20 cfs (ft)
C1       T1 meander 2.0                571.65           1.219
C2       T2 NC 0 0 0 (inherits)        404.22           1.219
C3       T3 NC 0.05 0.05 0.03          538.37           0.995
Analytic full flow with n = 0.05/0.05/0.03: 538.37 cfs
FAIL: T2 (NC 0 0 0) has full flow 404.22 cfs (-25 % from 538.37) and depth 1.219 ft vs 0.995 ft for the same n written out
NUM-22 5.3.0 base: FAIL
```

5.2.4 gives 403.13 cfs for C2 and 536.92 cfs for C3 (its transect tables use 1.49 instead of 1.486, see [NUM-57](../NUM-57-transect-radius-1-49-524/)), and also FAILs. 6.0.0 PASSes unpatched:

```
C2       T2 NC 0 0 0 (inherits)        538.37           0.995
C3       T3 NC 0.05 0.05 0.03          538.37           0.995
NUM-22 6.0.0 base: PASS
```

**With the fix**, 5.3.0 prints the same numbers as 6.0.0:

```
Conduit  Transect                 Full flow (cfs)  Depth at 20 cfs (ft)
C1       T1 meander 2.0                571.65           1.219
C2       T2 NC 0 0 0 (inherits)        538.37           0.995
C3       T3 NC 0.05 0.05 0.03          538.37           0.995
Analytic full flow with n = 0.05/0.05/0.03: 538.37 cfs
PASS: NC 0 0 0 reuses the n values as written, not the meander-adjusted ones
NUM-22 5.3.0 patched: PASS
```

## The fix

```diff
     // --- save unadjusted main channel roughness 
     Transect[j].roughness = oldNchannel;
+
+    // --- restore it so that a following NC line with a zero channel n
+    //     inherits the value as entered, not the meander-adjusted one
+    Nchannel = oldNchannel;
 }
```

The current transect's tables and roughness are built exactly as before; only the value that a later `NC` line with a zero channel n inherits changes. Transects that follow a transect with no meander factor are unaffected.

**Effect on other models.** None of the six regression decks in `regsuite` with transects uses a meander factor, so their results do not change.
