# Verification

## PRs 191-193 follow-up (28 October 2026)

Validated the submitted production changes against common PR base `fbd1d7b03c036d7a80e9bb64c9781dc5ccf464c2`, using isolated source copies on macOS arm64 / Apple Clang 21.0.0, `-O1 -g`, AddressSanitizer and UndefinedBehaviorSanitizer. The submitted production file blob hashes match GitHub's PR metadata. The active engine checkout and its unrelated changes were not used. These focused builds use a minimal CMake harness for all legacy C sources, OpenMP, and the exact PR Google Test sources; this is not a full application or CI build.

| Finding | PR / issue | Reviewed head | Original test on base | Original test on PR | PR Google Tests | Status |
|---|---|---|---|---|---|---|
| [CRASH-02](5-crashes/CRASH-02-xsection-before-link-heap-overflow/#fix-validation-28-october-2026) | [#191](https://github.com/HydroCouple/openswmm.engine/pull/191) / [#178](https://github.com/HydroCouple/openswmm.engine/issues/178) | `ac5d4306acc9b4d94faca3b6daf9b79ddce9f9c1` | ASan heap overflow | PASS; ERROR 209 / weir 5.0000 cfs | 2/2 pass | Fixed in 5.3.0; pending merge; see IO-11 interaction |
| [IO-10](4-io/IO-10-barrels-stored-as-char/#fix-validation-28-october-2026) | [#192](https://github.com/HydroCouple/openswmm.engine/pull/192) / [#177](https://github.com/HydroCouple/openswmm.engine/issues/177) | `4cc91c9d50132bda5085b15171cc55c7c12e600a` | FAIL; 300→44, 200 rejected | PASS; 300/200, depths 0.47/0.59 ft | 1/1 pass | Fixed in 5.3.0; pending merge |
| [IO-11](4-io/IO-11-irregular-street-barrels-ignored/#pr-review-validation-28-october-2026) | [#193](https://github.com/HydroCouple/openswmm.engine/pull/193) / [#176](https://github.com/HydroCouple/openswmm.engine/issues/176) | `eaa292d8b3c2cd94f70fa46c7d83221d47a247de` | FAIL; 2 barrels read as 1 | PASS for IRREGULAR; depths both 0.696 ft | 1/1 pass | Partial; changes required |

Each PR's added test program also reproduces its defect on the uncorrected base. Combining the three PRs passes all four added Google Tests and all three original C reproducers. Five existing legacy programs (`test_solver_api`, `test_solver_errors`, `test_solver_shapes`, `test_solver_storage_shapes`, `test_solver_transect_nc`) pass **81 Google Tests per variant** on the base, PR #191, PR #192 and combined changes. These broader runs use the existing `tools/ubsan.supp` for the unrelated CRASH-01 datetime conversion. An initial restricted run could not create the existing tests' `/var/tmp` inputs; rerunning with temporary-file access resolves those fixture failures. The focused tests use no UBSan suppression. The 6.0.0 engine was not rebuilt: all three findings already identify it as unaffected and these PRs change only legacy production code.

**Additional checks prevent marking IO-11 completed:**

- The [new STREET check](4-io/IO-11-irregular-street-barrels-ignored/IO-11_followup_test.c) fails on PR #193 and the combined changes: entered C1=2/C2=1, reported C1=1/C2=1. The STREET branch is unchanged despite the PR and issue scope.
- The [new rejected-link check](4-io/IO-11-irregular-street-barrels-ignored/IO-11_orifice-bad-node.inp) safely reports ERROR 209 on PR #191 alone but aborts with an ASan **heap-buffer-overflow, WRITE of size 4** when the three PRs are combined. IO-11's new IRREGULAR assignment is missing `Link[j].ID &&`; adding this guard in a temporary experiment restores safe ERROR 209 rejection. The experiment was reverted, and the documented submitted combined source was restored.

CRASH-02 and IO-10 are confirmed corrections, pending merge. IO-11 must preserve the CRASH-02 guard and resolve or narrow its claimed STREET scope before approval. No GitHub approval, merge, comment or issue closure was made during this review. The original generated verification matrix below remains historical and has not been rewritten as a result of these later PRs.

## PRs 190, 189, 188, 187 and 186 (23 November 2026)

Validated PR #190 (CRASH-07) merged onto `swmm6_rel` at 7ffd8dad, and PR #189 (IO-29) merged onto a2b5fb9e, on macOS arm64 / Apple Clang. For IO-29 the review deck reproduced on both unfixed engines (the 11-item conduit reported area 300 ft² and width 80 ft against 150 / 40 for the 10-item one) and the PR's two Google Tests pass on the fix and fail on the unfixed sources. On arm64 the
undefined float-to-int conversion saturates, so none of the three review decks crashes on the unfixed engines
there; the defect was instead shown with UndefinedBehaviorSanitizer (`-fsanitize=undefined,float-cast-overflow`)
on the PR's two Google Test programs, built once against the unfixed sources and once against the fix.

| Finding | PR / issue | Reviewed head | Original test on base | Original test on PR | PR Google Tests | Status |
|---|---|---|---|---|---|---|
| [CRASH-07](5-crashes/CRASH-07-xsection-lookup-nan-index/) | [#190](https://github.com/HydroCouple/openswmm.engine/pull/190) / [#174](https://github.com/HydroCouple/openswmm.engine/issues/174) | `25801e89` | no crash on arm64 (saturating conversion); UBSan: nan→int at XSectKernels.hpp:2779, 5e27→int at xsect.c:1560 | PASS (UBSan clean) | PASS, both engines | Merged (a2b5fb9e) |
| [IO-29](4-io/IO-29-transect-x1-token-layout/) | [#189](https://github.com/HydroCouple/openswmm.engine/pull/189) / [#173](https://github.com/HydroCouple/openswmm.engine/issues/173) | `c1e240a5` | FAIL (area 300, width 80 on the 11-item line, both engines) | PASS | PASS, both engines; FAIL on unfixed sources | Merged (bca45399) |
| [IO-13](4-io/IO-13-xsection-error-empty-token/) | [#188](https://github.com/HydroCouple/openswmm.engine/pull/188) / [#175](https://github.com/HydroCouple/openswmm.engine/issues/175) | `1706aef9` | legacy: ERROR 211 with no token (6.0.0 on `swmm6_rel` already named the link) | PASS | PASS, both engines; legacy test FAILs on the unfixed reader | Merged (b968e660) |
| [IO-12](4-io/IO-12-ellipse-arch-size-code-overrides-dimensions/) | [#187](https://github.com/HydroCouple/openswmm.engine/pull/187) / [#172](https://github.com/HydroCouple/openswmm.engine/issues/172) | `ded58af0` | FAIL (Geom1 read as the code with a Geom3 code present; 12.5 truncated to 12; both engines) | PASS | PASS, both engines; FAIL on unfixed sources | Merged (9c6941cf) |
| [CON-18](2-conceptual/CON-18-slot-area-not-monotonic/) | [#186](https://github.com/HydroCouple/openswmm.engine/pull/186) / [#171](https://github.com/HydroCouple/openswmm.engine/issues/171) | `00754027` | FAIL (closed pipe lost ~435 of 740 ft³, 5.8 % continuity error, both engines) | PASS | PASS, both engines; FAIL on unfixed sources | Merged (d4046ce9) |

## NUM-60 added (19 November 2026)

[NUM-60](1-numerical/NUM-60-bottom-orifice-submergence-counted-twice/) was found while investigating why OpenSWMM 6.0.0's semi-implicit dynamic-wave iteration does not converge on a large sewer model, and was added to the review after the matrix below was generated. Verified on macOS arm64 / Apple Clang 21.0.0 with the review's own scripts (`tools/run-test.sh`, AddressSanitizer and UndefinedBehaviorSanitizer, the pinned revisions):

| | 5.2.4 | 5.3.0 | 6.0.0 |
|---|---|---|---|
| Unpatched | FAIL | FAIL | FAIL |
| With `NUM-60_swmm530.patch` / `NUM-60_swmm600.patch` | — | PASS | PASS |

Unpatched, all three engines settle S1 at 1.3203 ft instead of 1.2430 ft, and S2's orifice flow keeps switching between 1.799 and 2.567 cfs instead of passing 2.2 cfs. Patched, 5.3.0 and 6.0.0 print identical numbers (S1 1.2430 ft, S2 1.3542 ft, both steady). All 327 patches, NUM-60's included, still apply together on cb3e192b in the order `tools/patch-order.sh` gives (checked with `git apply`); the all-patches build and its unit tests were not rerun for this addition.

In `swmm6_rel` the owner kept the legacy law in the 5.3.0 solver and under 6.0.0's default `NODE_CONTINUITY EXPLICIT`, and the fix applies under `NODE_CONTINUITY SEMI_IMPLICIT` only (see the write-up).

## NUM-03 follow-up: fixed in 5.3.0 and 6.0.0

Validated on **19 October 2026**, macOS arm64 / Apple Clang 21.0.0 / Debug, against `swmm6_rel` base `d4c3b6f4` plus fix `5efbe190`. [Issue #164](https://github.com/HydroCouple/openswmm.engine/issues/164), [engine PR](https://github.com/HydroCouple/openswmm.engine/pull/165) (**pending merge**).

All eight added unit tests pass, and the original NUM-03 dynamic-wave tests pass for both engines, giving the expected 1.001 ft normal depth and 2.535 ft/s velocity. The selected cross-section and simulation suites run 139 tests: 137 pass. Two circular-section tests fail identically before and after the correction (`XSectBitExact.GoldenHashCircularKernels` and `XSectFastMode.LookupFastWithinTolerance`); all three simulation regressions pass. See the [finding's fix-validation record](1-numerical/NUM-03-rect-round-hydraulic-radius/#fix-validation-19-october-2026) for details.

The generated matrix below remains the historical record for the pinned review revisions; its original NUM-03 failures do not describe the corrected PR branch.

## Original pinned-revision verification

Generated by `tools/run-all.sh` on 2026-10-06 (Linux x86_64, Ubuntu clang version 18.1.3 (1ubuntu1)).

Each test was compiled and run against engines built with AddressSanitizer and UndefinedBehaviorSanitizer:

- **5.2.4**: EPA SWMM tag v5.2.4, unpatched. 5.2.4 is reviewed but not patched.
- **5.3.0** and **6.0.0**: `src/legacy/engine` and `src/engine` at cb3e192b, unpatched, then with the issue's own patch (*patched*), then with every patch in this review applied together (*all*).

Expected: an affected engine shows the bug unpatched (`FAIL`, `CRASH` or `HANG`), an unaffected one `PASS`es, and every patched run `PASS`es. `n/a` means there is no test for that engine (e.g. a 5.3.0-only API). Issues fixed only in documentation are marked *docs fix*.

| ID | 5.2.4 | 5.3.0 | 5.3.0 patched | 6.0.0 | 6.0.0 patched | 5.3.0 all | 6.0.0 all | |
|---|---|---|---|---|---|---|---|---|
| [NUM-01](1-numerical/NUM-01-dry-conduit-quality-nan/) | PASS | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [NUM-02](1-numerical/NUM-02-inlet-backflow-all-inlets-one-bucket/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-03](1-numerical/NUM-03-rect-round-hydraulic-radius/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-04](1-numerical/NUM-04-si-snowmelt-coefficient-converted-wrong-way/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-05](1-numerical/NUM-05-weir-divider-scaled-by-flow-units/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-06](1-numerical/NUM-06-mitered-culvert-slope-correction/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-07](1-numerical/NUM-07-force-main-full-flow-overstated/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-08](1-numerical/NUM-08-first-flood-step-double-counted/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-09](1-numerical/NUM-09-outlet-only-junction-depth-frozen/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-10](1-numerical/NUM-10-ideal-pump-dummy-link-order/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-11](1-numerical/NUM-11-pump-drain-limit-ratchet/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-12](1-numerical/NUM-12-storage-curve-volume-drops-above-first-point/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-13](1-numerical/NUM-13-si-exfiltration-bottom-area/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-14](1-numerical/NUM-14-storage-evaporation-rate-as-flow/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-15](1-numerical/NUM-15-functional-storage-exponent-minus-one/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-16](1-numerical/NUM-16-vnotch-weir-partial-setting/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-17](1-numerical/NUM-17-kw-storage-outflow-multi-barrel/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-18](1-numerical/NUM-18-kw-capped-conduit-losses-not-removed/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-19](1-numerical/NUM-19-outfall-two-way-flow-not-booked/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-20](1-numerical/NUM-20-arch-code-31-full-radius/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-22](1-numerical/NUM-22-transect-nc-inherits-meander-adjusted-n/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [NUM-23](1-numerical/NUM-23-transect-bank-stations-float-compare/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-24](1-numerical/NUM-24-transect-right-wall-perimeter/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-25](1-numerical/NUM-25-curb-inlet-capture-nan-in-gutter/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-26](1-numerical/NUM-26-on-sag-inlet-stale-street-sides/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-28](1-numerical/NUM-28-timeseries-backward-lookup/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [NUM-29](1-numerical/NUM-29-zero-roughness-subarea-depression-storage/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-30](1-numerical/NUM-30-pervious-to-impervious-routing-lost/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-31](1-numerical/NUM-31-outfall-runon-step-ratio/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-32](1-numerical/NUM-32-evaporation-and-infiltration-same-water/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-33](1-numerical/NUM-33-horton-max-volume-ignored/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-34](1-numerical/NUM-34-gwf-variable-k-stale/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-35](1-numerical/NUM-35-gw-upper-et-fraction-above-one/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-36](1-numerical/NUM-36-gw-outflow-endpoint-trapezoid/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-37](1-numerical/NUM-37-rooftop-disconnection-zero-roughness/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-38](1-numerical/NUM-38-permeable-pavement-drain-unbounded/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-39](1-numerical/NUM-39-bioretention-no-storage-drain/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-40](1-numerical/NUM-40-rain-adjustment-read-ahead/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [NUM-41](1-numerical/NUM-41-rdii-rainfall-point-sampled/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-42](1-numerical/NUM-42-rdii-cogage-loses-scale-factor/) | PASS | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [NUM-43](1-numerical/NUM-43-hargreaves-evaporation-nan-polar/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [NUM-44](1-numerical/NUM-44-day-of-year-leap-years/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-45](1-numerical/NUM-45-dry-node-reports-mass-rate-as-concentration/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-46](1-numerical/NUM-46-quality-final-storage-double-counted/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [NUM-47](1-numerical/NUM-47-quality-ledger-half-step-integration/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [NUM-48](1-numerical/NUM-48-first-order-decay-explicit-euler/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-49](1-numerical/NUM-49-treatment-latch-leaves-minus-one-removal/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [NUM-50](1-numerical/NUM-50-steady-link-api-flux-mass-lost/) | PASS | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [NUM-51](1-numerical/NUM-51-mathexpr-power-of-nonpositive-base/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-52](1-numerical/NUM-52-pid-steady-error-reset/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-53](1-numerical/NUM-53-rule-step-uses-routing-step/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [NUM-54](1-numerical/NUM-54-slot-velocity-uses-slot-area/) | FAIL | PASS | PASS | PASS | PASS | PASS | PASS | 5.2.4 only, no patch |
| [NUM-55](1-numerical/NUM-55-storage-depth-newton-wrong-index/) | FAIL | PASS | PASS | PASS | PASS | PASS | PASS | 5.2.4 only, no patch |
| [NUM-56](1-numerical/NUM-56-modified-horton-max-volume-524/) | FAIL | PASS | PASS | PASS | PASS | PASS | PASS | 5.2.4 only, no patch |
| [NUM-57](1-numerical/NUM-57-transect-radius-1-49-524/) | FAIL | PASS | PASS | PASS | PASS | PASS | PASS | 5.2.4 only, no patch |
| [NUM-58](1-numerical/NUM-58-washoff-cutoff-drops-load-524/) | FAIL | PASS | PASS | PASS | PASS | PASS | PASS | 5.2.4 only, no patch |
| [NUM-59](1-numerical/NUM-59-node-only-model-not-routed/) | FAIL | PASS | PASS | PASS | PASS | PASS | PASS | 5.2.4 only, no patch |
| [CON-01](2-conceptual/CON-01-surcharged-storage-volume-pinned/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-02](2-conceptual/CON-02-min-surface-area-overrides-storage-curve/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-03](2-conceptual/CON-03-lid-runon-to-every-partial-unit/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-04](2-conceptual/CON-04-covered-barrel-returns-rain-over-all-lids/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-05](2-conceptual/CON-05-covered-barrel-rain-lost/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-06](2-conceptual/CON-06-snow-plowing-ignores-areas/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-07](2-conceptual/CON-07-groundwater-et-reduced-by-impervious-evaporation/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-08](2-conceptual/CON-08-mass-load-at-dry-node-discarded/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-09](2-conceptual/CON-09-ponded-evaporation-destroys-pollutant/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-10](2-conceptual/CON-10-withdrawal-removes-no-pollutant/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-11](2-conceptual/CON-11-treatment-reference-uses-wrong-pollutant-type/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-12](2-conceptual/CON-12-cyclic-treatment-error-unreachable/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-14](2-conceptual/CON-14-lid-drain-creates-washoff-mass/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-15](2-conceptual/CON-15-kw-empty-conduit-inlet-area-jump/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-16](2-conceptual/CON-16-subarea-runoff-end-of-step-rate/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-17](2-conceptual/CON-17-custom-ellipse-ignores-major-axis/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-18](2-conceptual/CON-18-slot-area-not-monotonic/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-19](2-conceptual/CON-19-swale-vegetation-fraction-inconsistent/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-20](2-conceptual/CON-20-swale-infiltration-diluted-by-imperviousness/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-21](2-conceptual/CON-21-statistics-averaged-over-steps/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-22](2-conceptual/CON-22-pump-status-on-needs-setting-one/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-23](2-conceptual/CON-23-control-only-gage-never-updated/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CON-24](2-conceptual/CON-24-snow-cold-content-cap/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-01](3-boundary/BND-01-free-outfall-below-regulator-gets-crest-depth/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-02](3-boundary/BND-02-tidal-outfall-elapsed-hours/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-03](3-boundary/BND-03-outfall-backflow-concentration-ignored/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-04](3-boundary/BND-04-outfall-backflow-mass-unbooked/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-05](3-boundary/BND-05-initial-depth-uses-upstream-offset-twice/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-07](3-boundary/BND-07-culvert-inlet-control-adverse-slope/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-08](3-boundary/BND-08-ponding-ignores-surcharge-depth/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-09](3-boundary/BND-09-duration-floor-drops-last-period/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-10](3-boundary/BND-10-events-skip-rule-step-error-107/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-11](3-boundary/BND-11-hotstart-groundwater-flow-lost/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-12](3-boundary/BND-12-timeopen-from-midnight/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-13](3-boundary/BND-13-custom-inlet-rating-ignores-max-flow/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-14](3-boundary/BND-14-runon-to-zero-area-subcatchment/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-15](3-boundary/BND-15-negative-evaporation-adjustment/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-16](3-boundary/BND-16-evaporation-series-start-between-entries/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-17](3-boundary/BND-17-polar-temperature-interpolation/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-18](3-boundary/BND-18-averages-not-reset-at-report-start/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-19](3-boundary/BND-19-report-start-mixes-whole-run-volumes/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [BND-20](3-boundary/BND-20-summary-nan-without-routing-step/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-01](4-io/IO-01-nan-inf-accepted-as-numbers/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-02](4-io/IO-02-time-options-accept-nan-inf-huge/) | CRASH | CRASH | PASS | CRASH | PASS | PASS | PASS | ✓ |
| [IO-03](4-io/IO-03-negative-nan-rainfall-accepted/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-04](4-io/IO-04-step-options-three-unit-conventions/) | PASS | PASS | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-05](4-io/IO-05-long-lines-split-silently/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-06](4-io/IO-06-tokens-past-40-dropped/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-07](4-io/IO-07-semicolon-inside-quotes/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-08](4-io/IO-08-utf8-bom-hides-first-section/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-09](4-io/IO-09-outfall-gate-and-routeto/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-10](4-io/IO-10-barrels-stored-as-char/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-11](4-io/IO-11-irregular-street-barrels-ignored/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-12](4-io/IO-12-ellipse-arch-size-code-overrides-dimensions/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-13](4-io/IO-13-xsection-error-empty-token/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-14](4-io/IO-14-culvert-code-out-of-range/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-15](4-io/IO-15-groundwater-needs-eleven-tokens/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-16](4-io/IO-16-link-properties-reset-by-section-order/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-17](4-io/IO-17-subcatchment-section-order/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-18](4-io/IO-18-pct-zero-over-100/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-19](4-io/IO-19-coverage-percent-unvalidated/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-20](4-io/IO-20-subareas-bad-number-error-209/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-21](4-io/IO-21-pattern-factor-count-unchecked/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-22](4-io/IO-22-rdii-negative-recession-accepted/) | CRASH | CRASH | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-23](4-io/IO-23-unit-hydrograph-without-gage/) | CRASH | CRASH | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-24](4-io/IO-24-rain-barrel-covered-default/) | PASS | PASS | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-25](4-io/IO-25-gutter-depression-units/) | PASS | PASS | PASS | PASS | PASS | PASS | PASS | docs fix |
| [IO-26](4-io/IO-26-climate-file-wind-not-converted/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-27](4-io/IO-27-ghcn-default-temperature-units/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-28](4-io/IO-28-transect-not-finalized/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-29](4-io/IO-29-transect-x1-token-layout/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-30](4-io/IO-30-rule-variable-prefix-match/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-31](4-io/IO-31-nodestats-matched-as-nodes/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-32](4-io/IO-32-grate-type-prefix-match/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-33](4-io/IO-33-keyword-index-in-plain-char/) | PASS | PASS | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-34](4-io/IO-34-rule-settings-unbounded/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-35](4-io/IO-35-report-start-date-without-time/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-36](4-io/IO-36-rdii-file-relative-path/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-37](4-io/IO-37-use-files-opened-read-write/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-38](4-io/IO-38-timeseries-file-without-final-newline/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-39](4-io/IO-39-input-warnings-not-counted/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-40](4-io/IO-40-expression-paren-minus-number/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-41](4-io/IO-41-expression-malformed-exponent/) | FAIL | FAIL | PASS | CRASH | PASS | PASS | PASS | ✓ |
| [IO-42](4-io/IO-42-hotstart-short-read/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-43](4-io/IO-43-dated-hotstart-save-time/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-44](4-io/IO-44-hotstart-modified-horton-state/) | PASS | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-45](4-io/IO-45-runoff-file-evaporation-24x/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-46](4-io/IO-46-runoff-file-groundwater-flow/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-47](4-io/IO-47-runoff-file-groundwater-elevation-sign/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-48](4-io/IO-48-outflows-file-not-interpolated/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-49](4-io/IO-49-output-header-report-start-date/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-50](4-io/IO-50-continuity-error-hides-nan/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-51](4-io/IO-51-continuity-forcing-ignores-negative-errors/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-52](4-io/IO-52-runtime-errors-not-in-report/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-53](4-io/IO-53-instability-index-zero-hidden/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-54](4-io/IO-54-pump-min-flow-always-zero/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-55](4-io/IO-55-pump-off-curve-low-counted-high/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-56](4-io/IO-56-normal-flow-limited-when-dry/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-57](4-io/IO-57-system-runoff-double-counts-runon/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-58](4-io/IO-58-node-inflow-volume-includes-initial-storage/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-59](4-io/IO-59-report-flow-columns-merge/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-60](4-io/IO-60-lid-idle-unit-100-percent-error/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [IO-61](4-io/IO-61-report-gallons-factor-7-48/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [IO-62](4-io/IO-62-si-street-flow-header/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-01](5-crashes/CRASH-01-datetime-timediff-overflow/) | CRASH | CRASH | PASS | CRASH | PASS | PASS | PASS | ✓ |
| [CRASH-02](5-crashes/CRASH-02-xsection-before-link-heap-overflow/) | CRASH | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-03](5-crashes/CRASH-03-curve-without-points/) | CRASH | CRASH | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CRASH-04](5-crashes/CRASH-04-curve-id-only-line-count-pass/) | CRASH | CRASH | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [CRASH-05](5-crashes/CRASH-05-expression-buffer-overflow/) | CRASH | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-06](5-crashes/CRASH-06-sweep-date-buffer-overflow/) | CRASH | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-07](5-crashes/CRASH-07-xsection-lookup-nan-index/) | CRASH | CRASH | PASS | CRASH | PASS | PASS | PASS | ✓ |
| [CRASH-08](5-crashes/CRASH-08-rule-expression-freed-after-input-error/) | CRASH | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-09](5-crashes/CRASH-09-snow-removal-without-subcatchment/) | CRASH | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-10](5-crashes/CRASH-10-rdii-file-node-index-unchecked/) | CRASH | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-11](5-crashes/CRASH-11-averages-report-out-of-bounds/) | PASS | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-12](5-crashes/CRASH-12-steady-link-quality-node-index/) | PASS | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-13](5-crashes/CRASH-13-pollutant-flux-before-start/) | PASS | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-14](5-crashes/CRASH-14-getcount-getindex-object-type/) | PASS | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-15](5-crashes/CRASH-15-landuse-setter-timeseries-index/) | PASS | CRASH | PASS | CRASH | PASS | PASS | PASS | ✓ |
| [CRASH-16](5-crashes/CRASH-16-getsavedvalue-index-unchecked/) | CRASH | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-17](5-crashes/CRASH-17-getname-geterror-buffer-overrun/) | CRASH | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-18](5-crashes/CRASH-18-output-reader-system-result-bad-period/) | CRASH | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-19](5-crashes/CRASH-19-output-reader-null-handle/) | CRASH | CRASH | PASS | PASS | PASS | PASS | PASS | ✓ |
| [CRASH-20](5-crashes/CRASH-20-error-string-overflow-524/) | CRASH | PASS | PASS | PASS | PASS | PASS | PASS | 5.2.4 only, no patch |
| [CRASH-21](5-crashes/CRASH-21-single-point-rain-series-524/) | CRASH | PASS | PASS | PASS | PASS | PASS | PASS | 5.2.4 only, no patch |
| [API-01](6-api/API-01-settings-before-start-reset/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [API-02](6-api/API-02-call-order-error-sticky/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [API-03](6-api/API-03-api-error-messages-empty/) | PASS | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [API-04](6-api/API-04-statistics-getters-raw-accumulators/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [API-05](6-api/API-05-quality-getters-wrong-units/) | PASS | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [API-06](6-api/API-06-external-buildup-per-step/) | PASS | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [API-07](6-api/API-07-buildup-getter-sums-land-uses/) | PASS | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [API-08](6-api/API-08-mass-flux-litres-conversion/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [API-09](6-api/API-09-subcatchment-geometry-setters-skip-alpha/) | PASS | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [API-10](6-api/API-10-invert-offset-setters-stale-slope/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [API-11](6-api/API-11-min-slope-percent-vs-fraction/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [API-12](6-api/API-12-max-route-step-returns-min/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [API-13](6-api/API-13-api-rainfall-not-wet-step/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [API-14](6-api/API-14-gage-rainfall-setter-cogage/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [API-15](6-api/API-15-gage-scale-factor-runtime-change/) | PASS | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |
| [API-16](6-api/API-16-output-reader-range-checks/) | FAIL | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [API-17](6-api/API-17-output-reader-type-code-as-float/) | PASS | FAIL | PASS | PASS | PASS | PASS | PASS | ✓ |
| [API-18](6-api/API-18-cli-exit-code-zero-on-error/) | FAIL | FAIL | PASS | FAIL | PASS | PASS | PASS | ✓ |

## The project's own unit tests

The 6.0.0 tree's ctest suite (unit tests for both the legacy 5.3.0 library and the 6.0.0 engine) was built with the same sanitizers, unpatched and with all patches.

- **base**: 93% tests passed, 13 tests failed out of 192
- **all**: 94% tests passed, 12 tests failed out of 192

Test programs failing with all patches but not unpatched:

- none

Test programs failing unpatched but passing with all patches:

- test_engine_msx_buildup_washoff (Failed)

Test programs failing in both (pre-existing):

- d1_no_hydrocouplesdk (Failed)
- gw2d_gates (Not Run)
- test_engine_2d_surface (Not Run)
- test_engine_capi_file_paths (Failed)
- test_engine_hotstart (Failed)
- test_engine_lard_heat (Failed)
- test_engine_lid_nodes (Failed)
- test_engine_msx_parity (Failed)
- test_engine_virtual_junction (Failed)
- test_engine_water_age (Failed)
- test_engine_xsect_kernels_parity (Failed)
- test_flowtrace (Failed)

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
