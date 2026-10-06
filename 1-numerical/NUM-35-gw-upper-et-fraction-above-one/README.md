# NUM-35: An upper-zone ET fraction above 1 makes lower-zone ET negative

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | When an aquifer's effective upper-zone ET fraction exceeds 1, lower-zone ET turns negative and adds water to the saturated zone. In the test, 10 days of June with evaporation 0.2 in/day: Lower Zone ET -0.362 in and a final aquifer storage 0.362 in too high. Groundwater continuity still shows 0.000 %, so the only sign is the negative ET line in the report. |
| **Reached from** | `[AQUIFERS]` with a monthly ETu pattern whose factor times ETu exceeds 1 (all three engines), or with ETu > 1 entered directly (5.2.4 and 5.3.0 accept it; 6.0.0 rejects it) |
| **5.3.0** | `getEvapRates()` scales ETu by the pattern factor without a limit, [`src/legacy/engine/gwater.c:749`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L749), and computes `(1 - upperFrac)`, [`gwater.c:772`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L772); `gwater_validateAquifer()` checks only ETu < 0, [`gwater.c:355`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L355) |
| **5.2.4** | Same code, [`src/solver/gwater.c:739`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gwater.c#L739), [`:762`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gwater.c#L762) and [`:343`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gwater.c#L343) |
| **6.0.0** | Reproduces with a pattern: validation rejects ETu > 1 ([`src/engine/core/SWMMEngine.cpp:9062`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L9062)), but the pattern-adjusted fraction is not limited ([`src/engine/hydrology/Groundwater.cpp:297`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Groundwater.cpp#L297), [`:142`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Groundwater.cpp#L142)) |
| **Since** | 5.1.000, which added the monthly ETu pattern; ETu > 1 entered directly has never been rejected by the legacy engine |
| **Fix** | Cap the pattern-adjusted fraction at 1, and in 5.3.0 also reject ETu > 1 at input: [`NUM-35_swmm530.patch`](NUM-35_swmm530.patch), [`NUM-35_swmm600.patch`](NUM-35_swmm600.patch) |

## The problem

The input reference defines ETu as the "fraction of total evaporation available for evapotranspiration in the upper unsaturated zone", and the optional pattern as a "monthly time pattern used to adjust the upper zone evaporation fraction". Vol I sec. 5.3.3 gives lower-zone ET as

f<sub>EL</sub> = (1 - UEF) e<sub>max</sub> (DEL - d<sub>U</sub>) / DEL

and states that it "is constrained to be non-negative". Nothing stops UEF from going above 1, though. A pattern that raises the upper-zone share in summer (ETu 0.5 with a June factor of 3) does it, and so does an ETu typed as 1.5.

The test deck is a 1-acre subcatchment, 50 % impervious, over an aquifer whose water table is 4 ft below the surface and whose lower-zone ET depth is 14 ft. It rains 0.5 in in the first hour of 1 June, then the soil dries for 10 days at 0.2 in/day of evaporation. With an effective fraction of 1.5, all three engines report:

```
  Upper Zone ET ............         0.083         0.991
  Lower Zone ET ............        -0.030        -0.362
  Final Storage ............         4.168        50.021
  Continuity Error (%) .....         0.000
```

The upper zone takes all the available evaporation (0.991 in of 1.0 in), which is right. The lower zone then "evaporates" -0.362 in: 0.362 in of water is created in the saturated zone, and the water table ends higher than it should. Because the negative ET is booked as an outflow, the groundwater continuity error stays at 0.000 %.

## Why it happens

```c
// src/legacy/engine/gwater.c, getEvapRates()
    upperFrac *= f;                                   // pattern factor, no limit
    ...
        UpperEvap = upperFrac * MaxEvap;
        UpperEvap = MIN(UpperEvap, AvailEvap);        // limited, so still correct
    ...
        LowerEvap = lowerFrac * (1.0 - upperFrac) * MaxEvap;   // < 0 if upperFrac > 1
        LowerEvap = MIN(LowerEvap, (AvailEvap - UpperEvap));   // does not stop a negative value
```

`getDxDt()` subtracts `LowerEvap` from the lower-zone balance, so a negative rate is a source. `gwater_validateAquifer()` rejects `upperEvapFrac < 0.0` but not values above 1.

6.0.0 rejects ETu > 1 in its aquifer validation (ERROR 109), but `GWSolver::execute()` multiplies by the pattern factor and passes the result on unchanged, and `getFluxes()` uses the same `(1.0 - c.upper_evap_frac)` formula.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-35_monthly-pattern.inp`](NUM-35_monthly-pattern.inp) | ETu 0.5 with a June pattern factor of 3.0 (valid input); the trailing `*` in `[GROUNDWATER]` works around IO-15 |
| [`NUM-35_etu-1.5.inp`](NUM-35_etu-1.5.inp) | The same with ETu 1.5 and no pattern |
| [`NUM-35_test.c`](NUM-35_test.c) | Runs both decks through the legacy toolkit and reads Upper and Lower Zone ET from the report. Passes if lower-zone ET is >= 0 (within the report's rounding of 0.0005 in), with ETu 1.5 either rejected or giving no negative ET |
| [`NUM-35_test6.c`](NUM-35_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-35            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-35 --patched  # 5.3.0, 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0:

```
Deck                         Upper Zone ET  Lower Zone ET
                                      (in)           (in)
ETu 0.5 x June factor 3.0            0.991         -0.362
ETu 1.5                              0.991         -0.362
FAIL: negative lower-zone ET adds water to the aquifer: -0.362 in with the ETu pattern, -0.362 in with ETu 1.5 (accepted as input)
```

6.0.0 (the error code 5 is the API's parse error; the report has `ERROR 109: invalid parameter values for Aquifer AQ.`):

```
ETu 0.5 x June factor 3.0            0.991         -0.362
ETu 1.5                      rejected, error 5
FAIL: negative lower-zone ET adds water to the aquifer: -0.362 in with the ETu pattern
```

**With the fix**, 5.3.0:

```
ETu 0.5 x June factor 3.0            0.991          0.000
ETu 1.5                      rejected, error 109
PASS: lower-zone ET is never negative (0.000 in with the ETu pattern; ETu 1.5 rejected as input)
```

and 6.0.0:

```
ETu 0.5 x June factor 3.0            0.991          0.000
ETu 1.5                      rejected, error 5
PASS: lower-zone ET is never negative (0.000 in with the ETu pattern; ETu 1.5 rejected as input)
```

Both patched engines report the same groundwater balance for the pattern deck: Upper Zone ET 0.991 in, Lower Zone ET 0.000 in, Final Storage 49.659 in (50.021 in before).

## The fix

5.3.0: reject ETu > 1 like 6.0.0 does, and cap the pattern-adjusted fraction:

```diff
     ||   Aquifer[aquiferIndex].upperEvapFrac     <  0.0
+    ||   Aquifer[aquiferIndex].upperEvapFrac     >  1.0
 ...
     upperFrac *= f;
+    upperFrac = MIN(upperFrac, 1.0);
```

6.0.0:

```diff
-        c.upper_evap_frac  = evap_frac;
+        c.upper_evap_frac  = std::min(evap_frac, 1.0);  // a pattern may push it above 1
```

Upper-zone ET does not change: it was already limited to the available evaporation, which never exceeds e<sub>max</sub>, so a fraction of 1 or 1.5 gives the same value. Lower-zone ET becomes 0 instead of negative, which is the bound Vol I states. For any effective fraction up to 1 the results are bit-identical. In 5.3.0 an input file with ETu > 1 now stops with ERROR 109, as it already does in 6.0.0. The only regression deck with an aquifer, `examples/Example5.inp`, has ETu 0.35 and no pattern, so it is not affected.
