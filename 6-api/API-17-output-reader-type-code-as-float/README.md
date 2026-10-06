# API-17: SMO_getPropertyValue() returns node and link type codes as the bits of an integer read into a float

| | |
|---|---|
| **Category** | [Toolkit API](../README.md) |
| **Impact** | The 5.3.0 output reader's property getters return an outfall's type code (1) as 1.4e-45, a storage unit's (2) as 2.8e-45, a pump's (1) as 1.4e-45 and so on. Junctions and conduits (type 0) happen to come back right; a comparison with 1-4 never matches, and rounding gives 0 for every element. No error is returned. |
| **Reached from** | `SMO_getPropertyValue(h, SMO_node or SMO_link, 0, j, &value)` and `SMO_getPropertyValues(h, SMO_node or SMO_link, j, ...)[0]`, and the Python `Output.get_property_value(s)` built on them |
| **5.3.0** | `SMO_getPropertyValue()` in [`src/legacy/output/swmm_output.c:1220`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1220) and [1235](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1235); `SMO_getPropertyValues()` at [1294](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1294) and [1309](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/output/swmm_output.c#L1309); the file layout is set by `output_open()` in [`src/legacy/engine/output.c:266-270`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L266-L270) and [290-315](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/output.c#L290-L315) |
| **5.2.4** | Not affected: its output library has no property getters |
| **6.0.0** | Not applicable: 6.0.0's output reader [skips the property block](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/output/OutputReader.cpp#L458-L485); type codes come from `swmm_node_get_type()` / `swmm_link_get_type()` as integers |
| **Since** | 5.3.0, fork commit b91f3107 (2025-03-03), which added the property getters |
| **Fix** | Read the type-code slot as an INT4 and convert it: [`API-17_swmm530.patch`](API-17_swmm530.patch) |

## The problem

5.3.0 added getters for the "object properties" block of the binary output file: subcatchment area; node type, invert and maximum depth; link type, offsets, full depth and length. Property 0 of a node or link is its type code (junction 0, outfall 1, storage 2, divider 3; conduit 0, pump 1, orifice 2, weir 3, outlet 4).

The getters return that code as a float whose bits are the integer's bits. For the test deck, which has one node and one link of each type:

| Element | Type code | `SMO_getPropertyValue(..., 0, ...)` |
|---|---|---|
| J1 junction | 0 | 0 |
| O1 outfall | 1 | 1.4013e-45 |
| SU1 storage | 2 | 2.8026e-45 |
| D1 divider | 3 | 4.2039e-45 |
| P1 pump | 1 | 1.4013e-45 |
| OR1 orifice | 2 | 2.8026e-45 |
| W1 weir | 3 | 4.2039e-45 |
| OL1 outlet | 4 | 5.60519e-45 |

The other properties (invert, depths, offsets, length) are stored as reals and are read correctly.

## Why it happens

The engine writes each node's record as one integer and two reals, and each link's as one integer and four reals:

```c
// src/legacy/engine/output.c, output_open()
        k = Node[j].type;
        NodeResults[0] = (REAL4)(Node[j].invertElev * UCF(LENGTH));
        NodeResults[1] = (REAL4)(Node[j].fullDepth * UCF(LENGTH));
        fwrite(&k, sizeof(INT4), 1, Fout.file);
        fwrite(NodeResults, sizeof(REAL4), 2, Fout.file);
```

The reader treats every slot as a `float`:

```c
// src/legacy/output/swmm_output.c, SMO_getPropertyValue(), case SMO_node
                _fseek(p_data->file, offset, SEEK_SET);
                fread(value, RECORDSIZE, 1, p_data->file);      // value is float *

// SMO_getPropertyValues(), case SMO_node
                fread(*outValueArray, RECORDSIZE, p_data->NodeProperties, p_data->file);
```

The integer 1 read as an IEEE single is the smallest denormal, 1.4e-45.

A related gap, not changed here: `SMO_getPropertyCode()` returns the engine's private `InputDataType` codes, which no public header defines. The same commit renumbered them (node maximum depth 3 to 4; link offsets 4, 4 to 6, 7; link full depth 3 to 4; length 5 to 8), so a file written by 5.2.4 and one written by 5.3.0 report different codes for the same properties. Only code 0, the type code, is the same in both.

## How to reproduce

| File | What it is |
|---|---|
| [`API-17_types.inp`](API-17_types.inp) | Dynamic wave model with nodes J1, J2 (junctions), O1 (outfall), SU1 (storage), D1 (divider) and links C1-C3 (conduits), P1 (pump), OR1 (orifice), W1 (weir), OL1 (outlet) |
| [`API-17_test.c`](API-17_test.c) | Runs the deck and reads property 0 of every node and link with both getters (5.3.0; on 5.2.4 it only reports that the getters do not exist) |
| [`API-17_test6.c`](API-17_test6.c) | Reads every node's and link's type through 6.0.0's `swmm_node_get_type()` / `swmm_link_get_type()` |

```sh
tools/run-test.sh API-17            # 5.3.0: FAIL; 5.2.4 and 6.0.0: PASS
tools/run-test.sh API-17 --patched  # 5.3.0 with the fix: PASS
```

**Without the fix** (5.3.0):

```
Nodes: 3 properties, property 0 has code 0
  ID   expected   getPropValue  getPropValues   other properties
  J1          0              0              0   12 5
  J2          0              0              0   1 5
  O1          1     1.4013e-45     1.4013e-45   0 2
  D1          3     4.2039e-45     4.2039e-45   10 5
  SU1         2     2.8026e-45     2.8026e-45   4 8

Links: 5 properties, property 0 has code 0
  ID   expected   getPropValue  getPropValues   other properties
  C1          0              0              0   0 0 1.5 400
  C2          0              0              0   0 0 1.5 400
  C3          0              0              0   0 0 2 400
  P1          1     1.4013e-45     1.4013e-45   0 0 0 0
  OR1         2     2.8026e-45     2.8026e-45   0 0 0.5 0
  W1          3     4.2039e-45     4.2039e-45   5 5 1 0
  OL1         4    5.60519e-45    5.60519e-45   1 1 0 0

FAIL: property 0 does not give the node/link type code (an integer's bits read as a float)
API-17 5.3.0 base: FAIL
```

5.2.4 and 6.0.0:

```
PASS: 5.2.4's output library has no property getters (SMO_getPropertyValue was added in 5.3.0), so nothing is misread
API-17 5.2.4 base: PASS
```

```
  ID   expected     type
  O1          1        1
  SU1         2        2
  P1          1        1
  OL1         4        4
  ...
PASS: every node and link reports its integer type code
API-17 6.0.0 base: PASS
```

**With the fix**, both getters return the type codes as numbers and the other properties are unchanged:

```
  ID   expected   getPropValue  getPropValues   other properties
  O1          1              1              1   0 2
  D1          3              3              3   10 5
  SU1         2              2              2   4 8
  ...
  P1          1              1              1   0 0 0 0
  OR1         2              2              2   0 0 0.5 0
  W1          3              3              3   5 5 1 0
  OL1         4              4              4   1 1 0 0

PASS: property 0 of every node and link is its type code
API-17 5.3.0 patched: PASS
```

## The fix

A small helper reads one property and converts the type-code slot, which it recognises by its property code, from INT4:

```diff
+#define TYPE_CODE_PROPERTY 0
...
+static float readPropertyValue(FILE *file, int propertyCode)
+{
+    INT4 k = 0;
+    REAL4 x = 0.0f;
+
+    if (propertyCode == TYPE_CODE_PROPERTY)
+    {
+        fread(&k, RECORDSIZE, 1, file);
+        return (REAL4)k;
+    }
+    fread(&x, RECORDSIZE, 1, file);
+    return x;
+}
...
-                fread(value, RECORDSIZE, 1, p_data->file);
+                *value = readPropertyValue(p_data->file, p_data->NodePropertyIndexes[propertyIndex]);
...
-                fread(*outValueArray, RECORDSIZE, p_data->NodeProperties, p_data->file);
+                for (i = 0; i < p_data->NodeProperties; i++)
+                    (*outValueArray)[i] = readPropertyValue(p_data->file, p_data->NodePropertyIndexes[i]);
```

The link cases are the same; subcatchments have no type code and are untouched. Junctions and conduits still read 0, so the existing Python tests (`python/tests/legacy/test_output.py`, which check property 0 of a junction and a conduit) are unaffected. The engine and its output file do not change. 6.0.0 needs no change.
