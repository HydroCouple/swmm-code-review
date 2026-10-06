# IO-27: A GHCN climate file without a Units token is read as deg F or deg C, not in the documented default of tenths of a degree C

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | The input reference says the optional Units token of `[TEMPERATURE] FILE` defaults to C10 (tenths of a degree C), the format of older GHCN-Daily files and the only one SWMM 5.1 read. 5.2.x and 5.3.0 instead default to deg F in US models and deg C in SI models. Such a file used without the token, as the manual allows, is read 10 times too warm: in the test, TMAX 250 / TMIN 150 (25 / 15 C) gives hourly temperatures of 152.36 to 247.64 F instead of 59 to 77 F. Snowfall, snowmelt and temperature-based evaporation are then wrong, and the file's evaporation (tenths of mm) and wind speed are misread as well. There is no warning. 6.0.0 follows the manual. |
| **Reached from** | `[TEMPERATURE] FILE Fname` or `FILE Fname Start` with a GHCN-Daily file and no Units token |
| **5.3.0** | `climate_readParams()` in [`src/legacy/engine/climate.c:574`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/climate.c#L574) |
| **5.2.4** | Same code, [`src/solver/climate.c:214`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/climate.c#L214) |
| **6.0.0** | Not affected: `ClimateFileReader` defaults a GHCN file to C10 ([`src/engine/hydrology/ClimateFile.cpp:169`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/ClimateFile.cpp#L169)) and only a Units token overrides it ([`:79`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/ClimateFile.cpp#L79)), so it already differs from 5.3.0 here |
| **Since** | 5.2.0, which added the Units token; 5.1 always read GHCN temperatures in tenths of a degree C |
| **Fix** | Default to C10 as documented: [`IO-27_swmm530.patch`](IO-27_swmm530.patch) (5.3.0 only) |

## The problem

The `[TEMPERATURE]` section of the input reference gives the climate file line as `FILE Fname (Start) (Units)`, with:

> Units    temperature units for GHCN files (C10 for tenths of a degree C (the default), C for degrees C or F for degrees F.

and adds in the remarks:

> Temperatures supplied from NOAA's latest Climate Data Online GHCN files should have their units (C or F) specified. Older versions of these files listed temperatures in tenths of a degree C (C10).

SWMM 5.1 read every GHCN file in tenths of a degree C. SWMM 5.2.0 added the Units token, but when the token is missing it uses the project's unit system instead of C10: deg F for US flow units, deg C for SI. An older-format file that worked in 5.1, or a new model that relies on the documented default, is read 10 times too warm in deg C or as deg F.

The test file has TMAX 250 and TMIN 150 every day, which is 25 C and 15 C in the C10 format. With no Units token, 5.2.4 and 5.3.0 give these hourly air temperatures:

| Deck | Expected | 5.2.4 and 5.3.0 |
|---|---|---|
| US units | 59 to 77 F | 152.36 to 247.64 F |
| SI units | 15 to 25 C | 152.36 to 247.64 C |

At these temperatures no precipitation falls as snow, any snow pack melts at once, and Hargreaves evaporation is far too high. The same setting also chooses the units of the file's evaporation (tenths of mm for C10, mm for C, inches for F) and wind speed, so those are misread too.

## Why it happens

```c
// src/legacy/engine/climate.c, climate_readParams(), [TEMPERATURE] FILE line
        // --- file temperature units
        FileTempUnits = DEG_F;
        if (UnitSystem == SI)
            FileTempUnits = DEG_C;
        if (ntoks > 3)
        {
            i = findmatch(tok[3], TempUnitsWords);
            if (i < 0)
                return error_setInpError(ERR_KEYWORD, tok[3]);
            FileTempUnits = i;
        }
```

`convertGhcndValue()` then converts each value according to `FileTempUnits`: `DEG_C10` divides by 10 and converts to deg F, `DEG_C` converts to deg F, and anything else is taken as deg F. The default is never `DEG_C10`.

6.0.0 sets `temp_units_ = DEG_C10` when it reads the GHCN header and replaces it only with a Units token given in the input file, which is what the manual describes.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-27_us-units.inp`](IO-27_us-units.inp) | US flow units, `[TEMPERATURE] FILE IO-27_ghcnd.txt` with no Units token, latitude 40 N, one day (Jan 1 2020) |
| [`IO-27_si-units.inp`](IO-27_si-units.inp) | The same with SI flow units |
| [`IO-27_ghcnd.txt`](IO-27_ghcnd.txt) | GHCN-Daily file (header `STATION DATE TMAX TMIN`) with TMAX 250 and TMIN 150 every day of Dec 2019 and Jan 2020 |
| [`IO-27_test.c`](IO-27_test.c) | Runs both decks through the legacy toolkit and reads the hourly air temperature from the binary output (deg F for US, deg C for SI). Expects every value within 0.5 degrees of 59 to 77 F (US) and 15 to 25 C (SI). |
| [`IO-27_test6.c`](IO-27_test6.c) | The same check for 6.0.0, reading the binary output with `swmm_output_get_system_result()` |

```sh
tools/run-test.sh IO-27            # 5.2.4 and 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh IO-27 --patched  # 5.3.0: PASS, 6.0.0: PASS (no patch)
```

**Without the fix**, 5.2.4 and 5.3.0 print:

```
GHCN file with TMAX 250, TMIN 150 (tenths of deg C), no Units token
  IO-27_us-units.inp     hourly air temperature  152.36 to  247.64 F  (expected 59 to 77 F)
  IO-27_si-units.inp     hourly air temperature  152.36 to  247.64 C  (expected 15 to 25 C)
FAIL: without a Units token the GHCN temperatures are not read in tenths of a degree C, the documented default (US and SI decks)
IO-27 5.3.0 base: FAIL
```

6.0.0 unpatched and 5.3.0 with the fix print the same:

```
GHCN file with TMAX 250, TMIN 150 (tenths of deg C), no Units token
  IO-27_us-units.inp     hourly air temperature   59.42 to   76.58 F  (expected 59 to 77 F)
  IO-27_si-units.inp     hourly air temperature   15.24 to   24.76 C  (expected 15 to 25 C)
PASS: without a Units token the GHCN temperatures are read in tenths of a degree C
IO-27 5.3.0 patched: PASS
IO-27 6.0.0 base: PASS
```

The hourly values do not reach exactly 59 and 77 F because the hours of Tmin and Tmax (about 7:24 and 13:36 at 40 N on Jan 1) fall between report times.

## The fix

5.3.0, in `climate_readParams()`:

```diff
-        // --- file temperature units
-        FileTempUnits = DEG_F;
-        if (UnitSystem == SI)
-            FileTempUnits = DEG_C;
+        // --- file temperature units (default is tenths of a degree C)
+        FileTempUnits = DEG_C10;
```

This makes the code match the input reference, SWMM 5.1 and 6.0.0. Files given with an explicit `C`, `F` or `C10` token are unchanged.

The patch changes results for one group of users: those who read a current NOAA Climate Data Online file (in deg F or deg C) without a Units token, and whose file units happen to match the project's unit system. 5.2 read those files correctly. With the patch they are read as tenths of a degree C, as the manual and 6.0.0 already do, so they need the `F` or `C` token that the manual asks for. The alternative is to keep the code and change the manual to the unit-system default, which would leave 5.3.0 and 6.0.0 reading the same file differently. Effect on other models: none of the regression decks uses climate data.
