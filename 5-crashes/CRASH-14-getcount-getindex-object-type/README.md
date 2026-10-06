# CRASH-14: 5.3.0 swmm_getCount / swmm_getIndex read past their arrays for object types 18 to 99

| | |
|---|---|
| **Category** | [Crashes, memory errors and undefined behaviour](../README.md) |
| **Impact** | Out-of-bounds read. `swmm_getCount(50)` returns whatever lies in memory after the 18-entry `Nobjects[]` array, as if it were a count; `swmm_getIndex(50, "J1")` passes a pointer read from past the 18-entry hash-table array to `HTfind()`, which dereferences it. A binding that passes a type code 5.3.0 does not know (a newer enum value, a typo, `swmm_SYSTEM - 1`) gets a garbage count or a crash, with no error. |
| **Reached from** | `swmm_getCount(objType)` and `swmm_getIndex(objType, id)` with `18 <= objType <= 99` on an open project |
| **5.3.0** | `swmm_getCount()` and `swmm_getIndex()` in [`src/legacy/engine/swmm5.c:1248`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L1248) and [`:1326`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/swmm5.c#L1326); `project_findObject()` in [`project.c:380`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L380) |
| **5.2.4** | Not affected: both functions accept only `swmm_GAGE..swmm_LINK` and return 0 / -1 otherwise ([`src/solver/swmm5.c:788`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L788), [`:835`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/swmm5.c#L835)) |
| **6.0.0** | Not applicable: counts and lookups are typed per object (`swmm_node_count`, `swmm_node_index`, ...), so there is no object-type code to index with; the one engine function that takes an element-type code, `swmm_forcing_clear()`, refuses unknown codes |
| **Since** | 5.3.0: fork commit 5b87a2b5 ("WIP API bindings for pollutants", 2024-12-10) widened `swmm_getCount`'s check from `> swmm_LINK` to `> swmm_SYSTEM`, and 6c08f7ee ("WIP API and bindings unit testing", 2024-12-11) changed it to `>= swmm_SYSTEM` |
| **Fix** | Check against `MAX_OBJ_TYPES`, as `swmm_getName()` does: [`CRASH-14_swmm530.patch`](CRASH-14_swmm530.patch) |

## The problem

5.3.0 extended `swmm_getCount()` and `swmm_getIndex()` from the four network object types to all eighteen object types in the public `swmm_Object` enum (`swmm_GAGE` = 0 to `swmm_INLET` = 17). The range check was written against the next enum value that happened to exist, `swmm_SYSTEM` = 100, instead of the end of the object list, so 82 type codes that have no array entry pass it. On the test deck, `swmm_getCount(18)` reads `Nobjects[18]`, the first element past the array, and `swmm_getIndex(50, "J1")` reads `Htable[50]`.

5.2.4 refused anything above `swmm_LINK`; `swmm_getName()` in 5.3.0 checks `objType >= MAX_OBJ_TYPES` correctly. Only these two functions use the wrong limit.

## Why it happens

```c
// src/legacy/engine/swmm5.c, swmm_getCount()
    if (objType < swmm_GAGE || objType >= swmm_SYSTEM)
        return ERR_API_OBJECT_TYPE;
    return Nobjects[objType];

// src/legacy/engine/swmm5.c, swmm_getIndex()
    if (objType < swmm_GAGE || objType >= swmm_SYSTEM)
        return ERR_API_OBJECT_TYPE;
    return project_findObject(objType, name);

// src/legacy/engine/project.c, project_findObject()
    return HTfind(Htable[type], id);
```

`Nobjects` is declared `int Nobjects[MAX_OBJ_TYPES]` ([`globals.h:72`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/globals.h#L72)) and `Htable` is `HTtable* Htable[MAX_OBJ_TYPES]` ([`project.c:85`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/project.c#L85)), with `MAX_OBJ_TYPES` = 18 (`enums.h`).

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-14_model.inp`](CRASH-14_model.inp) | 1 gage, 1 subcatchment, 2 nodes, 1 conduit |
| [`CRASH-14_test.c`](CRASH-14_test.c) | Checks `swmm_getCount(swmm_NODE)` and `swmm_getIndex(swmm_NODE, "O1")`, then calls both functions with types 18, 50 and 99; each must return no count (0 or a negative code) and a negative index |
| [`CRASH-14_test6.c`](CRASH-14_test6.c) | 6.0.0's typed counts and lookups, and `swmm_forcing_clear()` with type codes 18, 50, 99, -1 |

```sh
tools/run-test.sh CRASH-14            # 5.2.4: PASS, 5.3.0: CRASH, 6.0.0: PASS
tools/run-test.sh CRASH-14 --patched  # 5.3.0 and 6.0.0: PASS
```

**Without the fix**, 5.3.0 stops at the first unknown type:

```
swmm_getCount(swmm_NODE)        = 2 (deck has 2)
swmm_getIndex(swmm_NODE, "O1")  = 1 (expected 1)
swmm_getCount(18) ...
../src/src/legacy/engine/swmm5.c:1250:12: runtime error: index 18 out of bounds for type 'int[18]'
    #0 0x7fa2de3c26a2 in swmm_getCount .../src/legacy/engine/swmm5.c:1250:12
    #1 0x55da5b80c89b in main .../CRASH-14_test.c:43:13
CRASH-14 5.3.0 base: CRASH
```

`swmm_getIndex(50, "J1")` on its own stops in the same way one level down: `project.c:380:19: runtime error: index 50 out of bounds for type 'HTtable *[18]'`. 5.2.4 returns 0 and -1 for all three types and passes; 6.0.0's `swmm_forcing_clear` returns `SWMM_ERR_BADPARAM` (9) for each code and passes.

**With the fix**:

```
swmm_getCount(18)             = -999904
swmm_getIndex(18, "J1")       = -999904
swmm_getCount(50)             = -999904
swmm_getIndex(50, "J1")       = -999904
swmm_getCount(99)             = -999904
swmm_getIndex(99, "J1")       = -999904
PASS: unknown object types are refused and known types still work
CRASH-14 5.3.0 patched: PASS
```

## The fix

```diff
-    if (objType < swmm_GAGE || objType >= swmm_SYSTEM)
+    if (objType < swmm_GAGE || objType >= MAX_OBJ_TYPES)
         return ERR_API_OBJECT_TYPE;
```

in both functions. The public enum and the internal object-type enum list the eighteen types in the same order, so every valid type keeps working. The error code is the one 5.3.0 already returns for negative types, `ERR_API_OBJECT_TYPE` (-999904); being negative, it cannot be mistaken for a count or an index. No model result changes.
