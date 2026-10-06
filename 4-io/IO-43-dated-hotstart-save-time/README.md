# IO-43: A hot start file saved at a given date holds the state after 45 seconds

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | 5.3.0 writes every `SAVE HOTSTART <file> <date> <time>` file about 44 s into the run, whatever date is given. In the test the file asked for at 03:00 holds the state at 00:00:45.5: C1 flow 0.0006 cfs instead of 5.97 cfs, J1 depth 0.018 ft instead of 1.03 ft. A run restarted from it starts almost dry. Nothing warns. 6.0.0 reads the same line but writes no file at all, also without a warning |
| **Reached from** | `[FILES]` `SAVE HOTSTART Fname Date Time`, the form 5.3.0 added for saving the state at set times (up to 10 files) |
| **5.3.0** | `hotstart_save()` in [`src/legacy/engine/hotstart.c:111`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/hotstart.c#L111), with the date read in `iface_readFileParams()` at [`src/legacy/engine/iface.c:105`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/iface.c#L105) |
| **5.2.4** | Not affected: it has no dated saves. Its format is `SAVE HOTSTART Fname`; it ignores the extra tokens and writes the file at the end of the run ([`src/solver/iface.c:66`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/iface.c#L66)) |
| **6.0.0** | Reproduces differently: the date is parsed ([`src/engine/input/handlers/FilesHandler.cpp:106`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/handlers/FilesHandler.cpp#L106)), but the only save, at the end of the run, skips dated rows ([`src/engine/core/SWMMEngine.cpp:7409`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L7409)), so the file is never written |
| **Since** | 5.3.0, fork commit a409713b "Added support for saving multiple hotstart files at specified times #150" (January 2024) |
| **Fix** | Compare the date of the current routing time with the requested date: [`IO-43_swmm530.patch`](IO-43_swmm530.patch); write dated files during the run: [`IO-43_swmm600.patch`](IO-43_swmm600.patch) |

## The problem

5.3.0 accepts a date and time after the file name on a `SAVE HOTSTART` line:

```
[FILES]
SAVE HOTSTART  IO-43_0300.hsf  01/01/2020  03:00:00
```

The file is meant to hold the state of the run at that moment, so that a later run can start from it, for example a forecast restarted from the state at a set hour. The test runs a two-conduit network from 00:00 to 06:00 with an inflow that ramps from 0 to 12 cfs and asks for the state at 03:00.

5.3.0 writes the file after 45.5 seconds of simulation. At that point the network is still nearly empty: C1 carries 0.0006 cfs and J1 is 0.018 ft deep, against 5.97 cfs and 1.03 ft at 03:00. Any date after 1900 gives the same result: the file is written at the first routing step after about 44 s and never again. A user who restarts from it gets near-initial conditions, and the report of the saving run says nothing.

6.0.0 parses the date and keeps it, but never writes the file.

## Why it happens

`iface_readFileParams()` turns the date and time into a SWMM `DateTime`, a number of days since 30 December 1899. 01/01/2020 03:00 is 43831.125:

```c
// src/legacy/engine/iface.c, iface_readFileParams()
        saveDateTime = saveDate + saveTime;
...
                    if(saveDateTime > 0)
                        FhotstartOutputs[i].saveDateTime = saveDateTime;
```

`hotstart_save()`, which `swmm_step()` calls after every routing step, compares it with `NewRoutingTime`, the elapsed routing time in milliseconds (`globals.h`: "Current routing time (msec)"):

```c
// src/legacy/engine/hotstart.c, hotstart_save()
        if (FhotstartOutputs[i].file && 
            FhotstartOutputs[i].saveDateTime > 0 && 
            NewRoutingTime >= FhotstartOutputs[i].saveDateTime)
        {
            saveRunoff(&FhotstartOutputs[i]);
            saveRouting(&FhotstartOutputs[i]);
            fclose(FhotstartOutputs[i].file);
```

43831.125 ms is 43.8 s, so the test passes at the first step after 43.8 s (00:00:45.5, as the 5-second steps start with a 0.5-second one). The file is then closed and the requested time never gets a save.

6.0.0 stores the date in `HotstartSaveEntry::datetime`. Its only hot start save is in `SWMMEngine::end()`, and that loop skips every entry with a date:

```cpp
// src/engine/core/SWMMEngine.cpp, SWMMEngine::end()
    for (const auto& entry : ctx_.files.hotstart_saves) {
        if (entry.datetime != 0.0) continue;   // intermediate save — not yet
```

## How to reproduce

| File | What it is |
|---|---|
| [`IO-43_dated-save.inp`](IO-43_dated-save.inp) | J1 - C1 - J2 - C2 - O1, inflow ramping from 0 to 12 cfs over 00:00-06:00, `SAVE HOTSTART IO-43_0300.hsf 01/01/2020 03:00:00` |
| [`IO-43_test.c`](IO-43_test.c) | Records J1 depth and C1 flow after every step, decodes the hot start file and finds the step whose state it holds (5.2.4 and 5.3.0) |
| [`IO-43_test6.c`](IO-43_test6.c) | The same with the 6.0.0 API |

```sh
tools/run-test.sh IO-43            # 5.2.4: PASS; 5.3.0, 6.0.0: FAIL
tools/run-test.sh IO-43 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix**:

```
---- IO-43 on 5.3.0 (base) ----
                                     J1 depth    C1 flow
                                         (ft)      (cfs)
run at 03:00:00.5                      1.0335     5.9748
run at 06:00:00.0 (end)                1.7425    11.9710
IO-43_0300.hsf                         0.0180     0.0006
The file holds the state of the run at 00:00:45.5
FAIL: the hot start file should hold the state at 03:00:00.5; it holds the state at 00:00:45.5 (C1 0.0006 cfs instead of 5.9748 cfs)
IO-43 5.3.0 base: FAIL
---- IO-43 on 6.0.0 (base) ----
run at 03:00:00.5                      1.0335     5.9748
run at 06:00:00.0 (end)                1.7425    11.9710
FAIL: IO-43_0300.hsf, asked for at 03:00:00, was not written
IO-43 6.0.0 base: FAIL
```

5.2.4 ignores the date and writes the end-of-run state, as its manual describes:

```
IO-43_0300.hsf                         1.7425    11.9710
The file holds the state of the run at 06:00:00.0
PASS: 5.2.4 has no dated saves; it ignores the date and saves at the end of the run, as its manual documents
IO-43 5.2.4 base: PASS
```

**With the fix**, both engines write the state after the first step at or past 03:00, and the two files are byte-identical:

```
IO-43_0300.hsf                         1.0335     5.9748
The file holds the state of the run at 03:00:00.5
PASS: the hot start file holds the state at the requested 03:00:00
IO-43 5.3.0 patched: PASS
...
IO-43 6.0.0 patched: PASS
```

## The fix

5.3.0: compare the date of the current routing time with the requested date. `getDateTime()` is the function the rest of the engine uses for this:

```diff
         if (FhotstartOutputs[i].file && 
             FhotstartOutputs[i].saveDateTime > 0 && 
-            NewRoutingTime >= FhotstartOutputs[i].saveDateTime)
+            getDateTime(NewRoutingTime) >= FhotstartOutputs[i].saveDateTime)
```

6.0.0: at the end of `step()`, write each dated file at the step whose date, computed as legacy `getDateTime()` does, first reaches the requested date, in the same format as the end-of-run save. A date at or before the start is written after the first step, as in 5.3.0:

```diff
+    for (const auto& entry : ctx_.files.hotstart_saves) {
+        if (entry.datetime == 0.0) continue;   // end-of-run save, see end()
+        const double now  = datetime::addSeconds(ctx_.options.start_date,
+                                                 (ctx_.elapsed_ms + 1.0) / 1000.0);
+        const double prev = datetime::addSeconds(ctx_.options.start_date,
+                                                 (hs_prev_ms + 1.0) / 1000.0);
+        if (now < entry.datetime || (hs_prev_ms > 0.0 && prev >= entry.datetime))
+            continue;                          // not reached, or written before
```

Only runs with a dated `SAVE HOTSTART` line are affected. No simulated value changes.

Not changed here: in 5.3.0 a file whose date falls after the end of the run keeps only its 39-byte header (the file is opened at the start and only `hotstart_save()` closes a dated file); 6.0.0 with the patch writes no file in that case. Neither warns.
