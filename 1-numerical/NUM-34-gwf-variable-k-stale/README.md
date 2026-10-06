# NUM-34: The K variable in [GWF] expressions is another aquifer's conductivity

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | A custom groundwater flow expression that uses `K` gets the conductivity last computed for any aquifer above field capacity, often a different subcatchment's, or 0 if there was none. In the test a dry aquifer with `DEEP = K` loses 13.568 ac-ft in a day instead of 0.996 ac-ft (13.6 times too much); without the wet neighbour it loses nothing. Results depend on which other subcatchments exist and in what order. No warning. 6.0.0 gives 0.000 ac-ft. |
| **Reached from** | `[GWF]` LATERAL or DEEP expressions that use `K`, on an aquifer whose upper zone is at or below field capacity at any moment |
| **5.3.0** | `getUpperPerc()` sets the file-scope `HydCon` only above field capacity, [`src/legacy/engine/gwater.c:792`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L792) and [`:803`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L803); `getVariableValue()` returns it as `K`, [`gwater.c:874`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gwater.c#L874) |
| **5.2.4** | Same code, [`src/solver/gwater.c:793`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gwater.c#L793) and [`gwater.c:864`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gwater.c#L864) |
| **6.0.0** | Different and also wrong: no shared state, but `K` is set to the upper-zone percolation rate, which is 0 below field capacity and includes the tension gradient above it, [`src/engine/hydrology/Groundwater.cpp:168`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Groundwater.cpp#L168) and [`:217`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Groundwater.cpp#L217) |
| **Since** | 5.1.010, which added `HydCon` and the `K` variable |
| **Fix** | Compute K(θ) of the current aquifer for every evaluation: [`NUM-34_swmm530.patch`](NUM-34_swmm530.patch), [`NUM-34_swmm600.patch`](NUM-34_swmm600.patch) |

## The problem

The input reference lists `K` among the variables of a `[GWF]` expression as the "unsaturated hydraulic conductivity", and Vol I (eq. 5-19) defines it as a function of the upper-zone moisture content:

K(θ) = K<sub>s</sub> e<sup>-(φ - θ) HCO</sup>

That is defined for any θ. A deep-loss expression such as `DEEP K` or `DEEP 0.5*K` is a natural way to make seepage follow soil moisture.

The test deck has two 10-acre subcatchments and no rain or evaporation:

- S1: a wet aquifer (θ = 0.45, field capacity 0.30, K<sub>s</sub> = 5 in/hr), no expression;
- S2: a dry aquifer (θ = 0.20, below field capacity, K<sub>s</sub> = 1 in/hr, HCO = 10) with `[GWF] S2 DEEP K`.

S2's moisture content cannot change, so its K is 1.0 e<sup>-3</sup> = 0.0498 in/hr all day, and it should lose 1.195 in, or 0.996 ac-ft. 5.2.4 and 5.3.0 report 13.568 ac-ft of deep percolation, all of it from S2, because S2 is given the conductivity of S1 (about 0.68 in/hr, the value when S1 last drained above field capacity). Remove S1 and the same S2 loses 0.000 ac-ft, because `HydCon` then keeps its initial 0. 6.0.0 reports 0.000 ac-ft in both cases.

## Why it happens

`HydCon` is a file-scope variable shared by all subcatchments. Only `getUpperPerc()` writes it, and only after the early return for a dry upper zone:

```c
// src/legacy/engine/gwater.c, getUpperPerc()
    // --- no perc. from upper zone if no depth or moisture content too low
    if ( upperDepth <= 0.0 || theta <= A.fieldCapacity ) return 0.0;

    // --- compute hyd. conductivity as function of moisture content
    delta = theta - A.porosity;
    hydcon = A.conductivity * exp(delta * A.conductSlope);
    ...
    HydCon = hydcon;
    return hydcon * dhdz;

// getVariableValue()
    case gwvK:    return HydCon * UCF(RAINFALL);
```

`getFluxes()` evaluates the DEEP and LATERAL expressions right after `getUpperPerc()`, so for an aquifer at or below field capacity `K` is the value from the last wetter aquifer, from an earlier stage of the same ODE step, or from a previous time step.

6.0.0 builds the variable array per call, but fills `K` with the percolation rate:

```cpp
// src/engine/hydrology/Groundwater.cpp, getFluxes()
        vars[GWV_K]     = c.upper_perc > 0.0 ? c.upper_perc * c.ucf_rainfall : 0.0;
```

`upper_perc` is 0 below field capacity, and above it is K(θ) times the tension term `1 + 2 TS (θ - FC)/d_U`, capped by the available volume. That is the variable `Fu`, which the expression already has.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-34_dry-aquifer-deep-k.inp`](NUM-34_dry-aquifer-deep-k.inp) | S1 (wet aquifer) and S2 (dry aquifer, `DEEP K`), no rain, 24 hours |
| [`NUM-34_test.c`](NUM-34_test.c) | Runs the deck through the legacy toolkit and compares the report's Deep Percolation (all from S2) with K(θ) x 24 h x 10 ac, within 0.01 ac-ft |
| [`NUM-34_test6.c`](NUM-34_test6.c) | The same check through the 6.0.0 C API |

```sh
tools/run-test.sh NUM-34            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh NUM-34 --patched  # 5.3.0, 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0:

```
S2 K(theta) = 0.0498 in/hr
Deep percolation, expected     0.996 ac-ft
Deep percolation, reported    13.568 ac-ft (8.141 in over both subcatchments)
FAIL: DEEP = K on S2 percolates 13.568 ac-ft instead of 0.996 ac-ft (K of S2's own moisture content, 0.0498 in/hr)
NUM-34 5.2.4 base: FAIL
NUM-34 5.3.0 base: FAIL
```

6.0.0:

```
Deep percolation, reported     0.000 ac-ft (0.000 in over both subcatchments)
FAIL: DEEP = K on S2 percolates 0.000 ac-ft instead of 0.996 ac-ft (K of S2's own moisture content, 0.0498 in/hr)
NUM-34 6.0.0 base: FAIL
```

**With the fix**, both engines:

```
Deep percolation, reported     0.996 ac-ft (0.597 in over both subcatchments)
PASS: DEEP = K percolates 0.996 ac-ft, K(theta) of S2's aquifer
NUM-34 5.3.0 patched: PASS
NUM-34 6.0.0 patched: PASS
```

## The fix

5.3.0: compute `HydCon` for the current aquifer and moisture content in `getFluxes()`, before the expressions are evaluated, and drop the assignment in `getUpperPerc()`:

```diff
     getEvapRates(theta, upperDepth);
 
+    // --- unsaturated hyd. conductivity at current moisture content
+    //     (variable K of custom GW flow expressions)
+    HydCon = A.conductivity * exp((theta - A.porosity) * A.conductSlope);
+
     // --- find percolation rate from upper to lower zone
```

6.0.0: set `K` to the same value in both expression blocks:

```diff
-        vars[GWV_K]     = c.upper_perc > 0.0 ? c.upper_perc * c.ucf_rainfall : 0.0;
+        vars[GWV_K]     = c.k_sat * std::exp((theta - c.porosity) * c.k_slope) * c.ucf_rainfall;
```

Above field capacity the 5.3.0 value of `K` is bit-identical to before (same expression, same operands); only aquifers at or below field capacity change. The percolation rate itself is not touched. In 6.0.0 every expression that uses `K` changes, to the 5.3.0 value. None of the regression decks has a `[GWF]` section; `examples/Example5.inp` (groundwater, no expressions) gives identical reports with both patched engines.
