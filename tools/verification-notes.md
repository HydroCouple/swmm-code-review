### Notes on the unit tests

**Tests updated by the patches.** Five 6.0.0 patches also change unit tests that pinned the behaviour they fix; without those changes the tests fail once the fix is applied:

| Patch | Tests changed | Why |
|---|---|---|
| [IO-04](4-io/IO-04-step-options-three-unit-conventions/) | `test_dw_unsteady_friction`, `test_dw_tpa`, `test_fv_tpa_closure`, `test_fv_unsteady_friction` | their decks wrote `REPORT_STEP` as bare seconds (0.5, 1, 2, 5), which EPA SWMM reads as hours; now written as clock times with the same meaning |
| [API-08](6-api/API-08-mass-flux-litres-conversion/) | `test_ard_transport` | the forcing rate was given in internal units (concentration × cfs); now in mass/s |
| [NUM-37](1-numerical/NUM-37-rooftop-disconnection-zero-roughness/) | `test_lid` (`LIDRoof.SplitsBetweenOverflowAndDrain`) | expected the roof drain to receive nothing when Rough or Slope is 0 |
| [NUM-45](1-numerical/NUM-45-dry-node-reports-mass-rate-as-concentration/) | `test_quality_routing` (`ResidualInflowBelowLegacyZeroIsNotDividedBy`) | expected a dry node to report the load-rate residue as its concentration |
| [NUM-48](1-numerical/NUM-48-first-order-decay-explicit-euler/) | `test_quality_routing`, `test_reaction_legacy_binding`, benchmark `quality-cstr-first-order-decay` | expected the linear decay factor 1 − k·dt; the benchmark is now the exact solution |

**One test passes with the patches by coincidence.** `test_engine_msx_buildup_washoff` fails unpatched because two load totals meant to be bit-identical differ in the last bit (0.62823738241251414 against 0.62823738241251426). With all patches applied the washoff arithmetic changes and the two happen to agree; no patch addresses it.

**The pre-existing failures** are not related to the review's patches:

- `test_engine_hotstart`, `test_engine_lard_heat`, `test_engine_water_age`, `test_engine_msx_parity`, `test_engine_lid_nodes`: UndefinedBehaviorSanitizer stops them at the misaligned CRC read in `HotStartManager::read_file` (`HotStartManager.cpp:336`), a 6.0.0 defect listed in [further-findings.md](further-findings.md).
- `test_engine_capi_file_paths`: the test itself passes 9999 as a `SWMM_FilePathRole`, which UndefinedBehaviorSanitizer reports as an invalid enum load.
- `test_engine_2d_surface`, `gw2d_gates` (not run) and `test_engine_virtual_junction` (`ValidationRuleCodes` expects a 2D validation error): need the 2D module, which these builds turn off.
- `d1_no_hydrocouplesdk`: checks the `python/` folder, which the scripts do not export.
- `test_flowtrace` (`swmm_trace_prepare` returns −6 in this build) and `test_engine_xsect_kernels_parity` (kernel against evaluator agreement for some shapes) were not investigated.
