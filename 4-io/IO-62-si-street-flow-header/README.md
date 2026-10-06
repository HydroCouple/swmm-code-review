# IO-62: The SI Street Flow Summary has no "Count" label over the inlet count

| | |
|---|---|
| **Category** | [Input, report and output files](../README.md) |
| **Impact** | Cosmetic. In a model with SI flow units, the column that holds the number of inlets is headed only "Inlet" (from the line above), with a blank where US reports print "Count". The numbers are right and the columns line up. |
| **Reached from** | Any model with `[INLET_USAGE]` and SI flow units (CMS, LPS, MLD) |
| **5.3.0** | `writeStreetStatsHeader()` in [`src/legacy/engine/inlet.c:1047`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/inlet.c#L1047) |
| **5.2.4** | Same code, [`src/solver/inlet.c:1047`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/inlet.c#L1047) |
| **6.0.0** | Not affected: it writes a separate "Street Inlet Flow Summary" table with a "Count" column in all unit systems ([`src/engine/plugins/DefaultReportPlugin.cpp:3177`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/plugins/DefaultReportPlugin.cpp#L3177)) |
| **Since** | 5.2.0, when inlets were added |
| **Fix** | Add the label to the SI header line: [`IO-62_swmm530.patch`](IO-62_swmm530.patch) |

## The problem

The last line of the Street Flow Summary header is written in two versions, one per unit system. The US version labels the inlet count column; the SI version has spaces in its place:

```c
// src/legacy/engine/inlet.c, writeStreetStatsHeader()
if (UnitSystem == US) fprintf(Frpt.file,
"\n  Street Conduit         %3s        ft        ft  Design            Location  Count     Pcnt ...",
...
else fprintf(Frpt.file,
"\n  Street Conduit         %3s         m         m  Design            Location            Pcnt ...",
```

`writeStreetStats()` prints the count (`%5d`) in both cases, so an SI report shows an unlabelled column of integers between "Location" and the first "Pcnt" column. Both strings have the same length, so nothing else is misaligned.

## How to reproduce

| File | What it is |
|---|---|
| [`IO-62_si-street.inp`](IO-62_si-street.inp) | One street in CMS units with 2 grate inlets |
| [`IO-62_test.c`](IO-62_test.c) | Legacy toolkit (5.2.4, 5.3.0). Runs the deck, reads the report, and checks that the inlet table header has a "Count" label ending over the inlet count of the street's row |
| [`IO-62_test6.c`](IO-62_test6.c) | The same check on the 6.0.0 report |

```sh
tools/run-test.sh IO-62            # 5.2.4 and 5.3.0: FAIL, 6.0.0: PASS
tools/run-test.sh IO-62 --patched  # 5.3.0 with the fix and 6.0.0: PASS
```

**Without the fix** (5.2.4 and 5.3.0):

```
header: ...Design            Location            Pcnt
row:    ...Grate             ON-GRADE      2    85.31
FAIL: the SI inlet table header has no "Count" label for the inlet count column
IO-62 5.2.4 base: FAIL
IO-62 5.3.0 base: FAIL
```

6.0.0:

```
header: ...Design       Placement  Count     CMS     
row:    ...             ON-GRADE       2     0.051   
PASS: the SI inlet table labels the inlet count column "Count"
IO-62 6.0.0 base: PASS
```

**With the fix:**

```
header: ...Design            Location  Count     Pcnt
row:    ...Grate             ON-GRADE      2    85.31
PASS: the SI inlet table labels the inlet count column "Count"
IO-62 5.3.0 patched: PASS
```

## The fix

```diff
-"\n  Street Conduit         %3s         m         m  Design            Location            Pcnt     Pcnt     Pcnt     Pcnt      %3s      %3s",
+"\n  Street Conduit         %3s         m         m  Design            Location  Count     Pcnt     Pcnt     Pcnt     Pcnt      %3s      %3s",
```

Only the header text of SI reports changes. Tools that parse the report by column position are not affected.
