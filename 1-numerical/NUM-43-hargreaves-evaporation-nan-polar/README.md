# NUM-43: Temperature-based (Hargreaves) evaporation is NaN above the polar circles

| | |
|---|---|
| **Category** | [Numerical errors](../README.md) |
| **Impact** | Above about 66.5 degrees latitude, around both solstices, the evaporation rate is NaN. The NaN spreads to depression storage, so the subcatchment produces no runoff and the report shows `nan` for evaporation loss, final storage and the continuity error. At 70 N in early June, 0.5 in of rain gives 0.000 in of runoff instead of 0.268 in. Nothing warns the user apart from the `nan` in the report. |
| **Reached from** | `[EVAPORATION] TEMPERATURE` with a `[TEMPERATURE] FILE` and a `SNOWMELT` latitude above about 66.5 N or below 66.5 S, for any run that includes days of polar day or polar night |
| **5.3.0** | `getTempEvap()` in [`src/legacy/engine/climate.c:1355`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/climate.c#L1355) |
| **5.2.4** | Same code, [`src/solver/climate.c:998`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/climate.c#L998) |
| **6.0.0** | Fixed already: `climate::hargreaves()` clamps the argument before `acos()` ([`src/engine/hydrology/Climate.cpp:89`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/Climate.cpp#L89)), so its results differ from the legacy engine's NaN |
| **Since** | 5.0.016, when temperature-based evaporation was added |
| **Fix** | Clamp the cosine of the sunset hour angle to [-1, 1]: [`NUM-43_swmm530.patch`](NUM-43_swmm530.patch) |

## The problem

With `[EVAPORATION] TEMPERATURE`, SWMM computes a daily potential evaporation rate from the climate file's minimum and maximum temperatures with the Hargreaves equation. The equation needs the extraterrestrial radiation, and that needs the sunset hour angle at the site's latitude. Above the Arctic and Antarctic circles the sun does not set at all near the summer solstice and does not rise near the winter solstice. In those weeks SWMM's formula for the hour angle has no real solution, and the rate comes out as NaN.

The NaN does not stay in the evaporation figure. It becomes the subcatchment's evaporation loss, then its ponded depth, so no runoff is produced, and the report's runoff balance turns to `nan`. The test decks place one 10-acre subcatchment at latitude 70 N with 0.5 in of rain in the first hour and a climate file of Tmax 60 F / Tmin 40 F every day. For 1-5 June (polar day), 5.3.0 reports:

```
  Total Precipitation ......         0.417         0.500
  Evaporation Loss .........           nan           nan
  Infiltration Loss ........         0.000         0.000
  Surface Runoff ...........         0.000         0.000
  Final Storage ............           nan           nan
  Continuity Error (%) .....           nan
```

The run for 1-5 December (polar night) gives the same `nan` lines. With the fix, June gives 0.233 in of evaporation and 0.268 in of runoff, and December gives no evaporation, 0.301 in of runoff and 0.200 in left in depression storage.

At latitude 70 the affected days are 20 May to 22 July and 18 November to 21 January. That covers northern Alaska, Arctic Canada, Greenland, northern Scandinavia and Russia, and the Antarctic.

## Why it happens

`getTempEvap()` takes the arc cosine of `-tan(latitude)*tan(declination)` with no range check:

```c
// src/legacy/engine/climate.c, getTempEvap()
double phi = Temp.anglat * 2.0 * PI / 360.0;         // latitude angle (rad)
double del = 0.4093 * sin(a * (284. + (double)day)); // solar declination angle (rad)
double omega = acos(-tan(phi) * tan(del));           // sunset hour angle (rad)
double ra = 37.6 * dr *                              // extraterrestrial radiation
            (omega * sin(phi) * sin(del) +
             cos(phi) * cos(del) * sin(omega));
double e = 0.0023 * ra / lamda * sqrt(tr) * (ta + 17.8); // evap. rate (mm/day)
if (e < 0.0)
    e = 0.0;
```

At 70 N on 1 June the argument is -1.12; on 1 December it is +1.12. `acos()` of either is NaN, and so are `ra` and `e`. The guard `if (e < 0.0)` is false for NaN, so the NaN is returned. `setTemp()` stores it as the day's file evaporation and `setEvap()` turns it into `Evap.rate`. In `getSubareaRunoff()` (`subcatch.c:972`), `surfEvap = MIN(surfMoisture, evap)` yields NaN, which is added to the evaporation total and subtracted from the ponded depth.

Latitude is only checked to lie within +/-89.99 degrees, and `updateTempTimes()`, a few lines above in the same file, already clamps its own version of the same expression (`if (arg <= -1.0) arg = PI; else if (arg >= 1.0) arg = 0.0;`). `getTempEvap()` does not.

Vol. I gives the hour angle as `acos(-tan(phi) tan(delta))` (Eq. 2-11) without saying what to do outside the range. The standard treatment (FAO Irrigation and Drainage Paper 56, which uses the same radiation formula) is to set the hour angle to pi when the sun never sets and to 0 when it never rises. Radiation is then the 24-hour value in polar summer and zero in polar winter.

## How to reproduce

| File | What it is |
|---|---|
| [`NUM-43_lat70-june.inp`](NUM-43_lat70-june.inp) | One subcatchment at latitude 70 N, temperature-based evaporation, 1-5 June 2020, 0.5 in of rain in the first hour |
| [`NUM-43_lat70-december.inp`](NUM-43_lat70-december.inp) | The same for 1-5 December 2020 |
| [`NUM-43_clim.txt`](NUM-43_clim.txt) | Climate file: Tmax 60 F, Tmin 40 F every day of June and December 2020 |
| [`NUM-43_test.c`](NUM-43_test.c) | Runs both decks through the legacy toolkit, reads the potential evaporation rate at noon each day from the binary output, and compares it with the Hargreaves equation using an hour angle of pi or 0. It also checks that the runoff continuity error is finite. |
| [`NUM-43_test6.c`](NUM-43_test6.c) | The same check for 6.0.0, reading `swmm_climate_get_evap_rate()` at noon |

```sh
tools/run-test.sh NUM-43            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh NUM-43 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print the same:

```
NUM-43_lat70-june.inp
  Day   Noon PET (in/day)   Hargreaves (in/day)
  153                 nan               0.13787
  154                 nan               0.13859
  155                 nan               0.13926
  156                 nan               0.13990
  Runoff continuity error: nan %
NUM-43_lat70-december.inp
  Day   Noon PET (in/day)   Hargreaves (in/day)
  336                 nan               0.00000
  ...
  Runoff continuity error: nan %
FAIL: at latitude 70 N the temperature-based evaporation rate is not the Hargreaves value (June and December), or the runoff balance is not finite
NUM-43 5.3.0 base: FAIL
```

6.0.0 passes unpatched:

```
NUM-43_lat70-june.inp
  Day   Noon PET (in/day)   Hargreaves (in/day)
  153             0.13664               0.13787
  ...
NUM-43_lat70-december.inp
  Day   Noon PET (in/day)   Hargreaves (in/day)
  336             0.00000               0.00000
  ...
NUM-43 6.0.0 base: PASS
```

Its rates are 0.9% below the equation because 6.0.0 feeds the 7-day moving average differently from the legacy engine. That is a separate parity difference and is not part of this issue.

**With the fix**, 5.3.0 matches the equation exactly:

```
NUM-43_lat70-june.inp
  Day   Noon PET (in/day)   Hargreaves (in/day)
  153             0.13787               0.13787
  154             0.13859               0.13859
  155             0.13926               0.13926
  156             0.13990               0.13990
  Runoff continuity error: -0.253 %
NUM-43_lat70-december.inp
  Day   Noon PET (in/day)   Hargreaves (in/day)
  336             0.00000               0.00000
  ...
  Runoff continuity error: -0.257 %
PASS: at latitude 70 N the evaporation rate follows the Hargreaves equation in polar day and polar night and the runoff balance closes
NUM-43 5.3.0 patched: PASS
```

## The fix

Limit the argument to [-1, 1] before the arc cosine, as `updateTempTimes()` does:

```diff
-    double omega = acos(-tan(phi) * tan(del));           // sunset hour angle (rad)
+    double x = MAX(-1.0, MIN(1.0, -tan(phi) * tan(del))); // cos(omega), kept in [-1, 1]
+                                                         // in polar day and night
+    double omega = acos(x);                              // sunset hour angle (rad)
```

Where the argument is already in range, `MAX`/`MIN` return it unchanged, so results at latitudes below the polar circles are bit-identical. None of the regression decks uses temperature-based evaporation. 6.0.0 needs no patch.
