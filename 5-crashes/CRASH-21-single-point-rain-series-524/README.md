# CRASH-21: 5.2.4 converts 8.64e14 to int when a rain gage's time series has one record

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Undefined behaviour while the project is validated, on valid input. On x86-64 the conversion gives `INT_MIN` and the interval checks pass by accident; on ARM64 the instruction saturates to `INT_MAX`, which should give a spurious WARNING 09 ("time series interval greater than recording interval"; reasoned from the instruction semantics, not run). It stops every sanitizer build of 5.2.4 on 29 of the review's 249 test decks, including 24 of EPA's 25 `swc` decks, which give a second gage a one-record dummy series. |
| **Reached from** | Any input file with a rain gage (used by a subcatchment) whose `TIMESERIES` has a single record, e.g. a design pulse or a `TS1 0 0` placeholder |
| **5.3.0** | Fixed: `gage_validate()` skips the interval checks for a one-record series, [`src/legacy/engine/gage.c:263`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L263) |
| **5.2.4** | `gage_validate()` in [`src/solver/gage.c:251`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/gage.c#L251) |
| **6.0.0** | Not affected: the interval check runs only for a series with more than one point ([`src/engine/core/SWMMEngine.cpp:7852`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/core/SWMMEngine.cpp#L7852)) |
| **Since** | Every release up to 5.2.4 (the code is in the initial commit of EPA's repository); EPA's `develop` branch still has it |
| **Fix** | None here (5.2.4 is not patched). Fixed in the OpenSWMM fork by commit 678a9e87, which 5.3.0 contains |

## The problem

When a rain gage reads a time series, `gage_validate()` compares the gage's recording interval with the smallest spacing between the series' records: a recording interval larger than the spacing is ERROR 159, a smaller one WARNING 09. A series with a single record has no spacing. `table_validate()` computes the smallest spacing `dxMin` by starting from `BIG` (1e10 days) and lowering it for each pair of records, so for one record it stays at 1e10 days. 5.2.4 converts that to seconds and then to `int`:

```c
// src/solver/gage.c (5.2.4), gage_validate()
gageInterval = (int)(floor(Tseries[k].dxMin*SECperDAY + 0.5));   // 8.64e14 s
if ( gageInterval > 0 && Gage[j].rainInterval > gageInterval )
{
    report_writeErrorMsg(ERR_RAIN_GAGE_INTERVAL, Gage[j].ID);
}
if ( Gage[j].rainInterval < gageInterval )
{
    report_writeWarningMsg(WARN09, Gage[j].ID);
}
```

8.64e14 does not fit in an `int`, and converting an out-of-range floating-point value to an integer type is undefined behaviour (C17 6.3.1.4). What the program does depends on the processor:

- x86-64 (`cvttsd2si`) returns `INT_MIN`. Both tests are false and nothing is reported, which happens to be the right outcome.
- ARM64 (`fcvtzs`) saturates to `INT_MAX`. The second test is then true, so the report should get WARNING 09 for a series that has no interval at all (not run on ARM64 for this review).

A one-record series is common. A design storm pulse is naturally written as one record, and EPA's own regression decks `swc1` to `swc25` (except `swc17`) give a second gage the placeholder series `TS1  0  0`. A sanitizer build of 5.2.4 stops on all of them during `swmm_open()`. The review's harness suppresses this one report (`tools/ubsan.supp`) so that other tests can run on 5.2.4; this issue's folder turns the suppression off.

## Why it happens

`table_validate()` initialises `dxMin` to `BIG` and only updates it inside the loop over consecutive pairs of entries, so a single-entry table keeps the sentinel. `gage_validate()` uses `dxMin` without checking for it.

5.3.0 guards the computation ([gage.c:263-276](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/gage.c#L263-L276)), added in the OpenSWMM fork by commit 678a9e87 ("skip recording-interval checks for single-point rain series"):

```c
// src/legacy/engine/gage.c (5.3.0), gage_validate()
// --- a single-point time series never updates dxMin from its BIG
//     sentinel, so it has no meaningful recording interval to check
if ( Tseries[k].dxMin < BIG )
{
    gageInterval = (int)(floor(Tseries[k].dxMin*SECperDAY + 0.5));
    ...
}
```

6.0.0 computes the spacing itself and only for `tx.size() > 1`.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-21_one-record.inp`](CRASH-21_one-record.inp) | One impervious subcatchment on a gage whose series is one record, 1.0 in/hr at 0:00 with a 1-hour recording interval; 1-hour run |
| [`CRASH-21_test.c`](CRASH-21_test.c) | Runs the deck through the legacy toolkit (5.2.4 and 5.3.0); checks that the report has no WARNING 09 or ERROR 159 and 1.000 in of precipitation |
| [`CRASH-21_test6.c`](CRASH-21_test6.c) | The same check through 6.0.0's `swmm_engine_run()` |
| `no-ubsan-suppressions` | Tells `tools/run-test.sh` not to suppress this report for this issue |

```sh
tools/run-test.sh CRASH-21   # 5.2.4: CRASH; 5.3.0 and 6.0.0: PASS
```

There is no `--patched` run, since 5.3.0 and 6.0.0 are already correct.

**5.2.4** reports the conversion inside `swmm_open()`. On x86-64 the run then gives the right answers, so only the sanitizer shows the defect:

```
../src/src/solver/gage.c:251:24: runtime error: 8.64e+14 is outside the range of representable values of type 'int'
    #0 in gage_validate gage.c:251:24
    #1 in project_validate project.c:217:44
    #2 in swmm_open swmm5.c:299:9
run error code            0
WARNING 09 / ERROR 159    0 / 0   (expected 0 / 0)
Total Precipitation (in)  1.000   (expected 1.000)
PASS: the one-record series validates without interval messages and gives 1.000 in
CRASH-21 5.2.4 base: CRASH
```

**5.3.0 and 6.0.0** print the same lines without a sanitizer report:

```
run error code            0
WARNING 09 / ERROR 159    0 / 0   (expected 0 / 0)
Total Precipitation (in)  1.000   (expected 1.000)
PASS: the one-record series validates without interval messages and gives 1.000 in
CRASH-21 5.3.0 base: PASS
CRASH-21 6.0.0 base: PASS
```

## The fix

5.3.0 already has the fix above. For 5.2.4 (and EPA's `develop` branch, which has the same line 251), the change of commit 678a9e87 carries over directly, with the fork's `gageIndex` written as `j`: it puts the computation and both interval tests inside `if ( Tseries[k].dxMin < BIG )`. A series with two or more records gives bit-identical results; a one-record series no longer depends on how the processor converts an out-of-range value.
