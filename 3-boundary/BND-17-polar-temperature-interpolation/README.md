# BND-17: Temperatures interpolated from daily Tmax/Tmin jump at sunrise and never reach Tmax in polar winter

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | With climate-file temperatures, on days shorter than 3 h (about 65 to 90 degrees latitude around the winter solstice) the interpolated air temperature falls to Tmin at sunrise, jumps about 16 F within one step and never reaches the day's Tmax. At 70 N on Dec 20-21 with Tmax 40 F and Tmin 20 F, the highest value is 36.58 F and the temperature changes by 16.39 F in 10 minutes. 6.0.0 steps from Tmin to Tmax at noon instead (19.78 F in 10 minutes). Snowmelt and the snow/rain split near the divide temperature see the wrong temperature. There is no warning. |
| **Reached from** | `[TEMPERATURE] FILE` (daily Tmax/Tmin from a climate file) with a `SNOWMELT` latitude above about 64.9 degrees, north or south, in the weeks around the winter solstice. Days of 3 to 4 h (from about 63.4 degrees) reach Tmax but rise from Tmin to Tmax in less than an hour. |
| **5.3.0** | `updateTempTimes()` in [`src/legacy/engine/climate.c:1328`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/climate.c#L1328), used by `setTemp()` at [`:1190`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/climate.c#L1190) |
| **5.2.4** | Same code, [`src/solver/climate.c:967`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/climate.c#L967) |
| **6.0.0** | Reproduces, with different numbers: `climate::updateTempTimes()` puts sunrise and the maximum both at noon when the sun does not rise ([`src/engine/hydrology/Climate.cpp:196`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Climate.cpp#L196)) and copies the legacy formula for short days ([`:207`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Climate.cpp#L207)) |
| **Since** | Every release (the code is in the first commit of the EPA repository, 5.1) |
| **Fix** | Keep the hour of the maximum at least 1 h after sunrise: [`BND-17_swmm530.patch`](BND-17_swmm530.patch), [`BND-17_swmm600.patch`](BND-17_swmm600.patch) |

## The problem

When air temperatures come from a climate file, SWMM has only the daily maximum and minimum. It turns them into a temperature at any time of day by assuming that Tmin occurs at sunrise and Tmax 3 h before sunset, and joining these points with sine curves (Vol. I, Section 2.4, Eq. 2-5 to 2-9). The manual says the sunrise formula is valid between the polar circles; the code clamps it so that polar latitudes are accepted, with a day length of zero when the sun does not rise.

The scheme needs the time of Tmax to come after sunrise. That fails when the day is shorter than 3 h: "3 h before sunset" is then before sunrise. At 70 N in late December the sun does not rise at all, sunrise and sunset are both put at noon and Tmax is placed at 9:00. The rising part of the curve is never used. The temperature falls to Tmin at noon, jumps up in the next step and then falls again. Tmax is never reached.

The test decks use a climate file with Tmax 40 F and Tmin 20 F every day and a longitude correction of -30 min. At 40 N the curve is smooth and covers 20 to 40 F. At 70 N, 5.2.4 and 5.3.0 give:

```
  Day      Highest (at)       Lowest   Largest 10-min change (at)
  Dec 20   36.58 F (11.67 h)   20.19 F   16.39 F (11.67 h)
```

The day's maximum is 3.4 F too low, and the 16.39 F jump at sunrise is about as large as the whole diurnal range. The same happens on every day shorter than 3 h, that is at latitudes above about 64.9 degrees (north or south) in the weeks around the winter solstice, for example Tromsø or northern Alaska, Canada and Russia. On days of 3 to 4 h (above about 63.4 degrees) Tmax is reached, but the rise from Tmin to Tmax takes less than an hour. At 64.5 N on Dec 20-21 (same deck, latitude changed) the temperature rises 9.96 F in one 10-minute step.

Snowmelt uses this temperature: the divide temperature decides whether precipitation falls as snow, melt is computed from the temperature above the base temperature, and the antecedent temperature index follows it. A wrong maximum and a jump at noon change all three.

## Why it happens

`updateTempTimes()` computes the hour of sunrise `Hrsr` (time of Tmin) and the hour 3 h before sunset `Hrss` (time of Tmax):

```c
// src/legacy/engine/climate.c, updateTempTimes()
    decl = 0.40928 * cos(0.017202 * (172.0 - day));
    arg = -tan(decl) * Temp.tanAnglat;
    if (arg <= -1.0)
        arg = PI;
    else if (arg >= 1.0)
        arg = 0.0;
    else
        arg = acos(arg);
    hrang = 3.8197 * arg;
    Hrsr = 12.0 - hrang + Temp.dtlong;
    Hrss = 12.0 + hrang + Temp.dtlong - 3.0;
    Dhrdy = Hrsr - Hrss;
    Dydif = 24.0 + Hrsr - Hrss;
```

The half-day length `hrang` is below 1.5 h on a day shorter than 3 h, and 0 when the sun does not rise. Then `Hrss < Hrsr`. `setTemp()` picks one of three curves by the hour of day:

```c
// src/legacy/engine/climate.c, setTemp()
        if (hour < Hrsr)
            Temp.ta = Tmin + Trng1 / 2.0 * sin(PI / Dydif * (Hrsr - hour));
        else if (hour >= Hrsr && hour <= Hrss)
            Temp.ta = Tave + Trng * sin(PI / Dhrdy * (Hrday - hour));
        else
            Temp.ta = Tmax - Trng * sin(PI / Dydif * (hour - Hrss));
```

With `Hrss < Hrsr` the middle branch, the only one that rises to Tmax, can never be taken. Before sunrise the first curve falls to Tmin. After sunrise the third curve starts at `Tmax - Trng*sin(PI*(Hrsr - Hrss)/Dydif)`, which at 70 N with Tmax 40 F and Tmin 20 F is 36.58 F, and falls from there.

6.0.0 handles the case where the sun does not rise separately, with `hrsr = 12.0; hrss = 12.0;`. The middle curve then lasts for an instant and the temperature steps from Tmin to Tmax at noon (19.78 F in one step). This branch also drops the longitude correction, so 6.0.0 does not match legacy here. Days shorter than 3 h when the sun does rise go through the legacy formula and behave as in legacy.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-17_lat70-december.inp`](BND-17_lat70-december.inp) | Climate-file temperatures at 70 N, longitude correction -30 min, Dec 20-21 2019, report step 10 min |
| [`BND-17_lat40-december.inp`](BND-17_lat40-december.inp) | The same at 40 N (control) |
| [`BND-17_clim.txt`](BND-17_clim.txt) | User climate file with Tmax 40 F and Tmin 20 F every day of December 2019 |
| [`BND-17_test.c`](BND-17_test.c) | Runs both decks through the legacy toolkit and reads the air temperature from the binary output every 10 minutes. For each day it expects a highest value >= 39.5 F, a lowest value <= 20.5 F and no change above 10 F (half the daily range) between two consecutive values. |
| [`BND-17_test6.c`](BND-17_test6.c) | The same check for 6.0.0, reading `swmm_climate_get_temperature()` after every 10-minute step |

```sh
tools/run-test.sh BND-17            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh BND-17 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print:

```
BND-17_lat40-december.inp
  Day      Highest (at)       Lowest   Largest 10-min change (at)
  Dec 20   39.99 F (13.17 h)   20.01 F    0.85 F (10.17 h)
  Dec 21   39.99 F (13.17 h)   20.01 F    1.13 F ( 0.17 h)
BND-17_lat70-december.inp
  Day      Highest (at)       Lowest   Largest 10-min change (at)
  Dec 20   36.58 F (11.67 h)   20.19 F   16.39 F (11.67 h)
  Dec 21   36.58 F (11.67 h)   20.19 F   16.39 F (11.67 h)
FAIL: the interpolated temperature at latitude 70 does not reach Tmax 40 F or Tmin 20 F each day, or changes by more than 10 F in 10 minutes
BND-17 5.3.0 base: FAIL
```

and 6.0.0 prints the same for 40 N and, for 70 N:

```
BND-17_lat70-december.inp
  Day      Highest (at)       Lowest   Largest 10-min change (at)
  Dec 20   40.00 F (12.17 h)   20.22 F   19.78 F (12.17 h)
  Dec 21   40.00 F (12.17 h)   20.22 F   19.78 F (12.17 h)
FAIL: the interpolated temperature at latitude 70 does not reach Tmax 40 F or Tmin 20 F each day, or changes by more than 10 F in 10 minutes
BND-17 6.0.0 base: FAIL
```

**With the fix**, 5.3.0 and 6.0.0 print the same:

```
BND-17_lat40-december.inp
  Day      Highest (at)       Lowest   Largest 10-min change (at)
  Dec 20   39.99 F (13.17 h)   20.01 F    0.85 F (10.17 h)
  Dec 21   39.99 F (13.17 h)   20.01 F    1.13 F ( 0.17 h)
BND-17_lat70-december.inp
  Day      Highest (at)       Lowest   Largest 10-min change (at)
  Dec 20   40.00 F (12.67 h)   20.00 F    5.00 F (12.17 h)
  Dec 21   40.00 F (12.67 h)   20.00 F    5.00 F (12.17 h)
PASS: at latitudes 40 and 70 the interpolated temperature reaches Tmax and Tmin each day and changes smoothly
BND-17 5.3.0 patched: PASS
BND-17 6.0.0 patched: PASS
```

The times are those of the report periods; each value is the temperature of the runoff step that starts 10 minutes earlier. The 1.13 F change at midnight at 40 N comes from the way the scheme joins the evening and morning curves at midnight and is the same in all versions.

## The fix

5.3.0, in `updateTempTimes()`, keep the time of Tmax at least 1 h after sunrise:

```diff
     Hrsr = 12.0 - hrang + Temp.dtlong;
     Hrss = 12.0 + hrang + Temp.dtlong - 3.0;
+
+    // --- for days shorter than 4 h (polar winter) the max. temp. hour
+    //     would come less than 1 h after, or even before, the min. temp.
+    //     hour at sunrise; keep it 1 h after sunrise
+    if (Hrss < Hrsr + 1.0)
+        Hrss = Hrsr + 1.0;
```

On a day without sunrise this puts Tmin at solar noon and Tmax 1 h later. The evening and morning curves then each span about 11.5 h and meet at midnight, so the whole day is continuous, and the rise to Tmax takes 1 h (at most 5.00 F per 10 minutes in the test). The 1 h is a choice. It is the shortest rise that stays smooth at typical runoff steps, and it changes only days shorter than 4 h.

6.0.0 gets the same rule in `climate::updateTempTimes()`. Its polar-night branch now uses a zero half-day length with the longitude correction, as legacy does, so that both patched engines give the same temperatures:

```diff
     if (arg >= 1.0) {
-        // Polar night — sun never rises; use noon for both
-        hrsr = 12.0;
-        hrss = 12.0;
+        // Polar night — sun never rises: zero half-day length (legacy)
+        hrsr = 12.0 + state.dtlong;
+        hrss = 12.0 + state.dtlong - 3.0;
     } else if (arg <= -1.0) {
 ...
+    // Days shorter than 4 h (polar winter): the max temp hour would come less
+    // than 1 h after, or even before, sunrise; keep it 1 h after sunrise.
+    if (hrss < hrsr + 1.0) hrss = hrsr + 1.0;
```

Effect on other models: results change only for climate-file temperatures on days shorter than 4 h, which occur only above about 63.4 degrees latitude. The 40 N control deck gives the same values before and after the fix. None of the regression decks uses temperature data.

Not changed here: in polar summer, when the sun does not set, sunrise is put at midnight and the evening curve ends back at Tmax, so the temperature drops from about Tmax to Tmin at midnight (18.26 F in one step at 70 N at midnight between Jun 20 and 21 in 5.3.0). The same seam at midnight exists at every latitude and grows with the length of the day (1.13 F at 40 N in the test). It comes from where the scheme switches curves rather than from the polar clamp, and is reported separately.
