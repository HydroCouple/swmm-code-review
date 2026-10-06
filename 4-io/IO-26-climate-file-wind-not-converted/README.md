# IO-26: Wind speed from a user-prepared climate file is not converted from km/hr in SI models

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | In SI models that take wind speed from a user-prepared climate file, SWMM uses the km/hr values as if they were mph, 1.608 times too large. Rain-on-snow melt, the only process that uses wind speed, over-estimates its wind-driven part by the same factor. In the test, 2 h of rain on snow melt 20.070 mm instead of 14.513 mm. The reported wind speed (`swmm_getValue(swmm_WINDSPEED)` in 5.3.0, `swmm_climate_get_wind_speed()` in 6.0.0) is 1.608 times the file value. There is no warning. |
| **Reached from** | `[TEMPERATURE] WINDSPEED FILE` with a user-prepared climate file (`[TEMPERATURE] FILE`) in a model with SI flow units. NCDC/NOAA climate files are converted correctly; US models are not affected. |
| **5.3.0** | `parseUserFileLine()` in [`src/legacy/engine/climate.c:1609`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/climate.c#L1609); the value reaches `setWind()` at [`:1298`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/climate.c#L1298) and `getRainmelt()` in [`src/legacy/engine/snow.c:789`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/snow.c#L789) |
| **5.2.4** | Same code, [`src/solver/climate.c:1244`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/climate.c#L1244) |
| **6.0.0** | Reproduces: `ClimateFileReader::parseUserLine()` in [`src/engine/hydrology/ClimateFile.cpp:302`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/ClimateFile.cpp#L302) stores the value as read |
| **Since** | Every release (the code is in the first commit of the EPA repository, 5.1) |
| **Fix** | Divide the file value by the wind speed conversion factor when it is read: [`IO-26_swmm530.patch`](IO-26_swmm530.patch), [`IO-26_swmm600.patch`](IO-26_swmm600.patch) |

## The problem

SWMM can read daily wind speed from the same climate file as the daily temperatures. For a user-prepared climate file, the engine manual (Chapter 3, Climate Files) gives the units:

> For a user-prepared climate file, the data must be in the same units as the project being analyzed. For US units, temperature is in degrees F, evaporation is in inches/day, and wind speed is in miles/hour. For metric units, temperature is in degrees C, evaporation is in mm/day, and wind speed is in km/hour.

SWMM converts the file's temperatures from degrees C and keeps its evaporation in user units, but does not convert its wind speed. The engine works in mph, so in an SI model a wind speed of 32.18 km/hr in the file is used as 32.18 mph (51.75 km/hr). The same wind speed given as `WINDSPEED MONTHLY 32.18 ...` is converted and used correctly.

Wind speed is used only in the rain-on-snow melt equation, whose melt rate grows linearly with wind speed. The test deck has 100 mm of snow (water equivalent) on a 4-ha pervious subcatchment and 2 h of 12.7 mm/hr rain at 10 C. The three engines report:

| Wind speed source | Final snow cover (mm) | Melt (mm) |
|---|---|---|
| Climate file, 32.18 km/hr | 79.930 | 20.070 |
| `WINDSPEED MONTHLY`, 32.18 km/hr | 85.487 | 14.513 |
| None | 94.626 | 5.374 |

The wind-driven melt is 14.696 mm from the file against 9.139 mm from the monthly values, a ratio of 1.608. Running the monthly deck with 51.75 km/hr gives 79.929 mm, the same as the file deck.

## Why it happens

The user-prepared file parser converts temperatures to degrees F and leaves the wind speed as read:

```c
// src/legacy/engine/climate.c, parseUserFileLine()
    // --- process TMAX
    if (strlen(s0) > 0 && *s0 != '*')
    {
        x = atof(s0);
        if (UnitSystem == SI)
            x = 9. / 5. * x + 32.0;
        FileData[TMAX][d] = x;
    }
    ...
    // --- process WIND
    if (strlen(s3) > 0 && *s3 != '*')
        FileData[WIND][d] = atof(s3);
```

`setWind()` converts the monthly values from user units to mph, but takes the climate-file value as it is:

```c
// src/legacy/engine/climate.c, setWind()
    case MONTHLY_WIND:
        datetime_decodeDate(theDate, &yr, &mon, &day);
        Wind.ws = Wind.aws[mon - 1] / UCF(WINDSPEED);
        break;

    case FILE_WIND:
        Wind.ws = FileValue[WIND];
        break;
```

This is right for the NCDC TD-3200 and GHCN-Daily formats, whose parsers convert wind to mph (`setTD3200FileValues()`, `convertGhcndValue()`), but not for the user-prepared format. `getRainmelt()` then uses `uadj = 0.006 * Wind.ws` with `Wind.ws` in mph.

6.0.0 has the same omission in `ClimateFileReader::parseUserLine()`, and `SWMMEngine::stepRunoff()` uses the stored value as mph.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-26_wind-file.inp`](IO-26_wind-file.inp) | SI deck, 100 mm of snow on a 4-ha pervious subcatchment, 2 h of 12.7 mm/hr rain, temperature 10 C and wind speed from the climate file |
| [`IO-26_wind-monthly.inp`](IO-26_wind-monthly.inp) | The same with `WINDSPEED MONTHLY` 32.18 km/hr |
| [`IO-26_no-wind.inp`](IO-26_no-wind.inp) | The same with no wind speed data (wind speed 0) |
| [`IO-26_clim.txt`](IO-26_clim.txt) | User-prepared climate file: Tmax = Tmin = 10 C, wind 32.18 km/hr every day of Dec 2019 and Jan 2020 |
| [`IO-26_test.c`](IO-26_test.c) | Runs the three decks through the legacy toolkit and reads the snow cover from the reports. Since melt is linear in wind speed, the wind speed the engine took from the file is 32.18 x (melt_file - melt_none) / (melt_monthly - melt_none). Expects 32.18 km/hr (ratio within 2% of 1). |
| [`IO-26_test6.c`](IO-26_test6.c) | The same check for 6.0.0 with `swmm_get_runoff_total()`, and prints `swmm_climate_get_wind_speed()` |

```sh
tools/run-test.sh IO-26            # 5.2.4, 5.3.0 and 6.0.0: FAIL
tools/run-test.sh IO-26 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, all three engines print (6.0.0 adds the getter line):

```
SI deck, 100 mm of snow, 2 h of 12.7 mm/hr rain at 10 C
  IO-26_wind-file.inp      initial  100.000 mm  final   79.930 mm  melted  20.070 mm
  IO-26_wind-monthly.inp   initial  100.000 mm  final   85.487 mm  melted  14.513 mm
  IO-26_no-wind.inp        initial  100.000 mm  final   94.626 mm  melted   5.374 mm
Wind-driven melt: climate file 14.696 mm, MONTHLY 9.139 mm, ratio 1.608 (expected 1.000)
Wind speed used from the file: 51.75 km/hr (file value 32.18 km/hr)
swmm_climate_get_wind_speed() in the climate-file run: 51.75 km/hr
FAIL: the climate file's wind speed of 32.18 km/hr is used as 51.75 km/hr (1.608 x); final snow cover 79.930 mm instead of 85.487 mm
IO-26 6.0.0 base: FAIL
```

**With the fix**, 5.3.0 and 6.0.0 print the same (6.0.0 adds the getter line):

```
SI deck, 100 mm of snow, 2 h of 12.7 mm/hr rain at 10 C
  IO-26_wind-file.inp      initial  100.000 mm  final   85.487 mm  melted  14.513 mm
  IO-26_wind-monthly.inp   initial  100.000 mm  final   85.487 mm  melted  14.513 mm
  IO-26_no-wind.inp        initial  100.000 mm  final   94.626 mm  melted   5.374 mm
Wind-driven melt: climate file 9.139 mm, MONTHLY 9.139 mm, ratio 1.000 (expected 1.000)
Wind speed used from the file: 32.18 km/hr (file value 32.18 km/hr)
swmm_climate_get_wind_speed() in the climate-file run: 32.18 km/hr
PASS: the climate file's wind speed is read in km/hr, as documented for SI units
IO-26 6.0.0 patched: PASS
```

## The fix

5.3.0, in `parseUserFileLine()`, convert the value to mph when it is read, as `setWind()` does for the monthly values:

```diff
-    // --- process WIND
+    // --- process WIND (km/hr for SI units ==> mph)
     if (strlen(s3) > 0 && *s3 != '*')
-        FileData[WIND][d] = atof(s3);
+        FileData[WIND][d] = atof(s3) / UCF(WINDSPEED);
```

6.0.0, in `ClimateFileReader::parseUserLine()`:

```diff
-    // WIND
+    // WIND (user units: km/hr for SI → mph, legacy UCF(WINDSPEED) = 1.608)
     if (s3[0] != '\0' && s3[0] != '*') {
-        file_data_[WIND][d] = std::atof(s3);
+        double v = std::atof(s3);
+        if (unit_system_ == 1) v /= 1.608;
+        file_data_[WIND][d] = v;
     }
```

The conversion factor is 1 for US units, so only SI models with wind speed from a user-prepared climate file change. For them rain-on-snow melt drops to what the documented units give. Effect on other models: none of the regression decks uses climate data.
