# IO-37: USE HOTSTART, RAINFALL and RUNOFF files must be writable to be read

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Valid input rejected. A hot start, rainfall or runoff interface file that the user can read but not write stops the run with `ERROR 331: cannot open hot start interface file`, `ERROR 315: cannot open rainfall interface file` or `ERROR 323: cannot open runoff interface file`. Typical cases: archived or shared scenario libraries, files checked out or downloaded read-only, read-only container or network mounts, files owned by another user. The message suggests the file is missing or damaged |
| **Reached from** | `[FILES]` lines `USE HOTSTART`, `USE RAINFALL` and `USE RUNOFF`, and the 5.3.0 toolkit's hot start check (`hotstart_is_valid()`, also called from `swmm5.c`) |
| **5.3.0** | `fopen(..., "r+b")` in [`src/legacy/engine/hotstart.c:173`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/hotstart.c#L173) and [`:257`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/hotstart.c#L257), [`rain.c:153`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/rain.c#L153), [`runoff.c:111`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/runoff.c#L111) |
| **5.2.4** | Same mode, [`src/solver/hotstart.c:116`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/hotstart.c#L116), [`rain.c:153`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/rain.c#L153), [`runoff.c:109`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/runoff.c#L109) |
| **6.0.0** | Not affected: hot start files are read with `std::ifstream` ([`src/engine/core/HotStartManager.cpp:1288`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/HotStartManager.cpp#L1288)) and runoff files with `"rb"` ([`src/engine/hydrology/RunoffInterface.cpp:90`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydrology/RunoffInterface.cpp#L90)); 6.0.0 does not implement rainfall interface files (USE RAINFALL is ignored with a warning) |
| **Since** | Rainfall and runoff files: every release in the repository (initial commit, 2014); hot start files: since they were split into their own module (hotstart.c, 5.1 series) |
| **Fix** | Open USE-mode files with `"rb"`: [`IO-37_swmm530.patch`](IO-37_swmm530.patch) |

## The problem

In USE mode an interface file is input only: SWMM reads the hot start state, the collated rainfall or the saved runoff from it and never writes to it. The legacy code nevertheless opens it for update, `fopen(name, "r+b")`, which requires write permission. When the operating system refuses write access, the open fails and the run stops before it starts.

In the test, the three files are written by a first run, made read-only (`chmod 444`), and read back by three runs as an ordinary user. 5.2.4 and 5.3.0 reject all three:

```
ERROR 331: cannot open hot start interface file IO-37.hsf.
ERROR 315: cannot open rainfall interface file IO-37.rff.
ERROR 323: cannot open runoff interface file IO-37.rof.
```

(the reports give the full path; the test strips the folder). The files are intact; making them writable again, or running as an administrator, makes the same runs succeed, which is why the problem is easy to miss: the review's sanitizer harness runs as root, and root may write to any file whatever its permission bits.

## Why it happens

```c
// src/legacy/engine/hotstart.c, hotstart_is_valid() and initializeFromHotstartFile()
	if ( (f = fopen(hotstartFile, "r+b")) == NULL)
	...
    FhotstartInput.file = fopen(FhotstartInput.name, "r+b");

// src/legacy/engine/rain.c, rain_open()
      case USE_FILE:
        if ( (Frain.file = fopen(Frain.name, "r+b")) == NULL)

// src/legacy/engine/runoff.c, runoff_open()
      case USE_FILE:
        if ( (Frunoff.file = fopen(Frunoff.name, "r+b")) == NULL)
```

`"r+"` opens an existing file for reading and writing, so `fopen()` fails with `EACCES` (or `EROFS` on a read-only file system) when only reading is allowed. None of these handles is ever written: in USE mode the rainfall file is not rebuilt (`createRainFile()` runs only for SCRATCH and SAVE files), `runoff_initFile()` writes its header only in SAVE mode, and the hot start reader only reads. SAVE-mode and scratch files are opened separately with `"w+b"`. The RDII and routing inflow interface files, by contrast, are already opened with `"rb"` and `"rt"` (`rdii.c:474`, `iface.c:419`).

## How to reproduce

| File | What it is |
|---|---|
| [`IO-37_save-hotstart.inp`](IO-37_save-hotstart.inp) | A 1-cfs inflow through one conduit; `SAVE HOTSTART "IO-37.hsf"` |
| [`IO-37_use-hotstart.inp`](IO-37_use-hotstart.inp) | The same model with `USE HOTSTART "IO-37.hsf"` |
| [`IO-37_save-interface.inp`](IO-37_save-interface.inp) | One subcatchment on a rain-file gage; `SAVE RAINFALL "IO-37.rff"`, `SAVE RUNOFF "IO-37.rof"` |
| [`IO-37_use-rainfall.inp`](IO-37_use-rainfall.inp), [`IO-37_use-runoff.inp`](IO-37_use-runoff.inp) | The same model with `USE RAINFALL` and `USE RUNOFF` |
| [`IO-37_rain.dat`](IO-37_rain.dat) | The gage's rainfall file (0.5 in and 0.25 in in the first two hours) |
| [`IO-37_test.c`](IO-37_test.c) | Runs the two SAVE decks, makes the three files read-only, switches from root to user 65534 if needed (root ignores permissions), and runs the three USE decks; each must run without an error |
| [`IO-37_test6.c`](IO-37_test6.c) | The same for 6.0.0, with the hot start and runoff files only |

```sh
tools/run-test.sh IO-37            # 5.2.4, 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-37 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.2.4 and 5.3.0):

```
Running as uid 65534; interface files writable by this user: no, no, no
  deck           reads       error   first error in report
  use-hotstart   IO-37.hsf     331   ERROR 331: cannot open hot start interface file IO-37.hsf.
  use-rainfall   IO-37.rff     315   ERROR 315: cannot open rainfall interface file IO-37.rff.
  use-runoff     IO-37.rof     323   ERROR 323: cannot open runoff interface file IO-37.rof.
  (correct: all three read their read-only file and run)
FAIL: 3 of 3 read-only interface files cannot be used: they are opened for writing
IO-37 5.3.0 base: FAIL
```

6.0.0 reads its read-only hot start and runoff files (`IO-37 6.0.0 base: PASS`).

**With the fix:**

```
Running as uid 65534; interface files writable by this user: no, no, no
  deck           reads       error   first error in report
  use-hotstart   IO-37.hsf       0   -
  use-rainfall   IO-37.rff       0   -
  use-runoff     IO-37.rof       0   -
  (correct: all three read their read-only file and run)
PASS: read-only hot start, rainfall and runoff interface files are used
IO-37 5.3.0 patched: PASS
```

## The fix

Change the mode of the four opens from `"r+b"` to `"rb"`:

```diff
-	if ( (f = fopen(hotstartFile, "r+b")) == NULL)
+	if ( (f = fopen(hotstartFile, "rb")) == NULL)
-    FhotstartInput.file = fopen(FhotstartInput.name, "r+b");
+    FhotstartInput.file = fopen(FhotstartInput.name, "rb");
-        if ( (Frain.file = fopen(Frain.name, "r+b")) == NULL)
+        if ( (Frain.file = fopen(Frain.name, "rb")) == NULL)
-        if ( (Frunoff.file = fopen(Frunoff.name, "r+b")) == NULL)
+        if ( (Frunoff.file = fopen(Frunoff.name, "rb")) == NULL)
```

Reading, seeking and the data read are the same in both modes, so writable files give identical results.

**Effect on other models.** None of the regression decks uses a USE file; for writable files the change has no effect.

**Noticed while testing.** 6.0.0 cannot read back its own hot start file for a model with subcatchments: `save_legacy_routing()` writes the subcatchment count but no subcatchment records, while the reader expects (and skips) them, so `USE HOTSTART` fails with `Error initializing: USE HOTSTART:`. 6.0.0 reads hot start files written by 5.3.0 for the same model. The test's hot start model therefore has no subcatchments.
