# Further findings

[All issues](README.md)

Defects noticed while the issues were being confirmed and fixed, but not written up as issues: they have no test, deck or patch in this repository. Each line says where the code is and how far it was checked. "Verified" means it was reproduced with a run; "code reading" means it was not run.

## Legacy engines (5.2.4 and 5.3.0)

| Area | Finding | Where | Checked |
|---|---|---|---|
| Routing | Flooding volume is under-booked when a run ends while a node floods: `routing_execute()` gives node overflow (already a step average) half-step trapezoid weights, so half of the last step's overflow is never counted. The NUM-08 fix exposes it: the review's flood deck moves from -1.709 % to +2.564 %. | `routing.c` | Verified |
| Routing | Legacy books only half of the last routing step's ledger rates, because no update follows the final step. | `routing.c`, `massbal.c` | Code reading |
| Routing | Under KINWAVE, when inflow exceeds full flow, the "both negative" branch fills an empty conduit in one step: about -0.54 % continuity on the NUM-18 deck with no seepage. | `kinwave.c` | Verified |
| Input | `[HYDROGRAPHS]` placed before `[RAINGAGES]` loses all RDII inflow: `gage_readParams()` clears the "used" flag the unit-hydrograph reader set (0.000 instead of 0.417 ac-ft). Fixed as a side effect by CON-23's patch; 6.0.0 not affected. | `gage.c` | Verified |
| Input | `PlacementTypeWords` in `inlet.c` has no NULL terminator: an invalid placement keyword in `[INLET_USAGE]` (e.g. `SAG`) makes `findmatch()` read past the array (ASan global-buffer-overflow at `inlet.c:430`). 6.0.0 not affected. | `inlet.c:430` | Verified |
| Input | An ellipse or arch size code such as `1e10` overflows the int conversion and then the subtraction (undefined behaviour), in `xsect.c:616` and 6.0.0's `XSection.cpp:523`. | `xsect.c:616` | Verified (sanitizer) |
| Input | `datetime_encodeTime()` computes `hour*3600` in `int`, so a clock string such as `END_TIME 600000:00:00` is signed-overflow undefined behaviour. | `datetime.c:150` | Code reading |
| Input | `START_DRY_DAYS` is read with `atof()`: `nan` is accepted and `abc` reads as 0. | `project.c` | Code reading |
| Input | `table_readTimeseries()` drops a trailing date or time that has no value after it, with no error. | `table.c` | Code reading |
| Input | `[CONTROLS]` placed above `[PUMPS]` gives a spurious ERROR 209. | `controls.c` | Verified |
| Input | The object-count pass does not handle quoted object names with spaces (ERROR 209). | `input.c` | Verified |
| Input | In `rdii_readUnitHydParams()` a non-numeric initial-abstraction value is reported with the wrong token (`tok[i+2]` instead of `tok[i+3]`). 6.0.0 accepts it silently. | `rdii.c` | Code reading |
| Input | A weir's Cd2 defaults to 0, but the manual says it defaults to Cd (affects TRAPEZOIDAL weirs that leave Cd2 blank). | `link.c` | Code reading |
| Input | `exfil_initState()` has no PARABOLOID case, so a PARABOLOID storage unit with seepage uses uninitialised bottom area and bank depths. | `exfil.c` | Code reading |
| Input | The manual promises a warning and "no overbank" when a transect bank station matches no station; the code applies the right overbank's n to the whole section without a warning. | `transect.c` | Code reading |
| Input | `transect_createStreetTransect()` may overwrite the shared n values that an `NC 0` line inherits, so streets read before `[TRANSECTS]` could leak their n into transects. | `transect.c` | Code reading |
| Geometry | The egg shape's depth-from-area table runs 4–11 % below the inverse of its area table between 6 % and 30 % full, and the first two entries of `S_Egg` and `Y_Egg` were not corrected with the other egg tables in 5.2.1. Investigated as NUM-21 and dropped: the remaining error is table resolution, and an exact fix needs circle geometry for the invert. | `xsect.dat` | Verified |
| Geometry | `getFlowSpread()` decides whether flow stays in the gutter with slope a/W instead of Sx + a/W; capture barely changes (see NUM-25). | `inlet.c:1221` | Verified |
| Groundwater | `gwater_getGroundwater()` ignores the ODE solver's return code; if it failed, the aquifer state would stay unchanged while infiltration is booked. No input was found that makes it fail. | `gwater.c` | Code reading |
| Groundwater | The groundwater ledger books ET and deep percolation as end-of-step rate times the step, a smaller error than NUM-36's. | `gwater.c` | Code reading |
| Groundwater | `USE RUNOFF` runs print a Groundwater Continuity table with no fluxes (48.8 % error in IO-46's deck). | `massbal.c`, `iface.c` | Verified |
| Rainfall | API rainfall set on the first gage of a shared series leaks to later gages on that series that have no API value (S2 peak 10.084 instead of 5.042 cfs). See API-14. | `gage.c` | Verified |
| Runoff | A 1-hour run that ends during rain shows a -4.15 % runoff continuity error on a trivial impervious deck in all three engines (runoff plus final storage exceed precipitation by 0.042 in). Not investigated. | runoff | Verified |
| Snow / climate | When the sun never sets (polar day), the hourly temperature drops from about Tmax to Tmin at midnight: 18.26 F at 70 N on 20–21 June. A smaller seam exists at every latitude (about 1.1 F at 40 N). | `climate.c` | Verified |
| Snow | In `reduceColdContent()` cold content cancels 1/RNM times its depth of melt, while the manual (step 9) cancels an equal depth; they differ when RNM < 1. | `snow.c` | Code reading |
| Quality | STEADY routing decays a conduit's pollutant over the routing step, not the travel time its comment describes. | `qualrout.c:434` | Code reading |
| Quality | Expression quirks: `1/0` evaluates to inf; `LOG10` of a negative number zeroes the whole expression while `LOG` zeroes only its own term. | `mathexpr.c` | Verified |
| Quality | The input manual's first-order decay treatment example `C = BOD * exp(-0.05*HRT)` is applied every routing step and removes 80–98 % depending on the step instead of about 5 %. | manual | Verified |
| Hot start | 5.2.4 only: `saveRunoff()` writes each land use's pollutant buildup with `fwrite(x, sizeof(double), Nobjects[POLLUT], f)` from a one-value buffer, so with more than one pollutant the hot start file gets extra values and reads past `x`. Fixed in EPA's `develop` branch ("Build 5.2.5") and in 5.3.0. | 5.2.4 `hotstart.c:419` | Code reading |
| Hot start | A 5.3.0 dated save whose date falls after the end of the run leaves a 39-byte, header-only file. A date without a time, and more than 10 SAVE HOTSTART lines, are ignored without a message. | `hotstart.c` | Verified |
| Output | 5.2.4's `.out` file ends one reporting period earlier than 5.3.0's on some decks (BND-09), so `swmm_getSavedValue` sees one period fewer. | `swmm5.c`, `output.c` | Verified |
| Output reader | 5.2.4 `SMO_getSystemAttribute()` returns the address of a local float (stack use-after-return); 5.2.4 `SMO_open()` leaves the caller a dangling handle after a failed open. Both fixed in 5.3.0. | `swmm_output.c` | Verified |
| Output reader | 5.3.0's system-attribute enum stops at 13 although the file stores 15 system variables; the property codes from `SMO_getPropertyCode()` are in no public header and were renumbered by commit b91f3107. | `swmm_output.c` | Code reading |
| Units | SWMM's mg-to-lb factor 2.203e-6 is 0.074 % below the exact value, so US loads in reports are about 0.07 % low. | `consts.h` | Code reading |
| Build | Without OpenMP, the 5.3.0 legacy library does not link: `project.c` and `swmm5.c` both define the `omp_get_max_threads` stub. 5.2.4's CMake also fails to configure without OpenMP (`OpenMP::OpenMP_C` target missing). | `project.c:74`, `swmm5.c:96` | Verified |
| CLI | The legacy command line program also exits 0 for a wrong number of arguments (API-18 covers failed runs). | `main.c` | Verified |

## 6.0.0 only

These are in 6.0.0 code that has no legacy counterpart, or where 6.0.0 departs from legacy. The review checked 6.0.0 only to carry legacy fixes forward, so this list is incidental, not a review of 6.0.0.

| Area | Finding | Where | Checked |
|---|---|---|---|
| Input | `handle_timeseries()` reads only the first time/value pair of each `[TIMESERIES]` line: 10 pairs per line gives 0.964 instead of 0.991 ac-ft; 73 pairs on one line gives 0.000. | input handlers | Verified |
| Input | `[COVERAGES]` reads only the first land use and percent on a line (`S1 RES 60 COM 40`: 11.6 instead of 19.4 lb washoff). | `QualityHandler.cpp:257` | Verified |
| Input | Most handlers read numbers with `to_double()`, which turns any unreadable token into 0 with no message (`1O0` reads as 1.0); legacy gives ERROR 211. IO-01's patch adds strict checks only for the fields its test uses. | input handlers | Verified |
| Input | An unknown `FLOW_UNITS` keyword runs silently as CFS. | options | Verified |
| Input | Expressions that name an undefined variable evaluate to 0 (legacy ERROR 233); a `[TREATMENT]` expression that fails to parse is silently dropped, and `log10` and the trig functions are missing from the treatment parser. | `SWMMEngine.cpp:9224` | Verified |
| Input | FUNCTIONAL storage with a negative A0, `RECT_CLOSED 2 0`, a closed-rectangle transverse weir and conduit roughness 0 are accepted (legacy rejects them). | input handlers | Verified |
| Input | Rows of `[SUBAREAS]`, `[INFILTRATION]`, `[COVERAGES]`, `[LOADINGS]`, `[LID_USAGE]` placed above `[SUBCATCHMENTS]`, an outfall RouteTo naming a later or unknown subcatchment, and `[SUBAREAS]` lines with fewer than 6 tokens are dropped without a message. IO-17 and IO-20 fix the first two sections. | input handlers | Verified |
| Input | Pattern parsing reads non-numeric factors as 1.0 and accepts the type keyword again on continuation lines. | input handlers | Verified |
| Input | A deck where `[DWF]` is directly followed by `[REPORT]` fails with ERROR 209; `ncdc_format.inp` from the regression suite does not open; `[REPORT]` keywords are matched exactly, so legacy prefix forms such as `NODE ALL` are ignored; a deck with no START/END dates ran with a start in year 8616. | input | Verified (last: seen once) |
| Input | Any yes/no value other than YES/TRUE/1 is read as NO; snow pack parameters are never validated (ERR_SNOWPACK_PARAMS is never raised); the default latitude is 0 (legacy 40, manual 50). | options, snow | Code reading / verified |
| Hot start | `[FILES]` SAVE HOTSTART writes no subcatchment records, so USE HOTSTART of a 6.0.0 file fails for any model with subcatchments (empty "USE HOTSTART:" message); with a 5.3.0 file, 6.0.0 skips all subcatchment and groundwater state with a warning; node and link pollutant concentrations are read and discarded. | `HotStartManager.cpp` | Verified |
| Hot start | `HotStartManager::read_file` reads the trailing CRC through a misaligned `uint32_t*` (sanitizer error for any file whose length is not a multiple of 4). | `HotStartManager.cpp:336` | Verified (sanitizer) |
| Routing | STEADY routing reports a conduit's volume as Final Stored Volume where 5.3.0 reports 0 (-0.727 % on the NUM-18 deck; -2.764 % vs -0.179 % on `control_rules_test`). | routing | Verified |
| Routing | DW does not book an initial ponded volume for a junction that starts above full depth (legacy `initNodes()` does). | DynamicWave | Code reading |
| Routing | The engine builds with `-mfma` (`src/engine/CMakeLists.txt:191`), which makes some KINWAVE coefficients one ulp off legacy; KW flows differ from 5.3.0 by up to 2 % on IO-48's deck before any patch. | build | Verified |
| Runoff | Runoff continuity on `swc19` and `swc23` is -6.13 % and -8.84 % (5.3.0: -0.12 % and -0.10 %). | runoff | Verified |
| Runoff | 100 %-impervious swale infiltration uses the pervious area's ponded depth (stops at 02:01; 5.3.0 continues at 1.0 in/hr). | `Runoff.cpp:334` | Verified |
| Runoff | The runoff continuity table counts every LID's surface water as depth × void fraction, which is wrong for a swale's trapezoidal section (swale with VegFrac 0: 3.150 in and -67.8 % vs 1.782 in and 0.62 % in 5.3.0). | `LID.cpp:561` | Verified |
| Rainfall | A FILE rain gage gives 12 times less rain than legacy; `swmm_forcing_gage_rainfall()` is overwritten by the next runoff step and has no effect; API rainfall is multiplied by the monthly rain adjustment (legacy uses it as set). | gages | Verified / code reading |
| Controls | The GAGE n-hour past-rain premise never fires (`Gage.cpp:392` adds intensity/3600 per runoff step instead of multiplying by the step length); `VARIABLE V = GAGE G 6` may drop the hour count. | `Gage.cpp:392` | Verified / code reading |
| Quality | Treatment Mass Reacted is far too small: the removal rate (mass/s) is added to a mass accumulator (`QualityRouting.cpp:1703`). | `QualityRouting.cpp:1703` | Verified |
| Quality | Runoff Quality "Remaining Buildup" leaves out ponded mass (+88.9 % where 5.3.0 shows 0 %); "Surface Runoff" leaves out LID drain loads to the default outlet. Outfall-to-subcatchment run-on appears not to carry pollutant loads. | runoff quality | Verified / code reading |
| Report | Hard-coded or missing columns: Subcatchment Runoff Summary "Total Runon" is always 0.00 (`DefaultReportPlugin.cpp:2137`); Storage Volume Summary evaporation and exfiltration loss % are always 0.0 (`:2707`); the LID Performance Summary has no storage or continuity columns. | `DefaultReportPlugin.cpp` | Verified |
| Report | Pumps count as running at any flow above 0 (legacy 0.001 cfs), so Percent Utilized and Start-Ups differ (user3 PUMP1: 100 % and 1 vs 92.51 % and 4); the Link Flow Summary labels pumps "DUMMY". | `DefaultReportPlugin.cpp:2884` | Verified |
| Report | Street Inlet Flow Summary labels volumes "1000 Gal" even for SI models; the Street Flow Summary header sits about 3 columns right of its values; the RDII "Sewershed Rainfall" prints subcatchment rainfall (0.000 with no subcatchments). | `DefaultReportPlugin.cpp:3172`, `:933` | Verified |
| Report | With reporting starting after the simulation, the `.out` header holds the simulation start date (see IO-49); an output file that cannot be opened gives only "plugin prepare() failed". | output | Verified |
| API | `swmm_get_routing_total()` and `swmm_get_routing_continuity_error()` disagree with 6.0.0's own report (first-step inflow counted in full: 13.889 % vs 13.899 %; -0.022 % vs 0.000 %). | mass balance API | Verified |
| API | `swmm_node_get_inflow()` / `swmm_node_get_inflows_bulk()` return lateral inflow although documented as total inflow (`openswmm_nodes_impl.cpp:363`, `:468`); `swmm_link_get_capacity()` is documented as a depth ratio but returns q/q_full; `swmm_get_current_time()` returns seconds while `swmm_get_start_time()` returns a date. | C API | Verified |
| API | The binary RDII file reader only range-checks node indexes: a file pointing at a node with no `[RDII]` entry is accepted (5.3.0 gives ERROR 345). | RDII | Verified |
| Parity | With `MAX_TRIALS 1`, IO-53's deck gives different flows than 5.3.0 (C1 turns 11 instead of 33 times); three never-dry conduits on `events_example` differ in Norm Ltd by 0.01–0.02. | DynamicWave | Verified |
