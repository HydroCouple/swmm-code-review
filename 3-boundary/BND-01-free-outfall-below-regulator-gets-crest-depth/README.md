# BND-01: A FREE or NORMAL outfall below a weir or orifice is given the crest height as its depth

| | |
|---|---|
| **Category** | [Boundary conditions](../README.md) |
| **Impact** | The outfall gets a fake tailwater equal to the regulator's crest height above its upstream node. In the test, a FREE outfall below a weir sits 3 ft above the crest: 346 cfs flows back out of the "free" outfall at the start, and the storage upstream ends 2.58 ft too high. Nothing warns the user. |
| **Reached from** | Any weir, orifice or outlet whose downstream node is a FREE or NORMAL outfall, with a crest offset other than 0 (DYNWAVE) |
| **5.3.0** | `outfall_setOutletDepth()` in [`src/legacy/engine/node.c:1419`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L1419) and [`:1423`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/node.c#L1423), fed by `link_setOutfallDepth()` in [`src/legacy/engine/link.c:749`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L749) |
| **5.2.4** | Not affected: a nonzero offset gives the outfall depth 0 ([`src/solver/node.c:1433`](https://github.com/USEPA/Stormwater-Management-Model/blob/v5.2.4/src/solver/node.c#L1433)) |
| **6.0.0** | Reproduces: `legacyOffset()` returns the regulator's crest for a downstream outfall ([`src/engine/hydraulics/Outfall.cpp:134`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Outfall.cpp#L134)), and `setAllOutfallDepths()` adds it ([`:286`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Outfall.cpp#L286), [`:291`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/engine/hydraulics/Outfall.cpp#L291)) |
| **Since** | 5.3.0, fork commit [4606020d](https://github.com/HydroCouple/Stormwater-Management-Model/commit/4606020d64856990fb05f1762f9df97710bf7eca) ("Address zero depth outfall bug #154", January 2024; not in EPA's `develop`) |
| **Fix** | Use no offset for a regulator's downstream outfall: [`BND-01_swmm530.patch`](BND-01_swmm530.patch), [`BND-01_swmm600.patch`](BND-01_swmm600.patch) |

## The problem

A FREE outfall takes the smaller of the critical and normal depths of the link that feeds it, and a NORMAL outfall takes the normal depth. A weir, orifice or outlet has neither (the engine uses 0), so the outfall should have no depth and the regulator should discharge freely.

In 5.3.0 and 6.0.0 the outfall instead gets a depth equal to the regulator's crest offset, which is a height above the regulator's **upstream** node, applied at the outfall's own invert. When the outfall invert is higher than the upstream invert, this tailwater can stand above the crest:

- **Weir to a FREE outfall** (`BND-01_weir-free.inp`): storage invert 100 ft, weir crest 5 ft (elevation 105), outfall invert 103 ft, steady 20 cfs inflow. The outfall gets a depth of 5 ft, so its HGL is 108 ft, 3 ft above the crest. At the start, 346 cfs runs back from the "free" outfall into the storage unit, the weir flow then swings up to 70 cfs although the inflow is a steady 20 cfs, and the storage ends at 108.03 ft. The weir equation gives 105.45 ft, which 5.2.4 reproduces.
- **Side orifice to a NORMAL outfall** (`BND-01_orifice-normal.inp`): storage invert 100 ft, 1 x 2 ft orifice 3 ft up (bottom 103, crown 104), outfall invert 102 ft, steady 5 cfs. The outfall gets 3 ft (HGL 105, above the orifice crown), 12.8 cfs flows back at the start, and the storage ends at 105.23 ft instead of below the crown.

When the outfall invert is lower, flows are unaffected but the reported outfall depth and HGL are still wrong.

## Why it happens

`link_setOutfallDepth()` passes the link's downstream offset to the outfall as `z`:

```c
// src/legacy/engine/link.c, link_setOutfallDepth()
if ( Node[Link[j].node2].type == OUTFALL )
{
    n = Link[j].node2;
    z = Link[j].offset2;
}
```

For a conduit, `offset2` is the height of the pipe invert above the outfall invert. For a regulator, `link_setParams()` stores the crest offset in both fields (`Link[j].offset2 = Link[j].offset1;`, [`link.c:366`](https://github.com/HydroCouple/Stormwater-Management-Model/blob/cb3e192b52674757a16afdfc6494382ef890f02d/src/legacy/engine/link.c#L366), 375 and 388), so `z` is the crest height above the upstream node.

5.2.4 only used `z` to test whether the link enters above the outfall invert, and then set the depth to 0. Commit 4606020d changed the FREE and NORMAL rules to add `z` to the depth, which is right for a conduit but was applied to every link type:

```c
// src/legacy/engine/node.c, outfall_setOutletDepth()
case FREE_OUTFALL:
    Node[j].newDepth = z + MIN(yNorm, yCrit);   // 5.2.4: if ( z > 0.0 ) newDepth = 0.0; else MIN(yNorm, yCrit)
    return;

case NORMAL_OUTFALL:
    Node[j].newDepth = z + yNorm;
    return;
```

`link_getYnorm()` and the caller give `yNorm = yCrit = 0` for any link that is not a conduit, so the outfall depth is exactly the crest offset. 6.0.0 ported the same rule and returns the regulator's un-raised crest (`offset2`) from `legacyOffset()` to match it.

## How to reproduce

| File | What it is |
|---|---|
| [`BND-01_weir-free.inp`](BND-01_weir-free.inp) | Storage (invert 100) -> transverse weir (crest 5 ft, 20 ft long) -> FREE outfall at 103; 20 cfs |
| [`BND-01_orifice-normal.inp`](BND-01_orifice-normal.inp) | Storage (invert 100) -> 1 x 2 ft side orifice (offset 3 ft) -> NORMAL outfall at 102; 5 cfs |
| [`BND-01_test.c`](BND-01_test.c) | Runs both decks through the legacy toolkit (5.2.4, 5.3.0) and checks the outfall depth, the sign of the regulator flow and the final storage stage |
| [`BND-01_test6.c`](BND-01_test6.c) | The same through the 6.0.0 C API |

The test expects an outfall depth of 0 (tolerance 0.001 ft), no reverse flow (minimum flow >= -0.001 cfs), a storage stage of 105.448 +/- 0.05 ft for the weir (Q = 3.33 x 20 x h^1.5 = 20 cfs gives h = 0.448 ft) and a storage stage below the orifice crown for the orifice (a free orifice running just full passes 0.65 x 2 x sqrt(2g x 0.5) = 7.4 cfs, more than the 5 cfs inflow).

```sh
tools/run-test.sh BND-01            # 5.2.4 PASS, 5.3.0 and 6.0.0 FAIL
tools/run-test.sh BND-01 --patched  # 5.3.0 and 6.0.0 with the fix: PASS
```

**Without the fix** (5.3.0; 6.0.0 prints the same except a final storage HGL of 108.023 ft for the weir):

```
Deck                         O1 depth   min flow   max flow    SU1 HGL   expected SU1 HGL
                             max (ft)      (cfs)      (cfs)   end (ft)
BND-01_weir-free.inp            5.000   -346.064     70.102    108.025   105.398-105.498
BND-01_orifice-normal.inp       3.000    -12.777      5.000    105.230   103.000-104.000
FAIL: outfall depth 5.000 ft (weir, FREE) and 3.000 ft (orifice, NORMAL) instead of 0; minimum flow -346.06 / -12.78 cfs; storage HGL 108.025 ft (expected 105.448) / 105.230 ft (expected < 104)
BND-01 5.3.0 base: FAIL
BND-01 6.0.0 base: FAIL
```

5.2.4 passes with the same numbers as the patched engines. The flow continuity error in the weir run's report is +1.245 % in 5.3.0 and 6.0.0, against -0.003 % in 5.2.4.

**With the fix**, 5.3.0 and 6.0.0 print the same values:

```
BND-01_weir-free.inp            0.000      0.000     20.000    105.448   105.398-105.498
BND-01_orifice-normal.inp       0.000      0.000      5.000    103.772   103.000-104.000
PASS: outfall depth stays 0, no reverse flow, storage stage matches free discharge
BND-01 5.3.0 patched: PASS
BND-01 6.0.0 patched: PASS
```

## The fix

Give a regulator's downstream outfall no offset, so the FREE and NORMAL rules give depth 0 as in 5.2.4. Conduits keep the 5.3.0 rule:

```diff
     if ( Node[Link[j].node2].type == OUTFALL )
     {
         n = Link[j].node2;
         z = Link[j].offset2;
+
+        // --- a regulator's offset2 is its crest height above its upstream
+        //     node, not a height above the outfall
+        if ( Link[j].type != CONDUIT ) z = 0.0;
     }
```

The 6.0.0 patch returns 0 from `legacyOffset()` for the same case. For FIXED, TIDAL and TIMESERIES outfalls below a regulator the result does not change (with `yCrit = 0` both values of `z` give `max(0, stage - invert)`), except that an orifice or weir authored with a negative offset can no longer give a FIXED outfall a negative depth.

**Effect on other models.** The regression decks that route a regulator into an outfall (`extran9`, `control_rules_test`, `Storage_Shape_Test`) all have zero offsets; their reports are unchanged in both engines. No regression deck has a regulator with a nonzero offset into a FREE or NORMAL outfall.
