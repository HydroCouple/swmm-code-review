# API-16: The output reader returns another element's results for an out-of-range index, period or attribute

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | An off-by-one element index, an end period past the last period, or a pollutant attribute in a file without pollutants returns error 0 and plausible numbers from somewhere else in the file: node index 4 of 4 gives the flow in the first link, link index 3 of 3 gives the system results, "pollutant 0" gives the next node's depth, and periods past the end give whatever was on the stack. Nothing warns the caller. |
| **Reached from** | `SMO_get{Subcatch,Node,Link,System}Series`, `SMO_get{Subcatch,Node,Link,System}Attribute`, `SMO_get{Subcatch,Node,Link}Result`, and the Python `Output` methods built on them |
| **5.3.0** | Index tests `index > count` in [`swmm_output.c:1151`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1151), [1335](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1335), [1371](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1371), [1568](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1568), [1607](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1607), [1648](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1648); end period never tested against `Nperiods` ([1153-1154](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1153-L1154) and the three other series); the attribute is added to the file offset unchecked in [`getSubcatchValue()` .. `getSystemValue()`, 1907-2020](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1907-L2020) |
| **5.2.4** | Same code: [`src/outfile/swmm_output.c:553-556`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/outfile/swmm_output.c#L553-L556), [591-594](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/outfile/swmm_output.c#L591-L594), [628-631](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/outfile/swmm_output.c#L628-L631), [665](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/outfile/swmm_output.c#L665), [820](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/outfile/swmm_output.c#L820), [858](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/outfile/swmm_output.c#L858), [898](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/outfile/swmm_output.c#L898), [1111-1182](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/outfile/swmm_output.c#L1111-L1182) |
| **6.0.0** | Not affected: [`OutputReader`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/output/OutputReader.cpp#L203-L306) checks the element index, the period range and the variable index in every getter |
| **Since** | 5.1.14, the first release that ships the output library (`src/outfile`) |
| **Fix** | Test `endPeriod > Nperiods`, `index >= count` and `attr` against the number of variables: [`API-16_swmm530.patch`](API-16_swmm530.patch) (requires [CRASH-19](../../5-crashes/CRASH-19-output-reader-null-handle/)) |

## The problem

The reader's getters take zero-based element indices, a period range with an exclusive end (`startPeriod <= p < endPeriod`) and an attribute code. For an argument out of range they are meant to return error 421 ("invalid parameter code"), 422 ("reporting period index out of range") or 423 ("element index out of range"). In three cases they return 0 instead and read from a computed file offset that belongs to something else:

- **Element index equal to the element count.** The file stores, per period, all subcatchments, then all nodes, then all links, then the system. Index `Nnodes` is therefore the first link, index `Nlinks` is the system record, and index `Nsubcatch` is the first node.
- **End period past the last period.** Values after the last period are read from the file's closing records and then past the end of the file, where `fread` fails and the helper returns an uninitialised local `float`.
- **Attribute index past the last variable.** The attribute is an offset within the element's record, so "pollutant 0" in a file without pollutants is the first variable of the next element. 5.3.0's own Python wrapper passes `attribute.value + sub_index` straight through ([`python/openswmm/legacy/output/_output.pyx:1027`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/python/openswmm/legacy/output/_output.pyx#L1027)), so `get_node_series(j, POLLUTANT_CONCENTRATION, sub_index=k)` with `k` at or above the pollutant count returns another node's depth, head or volume.

For the test deck (1 subcatchment, nodes J1 J2 J3 O1, links C1 C2 C3, no pollutants, 24 periods), 5.3.0 returns:

| Call | Error | Values returned | They are |
|---|---|---|---|
| `getNodeSeries(index 4, depth, 0, 4)` | 0 | 2.874 0.9648 0.3758 0.1939 | flow in C1 (cfs) |
| `getLinkSeries(index 3, flow, 0, 4)` | 0 | 70 70 70 70 | air temperature (deg F) |
| `getLinkResult(period 0, index 3)` | 0 | 70 0 0 0.5 3.342 | the system record |
| `getSubcatchSeries(index 1, runoff, 0, 4)` | 0 | 3.209 0.8487 0.3399 0.1744 | total inflow to J1 |
| `getNodeSeries(0, depth, 22, 27)` | 0 | 0.02186 0.02082 0 0 0 | J1 depth for periods 22-23, then nothing (uninitialised) |
| `getNodeSeries(0, pollutant 0, 0, 4)` | 0 | 0.6477 0.3649 0.229 0.1668 | depth at J2 |
| `getNodeAttribute(period 1, pollutant 0)` | 0 | 0.3649 0.3884 0.4115 0.9648 | depths at J2, J3, O1, then flow in C1 |
| `getSystemSeries(attribute 15, 0, 4)` | 0 | -3.032e-13 2.229e-42 1.466e+13 -3.032e-13 | the next period's date (a double) read as floats |

The numbers are plausible, so a caller has no way to notice.

## Why it happens

The series getters check the start period but not the end, and compare the index with `>`:

```c
// src/legacy/output/swmm_output.c, SMO_getNodeSeries()
    else if (nodeIndex < 0 || nodeIndex > p_data->Nnodes)      // should be >=
        errorcode = ERR423;
    else if (startPeriod < 0 || startPeriod >= p_data->Nperiods ||
             endPeriod <= startPeriod)                         // endPeriod > Nperiods passes
        errorcode = ERR422;
    // Check memory for outValues
    else if MEMCHECK (temp = newFloatArray(len = endPeriod - startPeriod))
        errorcode = ERR411;
    else
    {
        for (k = 0; k < len; k++)
            temp[k] = getNodeValue(p_data, startPeriod + k, nodeIndex, attr);
```

`SMO_getSubcatchResult()`, `SMO_getNodeResult()` and `SMO_getLinkResult()` have the same `>` test. No public function checks `attr`; the helpers add it straight to the offset and ignore `fread`'s result:

```c
// getNodeValue()
    offset += RECORDSIZE * (p_data->Nsubcatch * p_data->SubcatchVars +
                            nodeIndex * p_data->NodeVars + attr);
    _fseek(p_data->file, offset, SEEK_SET);
    fread(&value, RECORDSIZE, 1, p_data->file);    // value stays uninitialised at EOF
    return value;
```

The element-name and property getters (`SMO_getElementName`, `SMO_getPropertyValue(s)`) test `index >= count` correctly; only the result getters have the off-by-one.

## How to reproduce

| File | What it is |
|---|---|
| [`API-16_reader.inp`](API-16_reader.inp) | One subcatchment, junctions J1-J3, outfall O1, conduits C1-C3, no pollutants; 6 hours reported every 15 minutes (24 periods) |
| [`API-16_test.c`](API-16_test.c) | Runs the deck and makes 18 calls with one out-of-range argument each (17 on 5.2.4, see below), 3 valid calls at the edges of the ranges, and reference calls that show where the bad reads land |
| [`API-16_test6.c`](API-16_test6.c) | The same classes of argument through 6.0.0's `swmm_output_*` functions (inclusive end period there) |

```sh
tools/run-test.sh API-16            # 5.2.4 and 5.3.0: FAIL; 6.0.0: PASS
tools/run-test.sh API-16 --patched  # 5.3.0 with the fix (and CRASH-19): PASS
```

On 5.2.4 the test skips `SMO_getSystemAttribute()`: in 5.2.4 that function returns the address of a local variable, which AddressSanitizer stops on before the range check matters. 5.3.0 allocates the result and is tested.

**Without the fix**, every out-of-range call returns error 0 (5.3.0, excerpt; 5.2.4 gives the same pattern with its own slightly different hydraulics):

```
API-16.out: 1 subcatchment, 4 nodes, 3 links, 0 pollutants, 24 periods

  call (out-of-range argument)                 error  data returned
  getSubcatchSeries(index 1, runoff, 0, 4)         0  len  4: 3.209 0.8487 0.3399 0.1744
  getNodeSeries(index 4, depth, 0, 4)              0  len  4: 2.874 0.9648 0.3758 0.1939
  getLinkSeries(index 3, flow, 0, 4)               0  len  4: 70 70 70 70
  getLinkResult(period 0, index 3)                 0  len  5: 70 0 0 0.5 3.342
  getNodeSeries(0, depth, 22, 27)                  0  len  5: 0.02186 0.02082 0 0 0
  getNodeSeries(0, pollutant 0, 0, 4)              0  len  4: 0.6477 0.3649 0.229 0.1668
  getSystemSeries(attribute 15, 0, 4)              0  len  4: -3.032e-13 2.229e-42 1.466e+13 -3.032e-13
  getNodeAttribute(period 1, pollutant 0)          0  len  4: 0.3649 0.3884 0.4115 0.9648
  getSystemAttribute(period 1, attribute 15)       0  len  1: 2.229e-42

  reference (valid call)                       error  data returned
  getNodeSeries(0, total inflow, 0, 4) [J1]        0  len  4: 3.209 0.8487 0.3399 0.1744
  getLinkSeries(0, flow, 0, 4)         [C1]        0  len  4: 2.874 0.9648 0.3758 0.1939
  getSystemSeries(air temp, 0, 4)                  0  len  4: 70 70 70 70
  getSystemResult(period 0)                        0  len 15: 70 0 0 0.5 3.342
  getNodeSeries(1, depth, 0, 4)        [J2]        0  len  4: 0.6477 0.3649 0.229 0.1668
FAIL: out-of-range arguments returned error 0 and data from elsewhere in the file
API-16 5.3.0 base: FAIL
```

```
API-16 5.2.4 base: FAIL
```

6.0.0 returns -1 for all 18 out-of-range calls and 0 for the valid ones:

```
  get_node_series(index 4, depth, 0, 3)               -1
  get_link_attribute(index 3, period 0)               -1
  get_node_series(0, depth, 22, 26)                   -1
  get_node_result(period 1, pollutant 0)              -1
  get_system_result(period 1, variable 15)            -1
  ...
PASS: every out-of-range index, end period and variable returned -1
API-16 6.0.0 base: PASS
```

**With the fix**, each call returns the error for the argument that is wrong, and the valid calls at the edges still succeed:

```
  call (out-of-range argument)                 error  data returned
  getSubcatchSeries(index 1, runoff, 0, 4)       423  
  getNodeSeries(index 4, depth, 0, 4)            423  
  getLinkSeries(index 3, flow, 0, 4)             423  
  getSubcatchResult(period 0, index 1)           423  
  getNodeResult(period 0, index 4)               423  
  getLinkResult(period 0, index 3)               423  
  getSubcatchSeries(0, runoff, 22, 27)           422  
  getNodeSeries(0, depth, 22, 27)                422  
  getLinkSeries(0, flow, 22, 27)                 422  
  getSystemSeries(runoff, 22, 27)                423  
  getSubcatchSeries(0, pollutant 0, 0, 4)        421  
  getNodeSeries(0, pollutant 0, 0, 4)            421  
  getLinkSeries(0, pollutant 0, 0, 4)            421  
  getSystemSeries(attribute 15, 0, 4)            421  
  getSubcatchAttribute(period 1, pollutant 0)    421  
  getNodeAttribute(period 1, pollutant 0)        421  
  getLinkAttribute(period 1, pollutant 0)        421  
  getSystemAttribute(period 1, attribute 15)     421  

  valid call (edge of range)                   error  data returned
  getNodeSeries(index 3, depth, 0, 24)             0  len 24: 0.5443 0.4115 0.2519 0.1812 0.1404
  getLinkResult(period 0, index 2)                 0  len  5: 2.09 0.5725 3.372 248 0.3508
  getSystemSeries(attribute 14, 0, 4)              0  len  4: 0 0 0 0
PASS: every out-of-range index, end period and attribute returned an error code
API-16 5.3.0 patched: PASS
```

`SMO_getSystemSeries()` already reported its period errors as 423; the patch keeps that code for the new end-period test.

## The fix

In the series getters (the node series shown; subcatchment, link and system series are the same):

```diff
-    else if (nodeIndex < 0 || nodeIndex > p_data->Nnodes)
+    else if (nodeIndex < 0 || nodeIndex >= p_data->Nnodes)
         errorcode = ERR423;
     else if (startPeriod < 0 || startPeriod >= p_data->Nperiods ||
-             endPeriod <= startPeriod)
+             endPeriod <= startPeriod || endPeriod > p_data->Nperiods)
         errorcode = ERR422;
+    else if ((int)attr < 0 || (int)attr >= p_data->NodeVars)
+        errorcode = ERR421;
```

The four attribute getters get the same `attr` test, and the three result getters `>=` instead of `>`. The variable counts (`SubcatchVars`, `NodeVars`, `LinkVars`, `SysVars`) are read from the file, so pollutant attributes up to the file's pollutant count stay valid. The `(int)` cast keeps the negative test meaningful when the compiler gives the enum an unsigned type.

Every in-range call returns the same values as before. The patch sits next to the NULL-handle lines changed by [CRASH-19](../../5-crashes/CRASH-19-output-reader-null-handle/), so it is written on top of that patch. 6.0.0 needs no change.
