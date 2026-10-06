# IO-36: A relative RDII interface file name is resolved against the working directory, not the input file's folder

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | When SWMM is not started from the project folder (GUI, batch script, API host), `USE RDII` fails with ERROR 343 although the file is next to the .inp, and `SAVE RDII` writes the file into the working directory, possibly over an unrelated file, while `SAVE OUTFLOWS` in the same [FILES] section goes next to the .inp |
| **Reached from** | `[FILES] USE RDII` or `SAVE RDII` with a relative file name |
| **5.3.0** | `iface_readFileParams()` in [`src/legacy/engine/iface.c:148`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/iface.c#L148) |
| **5.2.4** | Same code, [`src/solver/iface.c:118`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/iface.c#L118) |
| **6.0.0** | Fixed already: `PostParseResolver` resolves the RDII path with the other interface files ([`src/engine/input/PostParseResolver.cpp:146`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/input/PostParseResolver.cpp#L146)) |
| **Since** | 5.2.0, which added `addAbsolutePath()` to the other file names (EPA commit 65e8435e) |
| **Fix** | Pass the name through `addAbsolutePath()`: [`IO-36_swmm530.patch`](IO-36_swmm530.patch) |

## The problem

Since 5.2.0, SWMM prefixes every relative file name in an input file with the input file's folder: rain gage files, the climate file, external time series and curve files, and in [FILES] the rainfall, runoff, hotstart, inflows and outflows files. The RDII interface file is the one exception. Its name is used as written, so the C library resolves it against the process's working directory.

That is the project folder only when SWMM is started from there. A GUI, a batch script that runs several projects, or a program that calls the toolkit usually has some other working directory. Then:

- `USE RDII "flows.rdii"` stops with `ERROR 343: cannot open RDII interface file flows.rdii`, although the file is next to the .inp, or silently reads a different `flows.rdii` that happens to be in the working directory;
- `SAVE RDII "flows.rdii"` writes the file into the working directory, replacing any file of that name there, while `SAVE OUTFLOWS` from the same [FILES] section goes next to the .inp.

## Why it happens

```c
// src/legacy/engine/iface.c, iface_readFileParams()
      case RUNOFF_FILE:
        Frunoff.mode = k;
        sstrncpy(Frunoff.name, addAbsolutePath(fname), MAXFNAME);
        break;
      ...
      case RDII_FILE:
        Frdii.mode = k;
        sstrncpy(Frdii.name, fname, MAXFNAME);          // no addAbsolutePath()
        break;

      case INFLOWS_FILE:
        ...
        sstrncpy(Finflows.name, addAbsolutePath(fname), MAXFNAME);
```

`rdii_openRdii()` and `openNewRdiiFile()` then call `fopen(Frdii.name, ...)` with the bare name.

## How to reproduce

| File | What it is |
|---|---|
| [`model/IO-36_use-rdii.inp`](model/IO-36_use-rdii.inp) | Node J1 with an [RDII] entry; `USE RDII "IO-36_in.rdii"` |
| [`model/IO-36_save-rdii.inp`](model/IO-36_save-rdii.inp) | The same model with `SAVE RDII "IO-36_out.rdii"` and `SAVE OUTFLOWS "IO-36_outflows.txt"` |
| [`IO-36_test.c`](IO-36_test.c) | Runs both decks from the issue folder, i.e. with `model/` as the input file's folder and its parent as the working directory, through the legacy toolkit. It first writes `model/IO-36_in.rdii` (binary format, 1.0 cfs at J1). The USE run must find it; the SAVE run must write both files into `model/` |
| [`IO-36_test6.c`](IO-36_test6.c) | The same through the 6.0.0 C API |

```sh
tools/run-test.sh IO-36            # 5.2.4, 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh IO-36 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix**, 5.3.0 (5.2.4 prints the same):

```
USE RDII  "IO-36_in.rdii":  error 343, max RDII inflow at J1 0.000 cfs
SAVE run: error 0
  SAVE OUTFLOWS "IO-36_outflows.txt" written to: model/ (next to the .inp)
  SAVE RDII     "IO-36_out.rdii"     written to: working directory
FAIL: the RDII file name is not resolved against the input file's folder
IO-36 5.3.0 base: FAIL
```

**With the fix**, 5.3.0 behaves as 6.0.0 already does:

```
USE RDII  "IO-36_in.rdii":  error 0, max RDII inflow at J1 1.000 cfs
SAVE run: error 0
  SAVE OUTFLOWS "IO-36_outflows.txt" written to: model/ (next to the .inp)
  SAVE RDII     "IO-36_out.rdii"     written to: model/ (next to the .inp)
PASS: USE and SAVE RDII resolve a relative name against the input file's folder
IO-36 5.3.0 patched: PASS
```

## The fix

```diff
       case RDII_FILE:
         Frdii.mode = k;
-        sstrncpy(Frdii.name, fname, MAXFNAME);
+        sstrncpy(Frdii.name, addAbsolutePath(fname), MAXFNAME);
         break;
```

`addAbsolutePath()` leaves absolute names alone, so only relative RDII file names change, and only when SWMM runs with a working directory other than the input file's folder. A project that relied on the old behaviour, keeping its RDII file in the working directory and not next to the .inp, now gets ERROR 343 and needs the file moved or an absolute name. The scratch RDII file SWMM creates when there is no [FILES] RDII line does not go through this code and is unaffected.
