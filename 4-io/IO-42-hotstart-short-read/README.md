# IO-42: A truncated or mismatched hot start file is applied without an error

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Wrong initial state and no error message. When the file ends early, every value the reader did not get is set to the last value it did read. In the test, a file without its link records leaves both conduits with setting 0, so they stay closed for the whole continuation run (no outflow, flow continuity error -25.6 %). A file saved before a junction was turned into a storage unit, or the other way round, shifts every later value by one float: C1 starts at 1.04 or 0 cfs instead of 5.97 cfs, with a "setting" of 5.93 or 1.04. 6.0.0 refuses the short files, but with the empty message `USE HOTSTART: `, and accepts the long one just as the legacy engines do |
| **Reached from** | `USE HOTSTART` in `[FILES]` (and 5.3.0's `swmm_useHotStart()`), with a file whose header counts match the model but whose records do not: a copy that was cut short, or a file saved before a node changed type, or before groundwater, snow packs or storage units were added or removed |
| **5.3.0** | `readFloat()` in [`src/legacy/engine/hotstart.c:643`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/hotstart.c#L643), `readDouble()` at [`:665`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/hotstart.c#L665), and the end of `initializeFromHotstartFile()` at [`:305`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/hotstart.c#L305) |
| **5.2.4** | Same code, [`src/solver/hotstart.c:507`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/hotstart.c#L507) and [`:529`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/hotstart.c#L529) |
| **6.0.0** | Partly affected: `HotStartManager::apply_legacy_routing()` ([`src/engine/core/HotStartManager.cpp:1284`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/HotStartManager.cpp#L1284)) checks every read but returns without an error text ([`:1440`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/HotStartManager.cpp#L1440)), and never checks for data left over ([`:1450`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/HotStartManager.cpp#L1450)) |
| **Since** | 5.1.000 (2014), the first release in the repository with `hotstart.c` |
| **Fix** | Check every read and refuse a file with data left over: [`IO-42_swmm530.patch`](IO-42_swmm530.patch), [`IO-42_swmm600.patch`](IO-42_swmm600.patch) |

## The problem

A hot start file stores the state of every node and link by position, with nothing in between to say which record is which. The header holds the number of subcatchments, land uses, nodes, links and pollutants and the flow units, and that is all SWMM checks before it reads the records. The length of each record depends on things the header does not record: a storage unit has one float more than a junction (its residence time), a subcatchment with groundwater has 4 doubles more, one with a snow pack 15 more.

So two kinds of bad file pass the header check:

- **A file that was cut short**, for instance by a full disk or an interrupted copy.
- **A file from a model that has changed since it was saved**, with the same object counts. Turning a junction into a storage unit, or back, is enough.

The test saves the state of a two-conduit network at 03:00, then continues the run from 03:00 for 10 minutes with four files. Only the first matches the model:

| File | What it holds | What 5.2.4 and 5.3.0 do |
|---|---|---|
| matching file | the junction model's own file, 87 bytes | starts C1 at 5.9747 cfs, as saved |
| truncated | the same file without its last 24 bytes, the two link records | sets C1 and C2 to flow 0, depth 0, **setting 0**; both conduits stay closed for all 10 minutes |
| junction file, storage model | 87 bytes; the storage model needs 91 | every value after J2 moves one place: J2's residence time is read from O1's depth, C1's flow from C1's saved depth (1.04), C1's setting from C2's saved flow (5.93); the last value is missing and not noticed |
| storage file, junction model | 91 bytes; the junction model needs 87 | every value after J2 moves the other way: C1's flow is O1's lateral inflow (0), its depth is its saved flow (5.97) |

None of them gives an error or a warning. The only trace is in the continuity check, and only when the effect on volume is large: -25.6 % for the truncated file, -0.14 % for the shift that sets C1 to 1.04 cfs.

6.0.0 checks every read and refuses the two short files, but its message is `USE HOTSTART: ` with nothing after it. It does not check the file length, so it accepts the long file just as the legacy engines do.

## Why it happens

`readFloat()` ignores the return value of `fread()`. At the end of the file the read fails, `*x` keeps whatever it held, and the function returns `TRUE`:

```c
// src/legacy/engine/hotstart.c, readFloat()
    // --- read a value from the file
    fread(x, sizeof(float), 1, f);

    // --- test if the value is NaN (not a number)
    if ( *(x) != *(x) )
    {
        report_writeErrorMsg(ERR_HOTSTART_FILE_READ, "");
```

`readRouting()` reads every node and link value through the same local `float x`, so every value past the end of the file is the last value actually read. In the truncated file that is O1's lateral inflow, 0, which becomes each conduit's flow, depth and setting. A setting of 0 closes a conduit until a control rule opens it.

`readDouble()`, used for the subcatchment records, tests `feof()` before reading. `feof()` only becomes true after a read has failed, so the first read past the end is not caught either:

```c
// src/legacy/engine/hotstart.c, readDouble()
    if ( feof(f) )
    {    
        *(x) = 0.0;
        report_writeErrorMsg(ERR_HOTSTART_FILE_READ, "");
        return FALSE;
    }
    fread(x, sizeof(double), 1, f);
```

After the last link record nothing checks that the file has ended, so a file with records left over is accepted.

In 6.0.0 every `read_pod()` failure in `apply_legacy_routing()` returns 1 without setting `tl_last_io_error`, which the caller appends to `"USE HOTSTART: "`. After the link loop the function returns 0 whether or not the file has ended.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-42_save.inp`](IO-42_save.inp) | J1 - C1 - J2 - C2 - O1, inflow ramping from 0 to 12 cfs over 6 hours; runs to 03:00 and saves `IO-42_junction.hsf` |
| [`IO-42_save_storage.inp`](IO-42_save_storage.inp) | The same with J2 a storage unit; saves `IO-42_storage.hsf` |
| [`IO-42_use.inp`](IO-42_use.inp) | Continues from 03:00 to 03:10 with `USE HOTSTART IO-42_use.hsf` (J2 a junction) |
| [`IO-42_use_storage.inp`](IO-42_use_storage.inp) | The same with J2 a storage unit |
| [`IO-42_test.c`](IO-42_test.c) | Runs both save decks, writes each test file to `IO-42_use.hsf`, continues the run and reads C1's initial state, its largest flow and the flow continuity error (5.2.4 and 5.3.0) |
| [`IO-42_test6.c`](IO-42_test6.c) | The same with the 6.0.0 API, plus a check that a refusal has a message |

```sh
tools/run-test.sh IO-42            # 5.2.4, 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-42 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**, 5.2.4 and 5.3.0 print the same table:

```
C1 flow at the end of the save run: 5.9747 cfs
hot start file                  bytes  error  C1 flow C1 depth  C1 sett   C1 max  flow CE (%)  message
                                                (cfs)     (ft)             (cfs)
matching file (control)            87      0   5.9747   1.0394   1.0000   6.3084       -0.211  
truncated (no link records)        63      0   0.0000   0.0000   0.0000   0.0000      -25.561  
junction file, storage model       87      0   1.0394   1.0000   5.9289   7.2060       -0.138  
storage file, junction model       91      0   0.0000   5.9747   1.0393   7.6312       20.131  
FAIL: 3 of 3 hot start files that do not match the model were applied without an error
IO-42 5.3.0 base: FAIL
```

6.0.0 refuses the short files without saying why and accepts the long one:

```
matching file (control)            87      0   5.9747   1.0394   1.0000   6.3084       -0.211  
truncated (no link records)        63     12      nan      nan      nan      nan          nan  USE HOTSTART: 
junction file, storage model       87     12      nan      nan      nan      nan          nan  USE HOTSTART: 
storage file, junction model       91      0   0.0000   5.9747   1.0393   7.6294       19.906  
FAIL: of 3 hot start files that do not match the model, applied without an error: 1, refused with an empty message: 2
IO-42 6.0.0 base: FAIL
```

**With the fix**:

```
matching file (control)            87      0   5.9747   1.0394   1.0000   6.3084       -0.211  
truncated (no link records)        63    335      nan      nan      nan      nan          nan  ERROR 335: error in reading from hot start interface file.
junction file, storage model       87    335      nan      nan      nan      nan          nan  ERROR 335: error in reading from hot start interface file.
storage file, junction model       91    333      nan      nan      nan      nan          nan  ERROR 333: incompatible data found in hot start interface file.
PASS: the matching hot start file is applied; the truncated and mismatched files are refused with an error
IO-42 5.3.0 patched: PASS
```
```
truncated (no link records)        63     12      nan      nan      nan      nan          nan  USE HOTSTART: Hotstart file 'IO-42_use.hsf' ended before all records the model needs were read
junction file, storage model       87     12      nan      nan      nan      nan          nan  USE HOTSTART: Hotstart file 'IO-42_use.hsf' ended before all records the model needs were read
storage file, junction model       91     12      nan      nan      nan      nan          nan  USE HOTSTART: Hotstart file 'IO-42_use.hsf' holds more data than the model's nodes and links need
PASS: the matching hot start file is applied; the truncated and mismatched files are refused with an error message
IO-42 6.0.0 patched: PASS
```

## The fix

Check the result of every `fread()`, and after the last link record check that the file has ended:

```diff
-    fread(x, sizeof(float), 1, f);
+    if ( fread(x, sizeof(float), 1, f) != 1 )
+    {
+        report_writeErrorMsg(ERR_HOTSTART_FILE_READ, "");
+        *(x) = 0.0;
+        return FALSE;
+    }
```
```diff
     readRouting();
+
+    // --- data left over means the file does not match the model's layout
+    if ( !ErrorCode && fgetc(FhotstartInput.file) != EOF )
+        report_writeErrorMsg(ERR_HOTSTART_FILE_FORMAT, "");
     fclose(FhotstartInput.file);
```

`readDouble()` gets the same `fread()` check in place of the `feof()` test. The 6.0.0 patch sets one error text before the header reads, which every short read now reports (the reads that set their own text still do, and success clears it), and refuses a file with data left over after the link records.

A file can still match the model in length and not in content, for instance when two junctions are swapped, or when one node becomes a storage unit while another stops being one. Catching that needs more in the header (node types, the number of groundwater and snow pack subcatchments, or a hash of the IDs), which is a format change and is left out here.

**Effect on other models.** A file that matches its model is read exactly as before. Saving and reusing hot start files from Example3, Example5 and extran10 (storage units, groundwater, subcatchments) gives byte-identical `.out` files with and without the patch in both engines. Example1 cannot be used for this in 5.3.0, patched or not: its file holds NaN pollutant concentrations (see NUM-01), which `readFloat()` already rejects with error 335. 6.0.0 has a separate problem with its own files for models with subcatchments: it writes no subcatchment records, so it cannot read them back.
