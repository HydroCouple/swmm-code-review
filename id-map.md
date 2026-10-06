# Issue IDs and review-pass candidates

Every issue in this review has one ID, whose prefix is its category:

| Prefix | Category |
|---|---|
| `NUM` | [Numerical errors](1-numerical/README.md) |
| `CON` | [Conceptual and formulation issues](2-conceptual/README.md) |
| `BND` | [Boundary conditions](3-boundary/README.md) |
| `IO` | [Input, report and output files](4-io/README.md) |
| `CRASH` | [Crashes, memory errors and undefined behaviour](5-crashes/README.md) |
| `API` | [Toolkit API](6-api/README.md) |

The review pass produced about 250 candidate findings, each with a short key. Several reviewers found the same defect, and some candidates did not hold up when a test and a fix were written. This page maps every candidate key to the issue that covers it, or says why it was dropped.

- **Reviewer** names the part of the review pass that reported the candidate: R1 dynamic wave, R2 links, nodes and routing, R3 cross-sections, tables and inlets, R4 runoff, infiltration and groundwater, R5 snow, climate, gages, RDII and LID, R6 water quality, R7 input files, hot start and interface files, R8 report and output, R9 sanitizer sweep and fuzzing, R10 toolkit API and control rules; "project notes" are defects already recorded in the OpenSWMM planning notes.

| ID | Issue | Candidate keys | Reviewer |
|---|---|---|---|
| [NUM-01](1-numerical/NUM-01-dry-conduit-quality-nan/) | Every dry conduit gets a NaN pollutant concentration, including in EPA's Example1 | `api-qual-flux-zero-load-nan` | project notes |
| [NUM-02](1-numerical/NUM-02-inlet-backflow-all-inlets-one-bucket/) | A capture node's overflow is split across every inlet in the model | `inlet-backflow-ratio-single-bucket` | project notes |
| [NUM-03](1-numerical/NUM-03-rect-round-hydraulic-radius/) | RECT_ROUND hydraulic radius in the round bottom has a misplaced parenthesis | `rect-round-rofy-wrong-segment-formula` | R3 |
| [NUM-04](1-numerical/NUM-04-si-snowmelt-coefficient-converted-wrong-way/) | SI snow melt coefficients are multiplied by 1.8 instead of divided, so SI snow packs melt 3.24 times too fast | `snow-si-melt-coefficient-converted-wrong-way` | R5 |
| [NUM-05](1-numerical/NUM-05-weir-divider-scaled-by-flow-units/) | A WEIR divider's flow depends on the flow units chosen | `weir-divider-coeff-scaled-by-flow-units` | R2 |
| [NUM-06](1-numerical/NUM-06-mitered-culvert-slope-correction/) | Mitered culvert inlets get ten times the HDS-5 slope correction | `culvert-mitered-slope-correction-10x` | R1 |
| [NUM-07](1-numerical/NUM-07-force-main-full-flow-overstated/) | A force main's full flow uses the Hazen-Williams radius exponent with Manning's equation | `force-main-sfull-uses-hw-exponent-with-manning-beta` | R3 |
| [NUM-08](1-numerical/NUM-08-first-flood-step-double-counted/) | The step on which a storage unit starts to flood counts its fill volume twice | `dw-first-flood-step-overflow-double-count` | R1 |
| [NUM-09](1-numerical/NUM-09-outlet-only-junction-depth-frozen/) | A junction drained only by outlets never changes depth under dynamic wave routing | `dw-outlet-only-junction-depth-frozen` | R10 |
| [NUM-10](1-numerical/NUM-10-ideal-pump-dummy-link-order/) | Ideal pumps and DUMMY conduits miss the inflow from links listed after them | `dw-nonconduit-link-order-ideal-pump-dummy` | R1 |
| [NUM-11](1-numerical/NUM-11-pump-drain-limit-ratchet/) | A pump cannot draw down a junction wet well fed by a free-falling pipe | `dw-pump-limit-raw-surfarea-ratchet` | R1 |
| [NUM-12](1-numerical/NUM-12-storage-curve-volume-drops-above-first-point/) | A storage curve that starts above zero depth loses volume once the water passes its first point | `storage-curve-volume-drops-above-first-point` | R3 |
| [NUM-13](1-numerical/NUM-13-si-exfiltration-bottom-area/) | In SI units, storage seepage uses a bottom area 10.8 times too small for FUNCTIONAL and geometric shapes | `exfil-bottom-area-not-unit-converted` | project notes |
| [NUM-14](1-numerical/NUM-14-storage-evaporation-rate-as-flow/) | A nearly empty storage unit loses its evaporation rate in ft/s as if it were a flow in cfs | `storage-evap-ft-per-s-returned-as-cfs` | project notes |
| [NUM-15](1-numerical/NUM-15-functional-storage-exponent-minus-one/) | A FUNCTIONAL storage unit with exponent -1 or less is accepted and gets an infinite or meaningless volume | `storage-functional-exponent-minus-one-infinite-volume` | R9 |
| [NUM-16](1-numerical/NUM-16-vnotch-weir-partial-setting/) | Closing a V-notch weir by 0.1% removes almost all of its flow | `vnotch-partial-setting-drops-triangular-flow` | R2 |
| [NUM-17](1-numerical/NUM-17-kw-storage-outflow-multi-barrel/) | Under KINWAVE and STEADY, a storage unit drains into a multi-barrel conduit through one barrel only | `kw-storage-outflow-ignores-barrels` | R2 |
| [NUM-18](1-numerical/NUM-18-kw-capped-conduit-losses-not-removed/) | Under STEADY and KINWAVE, a full conduit's seepage and evaporation are booked but never taken from the water | `sf-kw-conduit-loss-booked-not-removed-when-capped` | R2 |
| [NUM-19](1-numerical/NUM-19-outfall-two-way-flow-not-booked/) | An outfall that both receives and sends flow books nothing to the mass balance | `outfall-two-way-flow-not-booked` | R2 |
| [NUM-20](1-numerical/NUM-20-arch-code-31-full-radius/) | Standard arch size code 31 has the full hydraulic radius of code 30 | `arch-code31-rfull-duplicated` | R3 |
| [NUM-22](1-numerical/NUM-22-transect-nc-inherits-meander-adjusted-n/) | An NC line of zeros inherits the previous transect's meander-adjusted n | `transect-inherited-nc-carries-meander-factor` | R3 |
| [NUM-23](1-numerical/NUM-23-transect-bank-stations-float-compare/) | In SI units a transect width factor can make SWMM miss the bank stations | `transect-bank-station-float-mismatch-si-wfactor` | R3 |
| [NUM-24](1-numerical/NUM-24-transect-right-wall-perimeter/) | A transect's right end wall is left out of the wetted perimeter, the left one is not | `transect-right-wall-perimeter-dropped` | R3 |
| [NUM-25](1-numerical/NUM-25-curb-inlet-capture-nan-in-gutter/) | Curb opening inlets capture all of the flow while the spread is inside a depressed gutter | `inlet-curb-capture-nan-when-spread-within-gutter` | R3 |
| [NUM-26](1-numerical/NUM-26-on-sag-inlet-stale-street-sides/) | An on-sag inlet is scaled by the street sides of the inlet processed before it | `inlet-onsag-stale-nsides` | project notes |
| [NUM-28](1-numerical/NUM-28-timeseries-backward-lookup/) | A time series looked up back in time interpolates from a stale bracket | `tseries-backward-lookup-wrong-bracket` | R3 |
| [NUM-29](1-numerical/NUM-29-zero-roughness-subarea-depression-storage/) | A subarea with Manning's n = 0 counts its depression-storage fill again as runoff | `subarea-n0-dstore-fill-double-count` | R4 |
| [NUM-30](1-numerical/NUM-30-pervious-to-impervious-routing-lost/) | Pervious runoff routed to the impervious area disappears when PctZero = 100 | `perv-to-imperv-routing-lost-without-imperv1` | R4 |
| [NUM-31](1-numerical/NUM-31-outfall-runon-step-ratio/) | Outfall flow routed onto a subcatchment is scaled by the ratio of two runoff steps | `outfall-runon-volume-scaled-by-step-ratio` | R4 |
| [NUM-32](1-numerical/NUM-32-evaporation-and-infiltration-same-water/) | Evaporation and infiltration are both charged to the same ponded water | `evap-and-infil-both-charged-to-same-water` | R4 |
| [NUM-33](1-numerical/NUM-33-horton-max-volume-ignored/) | Horton maximum infiltration volume is ignored for a constant-rate curve | `horton-fmax-ignored-for-constant-rate` | R4 |
| [NUM-34](1-numerical/NUM-34-gwf-variable-k-stale/) | The K variable in [GWF] expressions is another aquifer's conductivity | `gwf-variable-K-stale-module-static` | R4 |
| [NUM-35](1-numerical/NUM-35-gw-upper-et-fraction-above-one/) | An upper-zone ET fraction above 1 makes lower-zone ET negative | `gw-upper-evap-fraction-above-one-negative-lower-et` | R4 |
| [NUM-36](1-numerical/NUM-36-gw-outflow-endpoint-trapezoid/) | Groundwater flow sent to the node is not the volume the aquifer released | `gw-outflow-trapezoid-of-endpoint-rates` | R4 |
| [NUM-37](1-numerical/NUM-37-rooftop-disconnection-zero-roughness/) | A rooftop disconnection with Rough or Slope = 0 never sheds water | `lid-rooftop-zero-roughness-never-overflows` | R5 |
| [NUM-38](1-numerical/NUM-38-permeable-pavement-drain-unbounded/) | Permeable pavement drains water it does not have once pavement and soil are saturated | `lid-pavement-drain-unbounded-when-soil-and-pavement-full` | R5 |
| [NUM-39](1-numerical/NUM-39-bioretention-no-storage-drain/) | A bio-retention cell or rain garden without a storage layer drains water that no layer loses | `lid-biocell-no-storage-drain-creates-water` | R5 |
| [NUM-40](1-numerical/NUM-40-rain-adjustment-read-ahead/) | The monthly rainfall adjustment is applied when a rain record is read ahead, not in the month the rain falls | `rain-adjustment-applied-at-read-ahead-time` | R5 |
| [NUM-41](1-numerical/NUM-41-rdii-rainfall-point-sampled/) | RDII uses a point sample of the rain gage instead of the rain that fell | `rdii-rainfall-point-sampled-misses-rain` | R5 |
| [NUM-42](1-numerical/NUM-42-rdii-cogage-loses-scale-factor/) | RDII ignores a gage's rain scale factor when the gage shares its series with an earlier gage | `rdii-uh-reassigned-to-cogage-loses-scale-factor` | R5 |
| [NUM-43](1-numerical/NUM-43-hargreaves-evaporation-nan-polar/) | Temperature-based (Hargreaves) evaporation is NaN above the polar circles | `hargreaves-nan-above-polar-circle` | R5 |
| [NUM-44](1-numerical/NUM-44-day-of-year-leap-years/) | In leap years, month/day dates for street sweeping and DAYOFYEAR rules are matched one day early | `dayofyear-nonleap-anchor-shifts-leap-years`, `controls-dayofyear-leap-year-off-by-one` | R7; R10 |
| [NUM-45](1-numerical/NUM-45-dry-node-reports-mass-rate-as-concentration/) | A dry node with no inflow reports its pollutant load rate as its concentration | `dry-node-publishes-mass-accumulator-as-concentration` | R6 |
| [NUM-46](1-numerical/NUM-46-quality-final-storage-double-counted/) | Pollutant left behind when a node or link dries out is counted twice in Final Stored Mass | `qual-final-storage-dryout-double-counted` | R6 |
| [NUM-47](1-numerical/NUM-47-quality-ledger-half-step-integration/) | Losses in the first (0.5 s) dynamic-wave step are booked 10 to 30 times over | `routing-ledger-step-rates-integrated-over-next-half-step` | R6 |
| [NUM-48](1-numerical/NUM-48-first-order-decay-explicit-euler/) | First-order decay uses c(1 − K1Δt) instead of the manual's c·exp(−K1Δt), so results depend on the routing step | `first-order-decay-explicit-euler-step-dependent` | R6 |
| [NUM-49](1-numerical/NUM-49-treatment-latch-leaves-minus-one-removal/) | After a cyclic treatment, later pollutants at the node get a removal of -100 % and their concentration doubles | `treatment-latch-leaves-minus1-removal` | project notes |
| [NUM-50](1-numerical/NUM-50-steady-link-api-flux-mass-lost/) | A link API mass flux under STEADY routing is diluted by the conduit volume, and most of it disappears | `sf-link-api-flux-mass-not-conserved` | R6 |
| [NUM-51](1-numerical/NUM-51-mathexpr-power-of-nonpositive-base/) | x^y is 0 for every negative x, so (-3)^2 evaluates to 0 | `mathexpr-pow-nonpositive-base-returns-zero` | R6 |
| [NUM-52](1-numerical/NUM-52-pid-steady-error-reset/) | A PID controller with a steady error keeps integrating through its P and D terms | `controls-pid-steady-error-reset-integrates-p-term` | R10 |
| [NUM-53](1-numerical/NUM-53-rule-step-uses-routing-step/) | With RULE_STEP > 0 the rules use the routing step as their time step | `controls-rule-step-uses-routing-step-dt` | R10 |
| [NUM-54](1-numerical/NUM-54-slot-velocity-uses-slot-area/) | With the Preissmann slot, 5.2.4's friction term uses the slot area for the velocity | `slot-velocity-includes-slot-area` | project notes |
| [NUM-55](1-numerical/NUM-55-storage-depth-newton-wrong-index/) | 5.2.4 computes the depth of FUNCTIONAL, CONICAL and PYRAMIDAL storage units with another node's geometry and mixed units | `storage-getvoldiff-wrong-index-and-units`, `storage-getvoldiff-wrong-index-and-units-repro` | project notes; R2 |
| [NUM-56](1-numerical/NUM-56-modified-horton-max-volume-524/) | 5.2.4's Modified Horton stops infiltrating after one step when Fmax is set | `modhorton-fmax-max-instead-of-min` | R4 |
| [NUM-57](1-numerical/NUM-57-transect-radius-1-49-524/) | 5.2.4 builds transect hydraulic radii with 1.49 instead of 1.486 | `transect-hrad-1p49-vs-phi-524` | R3 |
| [NUM-58](1-numerical/NUM-58-washoff-cutoff-drops-load-524/) | 5.2.4 sends runoff below 0.001 in/hr to the network with no pollutant, and drops its load | `surfqual-low-runoff-cutoff-drops-load` | R6 |
| [NUM-59](1-numerical/NUM-59-node-only-model-not-routed/) | 5.2.4 never routes the inflows of a model that has nodes but no links | `node-only-model-inflows-never-routed` | R2 |
| [CON-01](2-conceptual/CON-01-surcharged-storage-volume-pinned/) | The surcharge band of a closed storage unit holds water its volume never books | `dw-surcharged-storage-no-dqdh-volume-pinned` | R1 |
| [CON-02](2-conceptual/CON-02-min-surface-area-overrides-storage-curve/) | MIN_SURFAREA overrides the area curve of a small storage unit | `dw-minsurfarea-floor-applied-to-storage-curves` | R1 |
| [CON-03](2-conceptual/CON-03-lid-runon-to-every-partial-unit/) | Run-on onto a subcatchment that is only partly LID is counted twice | `lid-runon-added-to-every-partial-lid-unit` | R5 |
| [CON-04](2-conceptual/CON-04-covered-barrel-returns-rain-over-all-lids/) | A covered rain barrel returns the rain that fell on every LID unit of its subcatchment | `lid-covered-barrel-return-uses-total-lid-area` | R5 |
| [CON-05](2-conceptual/CON-05-covered-barrel-rain-lost/) | The rain falling on a covered rain barrel disappears from the water balance | `lid-covered-barrel-rain-lost` | R5 |
| [CON-06](2-conceptual/CON-06-snow-plowing-ignores-areas/) | Snow plowed to another subcatchment changes volume by the ratio of the two subcatchments' areas | `snow-plow-to-subcatch-ignores-area-ratio` | R5 |
| [CON-07](2-conceptual/CON-07-groundwater-et-reduced-by-impervious-evaporation/) | Evaporation from the impervious surface reduces groundwater ET | `gw-surface-evap-includes-impervious` | R4 |
| [CON-08](2-conceptual/CON-08-mass-load-at-dry-node-discarded/) | A MASS-type pollutant load at a node with no flow is booked as inflow and then thrown away | `mass-inflow-to-no-flow-node-vanishes` | R6 |
| [CON-09](2-conceptual/CON-09-ponded-evaporation-destroys-pollutant/) | Pollutant in water ponded on a subcatchment disappears when the water evaporates | `ponded-evaporation-destroys-pollutant-mass` | R6 |
| [CON-10](2-conceptual/CON-10-withdrawal-removes-no-pollutant/) | A lateral withdrawal removes water from a node but no pollutant, so the node's concentration is inflated | `lateral-withdrawal-removes-no-mass` | R2 |
| [CON-11](2-conceptual/CON-11-treatment-reference-uses-wrong-pollutant-type/) | A pollutant named in a treatment equation is read by the wrong equation's type | `treatment-pollutant-ref-uses-other-pollutants-type` | R6 |
| [CON-12](2-conceptual/CON-12-cyclic-treatment-error-unreachable/) | ERROR 161 for a cyclic treatment dependency can never be reported | `treatment-cyclic-error-unreachable` | project notes |
| [CON-14](2-conceptual/CON-14-lid-drain-creates-washoff-mass/) | Water released by a LID underdrain is counted as removed and as delivered, so pollutant mass is created | `lid-drain-release-creates-washoff-mass` | R6 |
| [CON-15](2-conceptual/CON-15-kw-empty-conduit-inlet-area-jump/) | KINWAVE books water that never entered when an inflow starts abruptly in an empty conduit | `kw-inlet-area-jump-creates-water` | R2 |
| [CON-16](2-conceptual/CON-16-subarea-runoff-end-of-step-rate/) | Subarea runoff is booked as the end-of-step rate times the step, not as the water that left | `subarea-runoff-volume-end-of-step-rate` | R4 |
| [CON-17](2-conceptual/CON-17-custom-ellipse-ignores-major-axis/) | A custom elliptical pipe gets the area of a standard ellipse, whatever its span | `xsect-nonstd-ellipse-area-ignores-axis`, `xsect-nonstd-ellipse-fix-claimed-not-applied` | project notes (incidental); R3 |
| [CON-18](2-conceptual/CON-18-slot-area-not-monotonic/) | The Preissmann slot area shrinks as the head rises and does not match the slot width | `slot-area-nonmonotonic-geometry-mismatch` | project notes |
| [CON-19](2-conceptual/CON-19-swale-vegetation-fraction-inconsistent/) | Vegetative swales and permeable pavement lose track of surface water when VegFrac > 0 | `lid-swale-vegfrac-storage-inconsistent` | R5 |
| [CON-20](2-conceptual/CON-20-swale-infiltration-diluted-by-imperviousness/) | A vegetative swale infiltrates at the soil's rate times the subcatchment's pervious fraction | `lid-swale-native-infil-diluted-by-impervious-fraction` | R5 |
| [CON-21](2-conceptual/CON-21-statistics-averaged-over-steps/) | The summary averages are means over routing steps, not over time | `stats-step-count-weighted-averages` | R8 |
| [CON-22](2-conceptual/CON-22-pump-status-on-needs-setting-one/) | `PUMP x STATUS = ON` is false for a pump running at any speed other than 1 | `controls-pump-status-on-requires-setting-1` | R10 |
| [CON-23](2-conceptual/CON-23-control-only-gage-never-updated/) | A rain gage that only a control rule reads is never updated | `controls-gage-premise-unused-gage-reads-zero` | R10 |
| [CON-24](2-conceptual/CON-24-snow-cold-content-cap/) | The snow pack's cold content is capped 12 times below the manual's limit | `snow-cold-content-cap-12x-below-manual` | R5 |
| [BND-01](3-boundary/BND-01-free-outfall-below-regulator-gets-crest-depth/) | A FREE or NORMAL outfall below a weir or orifice is given the crest height as its depth | `outfall-free-normal-depth-uses-regulator-crest` | R2 |
| [BND-02](3-boundary/BND-02-tidal-outfall-elapsed-hours/) | A TIDAL outfall reads its curve at the hours since the run started, not at the hour of the day | `tidal-outfall-elapsed-hours-not-clock-hour`, `tidal-outfall-elapsed-not-clock-hour` | R1; R2 |
| [BND-03](3-boundary/BND-03-outfall-backflow-concentration-ignored/) | A CONCEN inflow at an outfall never sets the quality of its reverse flow | `outfall-concen-backflow-boundary-broken`, `outfall-reverse-flow-concen-inflow-dead` | R2; R6 |
| [BND-04](3-boundary/BND-04-outfall-backflow-mass-unbooked/) | Pollutant mass carried in by an outfall's reverse flow is not booked | `outfall-backflow-pollutant-mass-unbooked` | R6 |
| [BND-05](3-boundary/BND-05-initial-depth-uses-upstream-offset-twice/) | A conduit's inlet offset is added to the initial depth of its downstream node | `dw-init-node-depth-uses-offset1-for-both-ends` | R1 |
| [BND-07](3-boundary/BND-07-culvert-inlet-control-adverse-slope/) | A culvert on an adverse slope loses inlet control and gets it at its outlet instead | `culvert-inlet-control-lost-on-adverse-slope` | R1 |
| [BND-08](3-boundary/BND-08-ponding-ignores-surcharge-depth/) | With ponding allowed, a junction ponds at its rim and ignores its surcharge depth | `dw-ponding-starts-at-ymax-ignoring-ysur` | R1 |
| [BND-09](3-boundary/BND-09-duration-floor-drops-last-period/) | The run stops 1 s early and loses its last reporting period for many end times | `total-duration-floor-drops-last-second-and-period`, `total-duration-floor-drops-last-second`, `last-report-period-dropped-duration-floor` | R2; R7; R8 |
| [BND-10](3-boundary/BND-10-events-skip-rule-step-error-107/) | Control rules stop between [EVENTS] and the run aborts with ERROR 107 when RULE_STEP is not a multiple of ROUTING_STEP | `events-fixed-step-skips-rule-grid-error107`, `controls-rule-step-skipped-between-events-err107` | R2; R10 |
| [BND-11](3-boundary/BND-11-hotstart-groundwater-flow-lost/) | A hot start drops the groundwater outflow for the first runoff step | `gw-hotstart-flow-restored-into-oldflow`, `hotstart-gw-flow-restored-into-oldflow-only` | R4; R7 |
| [BND-12](3-boundary/BND-12-timeopen-from-midnight/) | TIMEOPEN and TIMECLOSED count from midnight of the start date, not from the start of the run | `controls-timeopen-measured-from-start-midnight` | R10 |
| [BND-13](3-boundary/BND-13-custom-inlet-rating-ignores-max-flow/) | Custom inlets with a RATING curve ignore the Qmax capture limit | `inlet-custom-rating-ignores-maxflow` | R3 |
| [BND-14](3-boundary/BND-14-runon-to-zero-area-subcatchment/) | Runoff sent to a subcatchment with zero area disappears | `runon-to-zero-area-subcatchment-lost` | R4 |
| [BND-15](3-boundary/BND-15-negative-evaporation-adjustment/) | A negative evaporation adjustment larger than the base rate makes evaporation add water | `evap-adjustment-can-make-evaporation-negative` | R5 |
| [BND-16](3-boundary/BND-16-evaporation-series-start-between-entries/) | An evaporation time series uses the next entry's rate when the run starts between two entries | `evap-timeseries-start-uses-next-entry` | R5 |
| [BND-17](3-boundary/BND-17-polar-temperature-interpolation/) | Temperatures interpolated from daily Tmax/Tmin jump at sunrise and never reach Tmax in polar winter | `climate-polar-temperature-interpolation-jump` | R5 |
| [BND-18](3-boundary/BND-18-averages-not-reset-at-report-start/) | With AVERAGES YES and a late REPORT_START, the first saved period averages the whole run up to it | `avg-results-not-reset-before-report-start` | R8 |
| [BND-19](3-boundary/BND-19-report-start-mixes-whole-run-volumes/) | With a late REPORT_START, summary rows mix whole-run volumes with reporting-period statistics | `report-start-mixes-whole-run-volumes` | R8 |
| [BND-20](3-boundary/BND-20-summary-nan-without-routing-step/) | Summary tables print -nan when no routing step falls in the reporting period | `stats-nan-when-no-routing-step-after-report-start`, `zero-step-summary-divide-by-zero-api` | R8; R9 |
| [IO-01](4-io/IO-01-nan-inf-accepted-as-numbers/) | "nan" and "inf" are accepted as numbers in the input file | `getdouble-accepts-nan-inf`, `getdouble-nan-sweep-evidence` | R7; R9 |
| [IO-02](4-io/IO-02-time-options-accept-nan-inf-huge/) | Clock-time and step options accept nan, huge and negative values | `time-options-decimal-hours-accept-nan-inf-overflow` | R9 |
| [IO-03](4-io/IO-03-negative-nan-rainfall-accepted/) | Negative and NaN rainfall values in a time series or rain file are used as rain | `rainfall-negative-and-nan-values-accepted` | R9 |
| [IO-04](4-io/IO-04-step-options-three-unit-conventions/) | A bare number in a time option means hours or seconds depending on the option, and 6.0.0 reads it as seconds everywhere | `step-option-unit-conventions` | project notes |
| [IO-05](4-io/IO-05-long-lines-split-silently/) | Input lines longer than 1023 characters are split in two, and the check for them never fires | `input-long-line-split-silently` | project notes (incidental) |
| [IO-06](4-io/IO-06-tokens-past-40-dropped/) | Values past the 40th token on an input line are dropped without a message | `maxtoks-silent-truncation` | project notes |
| [IO-07](4-io/IO-07-semicolon-inside-quotes/) | A ';' inside a quoted file name is read as the start of a comment | `gettokens-semicolon-inside-quotes` | project notes (incidental) |
| [IO-08](4-io/IO-08-utf8-bom-hides-first-section/) | A UTF-8 byte order mark hides the first section of the input file | `utf8-bom-hides-first-section` | R7 |
| [IO-09](4-io/IO-09-outfall-gate-and-routeto/) | An outfall's flap gate is dropped when the line also names a RouteTo subcatchment | `outfall-flapgate-and-routeto-exclusive` | project notes |
| [IO-10](4-io/IO-10-barrels-stored-as-char/) | The number of barrels is stored in a char and wraps above 127 | `barrels-stored-as-char` | project notes |
| [IO-11](4-io/IO-11-irregular-street-barrels-ignored/) | The Barrels item of an IRREGULAR cross section is ignored | `irregular-street-barrels-ignored` | project notes |
| [IO-12](4-io/IO-12-ellipse-arch-size-code-overrides-dimensions/) | Ellipse and arch pipes: a zero width turns the full height into the size code | `xsect-size-code-overrides-dimensions` | project notes |
| [IO-13](4-io/IO-13-xsection-error-empty-token/) | Invalid cross-section geometry is reported as "invalid number" with no number | `xsect-error-reported-with-empty-token` | project notes |
| [IO-14](4-io/IO-14-culvert-code-out-of-range/) | A culvert code above 57 is accepted and turns off normal-flow limiting | `culvert-code-out-of-range-disables-normal-flow-limit` | project notes |
| [IO-15](4-io/IO-15-groundwater-needs-eleven-tokens/) | [GROUNDWATER] rows with the 10 required fields are rejected | `groundwater-requires-eleven-tokens` | project notes |
| [IO-16](4-io/IO-16-link-properties-reset-by-section-order/) | Link properties in a section placed above the link's own section are lost or land on another link | `section-order-resets-link-properties` | project notes |
| [IO-17](4-io/IO-17-subcatchment-section-order/) | Subcatchment data placed above [SUBCATCHMENTS] is lost or misapplied | `subcatch-section-order-dependence` | R7 |
| [IO-18](4-io/IO-18-pct-zero-over-100/) | [SUBAREAS] accepts a PctZero above 100 and runs with a negative sub-area | `subarea-pctzero-over-100-negative-area` | R7 |
| [IO-19](4-io/IO-19-coverage-percent-unvalidated/) | Land use coverages above 100 % or below 0 % are accepted and scale pollutant loads | `coverage-percent-unvalidated` | R7 |
| [IO-20](4-io/IO-20-subareas-bad-number-error-209/) | An invalid [SUBAREAS] number is reported as "undefined object"; 6.0.0 accepts it | `subareas-bad-number-reported-as-undefined-object` | R4 |
| [IO-21](4-io/IO-21-pattern-factor-count-unchecked/) | A time pattern with a missing or extra factor is accepted without a message | `pattern-factor-count-not-validated` | R2 |
| [IO-22](4-io/IO-22-rdii-negative-recession-accepted/) | A negative RDII recession limb ratio K is accepted, and K < -1 crashes the legacy engines | `rdii-uh-negative-k-accepted` | R5 |
| [IO-23](4-io/IO-23-unit-hydrograph-without-gage/) | A unit hydrograph group without a rain gage line is accepted and silently uses gage 0 | `rdii-uh-without-gage-defaults-to-gage0` | R5 |
| [IO-24](4-io/IO-24-rain-barrel-covered-default/) | Rain barrel Covrd token: the manual's default is not the parser's, and 6.0.0 ignores the token | `lid-rain-barrel-covered-default-contradicts-manual` | R5 |
| [IO-25](4-io/IO-25-gutter-depression-units/) | The input reference gives gutter depressions in inches or mm; the engine reads feet or metres | `gutter-depression-units-ft-vs-manual-inches` | R3 |
| [IO-26](4-io/IO-26-climate-file-wind-not-converted/) | Wind speed from a user-prepared climate file is not converted from km/hr in SI models | `climate-user-file-wind-not-converted-si` | R5 |
| [IO-27](4-io/IO-27-ghcn-default-temperature-units/) | A GHCN climate file without a Units token is read as deg F or deg C, not in the documented default of tenths of a degree C | `climate-ghcn-default-units-contradict-manual` | R5 |
| [IO-28](4-io/IO-28-transect-not-finalized/) | A transect is only built when an NC line or a known section header follows it | `transect-not-built-when-nc-omitted`, `transect-last-not-validated` | R3; R7 |
| [IO-29](4-io/IO-29-transect-x1-token-layout/) | An X1 line written as the manual documents it is misread | `transect-x1-token-layout-contradicts-manual` | R3 |
| [IO-30](4-io/IO-30-rule-variable-prefix-match/) | Control-rule VARIABLE and EXPRESSION names are looked up by prefix | `controls-named-variable-prefix-match` | project notes |
| [IO-31](4-io/IO-31-nodestats-matched-as-nodes/) | The deprecated [REPORT] keyword NODESTATS is read as NODES and rejected | `report-nodestats-shadowed-by-node-prefix` | R7 |
| [IO-32](4-io/IO-32-grate-type-prefix-match/) | Grate type P_BAR-50x100 is read as P_BAR-50 | `inlet-grate-pbar50x100-parsed-as-pbar50` | R7 |
| [IO-33](4-io/IO-33-keyword-index-in-plain-char/) | Misspelt [FILES] and [REPORT] keywords are not caught where char is unsigned, and never in 6.0.0 | `char-findmatch-unsigned-char-platforms` | R7 |
| [IO-34](4-io/IO-34-rule-settings-unbounded/) | Rule actions give links settings outside their valid range | `controls-modulated-and-pump-settings-unbounded` | R10 |
| [IO-35](4-io/IO-35-report-start-date-without-time/) | REPORT_START_DATE is ignored when REPORT_START_TIME is not given | `report-start-date-ignored-without-time` | R7 |
| [IO-36](4-io/IO-36-rdii-file-relative-path/) | A relative RDII interface file name is resolved against the working directory, not the input file's folder | `rdii-file-path-not-resolved-to-inp-dir` | R7 |
| [IO-37](4-io/IO-37-use-files-opened-read-write/) | USE HOTSTART, RAINFALL and RUNOFF files must be writable to be read | `use-files-opened-read-write` | R7 |
| [IO-38](4-io/IO-38-timeseries-file-without-final-newline/) | A time-series file without a final line break ends on its second-to-last value | `tseries-file-no-final-newline-holds-previous-value` | R3 |
| [IO-39](4-io/IO-39-input-warnings-not-counted/) | Unknown-option and unknown-section warnings are not counted, and the section warning is written twice | `input-warnings-not-counted-and-duplicated` | R10 |
| [IO-40](4-io/IO-40-expression-paren-minus-number/) | An expression with "-" right after ")" is rejected, so "(TSS)-1" is ERROR 233 | `mathexpr-paren-then-minus-digit-parse-error` | R6 |
| [IO-41](4-io/IO-41-expression-malformed-exponent/) | A malformed number in an expression ("2.5E+", ".") is accepted instead of rejected | `mathexpr-malformed-exponent-silently-zero` | R6 |
| [IO-42](4-io/IO-42-hotstart-short-read/) | A truncated or mismatched hot start file is applied without an error | `hotstart-short-read-not-detected`, `hotstart-short-read-link-state-evidence` | R7; R9 |
| [IO-43](4-io/IO-43-dated-hotstart-save-time/) | A hot start file saved at a given date holds the state after 45 seconds | `hotstart-timed-save-compares-msec-to-datetime`, `hotstart-dated-save-compares-date-with-msec` | R7; R10 |
| [IO-44](4-io/IO-44-hotstart-modified-horton-state/) | A hot start file forgets how much Modified Horton has infiltrated toward Fmax | `hotstart-drops-modhorton-fmh` | R4 |
| [IO-45](4-io/IO-45-runoff-file-evaporation-24x/) | Evaporation read from a runoff interface file is 24 times too large | `runoff-iface-evap-loss-24x` | project notes |
| [IO-46](4-io/IO-46-runoff-file-groundwater-flow/) | Groundwater flow read from a runoff interface file is multiplied by the area again | `runoff-iface-gw-flow-total-read-as-per-area` | R4 |
| [IO-47](4-io/IO-47-runoff-file-groundwater-elevation-sign/) | The groundwater table read from a runoff interface file is mirrored below the aquifer bottom | `runoff-iface-gw-elevation-sign` | project notes (incidental) |
| [IO-48](4-io/IO-48-outflows-file-not-interpolated/) | The routing interface file holds the outfall state at the end of a routing step, stamped with an earlier report time | `outflows-iface-not-interpolated-to-report-time` | R7 |
| [IO-49](4-io/IO-49-output-header-report-start-date/) | The .out header's report start date is wrong when REPORT_START is later than START | `out-header-report-start-date-off-by-steps` | R8 |
| [IO-50](4-io/IO-50-continuity-error-hides-nan/) | A continuity table full of NaN reports "Continuity Error (%) 0.000" | `qual-continuity-nan-masked-as-zero`, `flow-continuity-nan-masked-input-repro` | project notes; R7 |
| [IO-51](4-io/IO-51-continuity-forcing-ignores-negative-errors/) | With CONTINUITY NO, a large negative continuity error is not reported | `continuity-report-forcing-ignores-negative-errors` | R6 |
| [IO-52](4-io/IO-52-runtime-errors-not-in-report/) | Errors 107 and 307 stop the run but are not written to the report | `runtime-errors-not-written-to-report` | R10 |
| [IO-53](4-io/IO-53-instability-index-zero-hidden/) | "All links are stable" when the most unstable link is the first one in the input | `instability-nonconv-index0-reported-as-none` | R8 |
| [IO-54](4-io/IO-54-pump-min-flow-always-zero/) | The Pumping Summary's "Min Flow" is always 0.00 | `pump-min-flow-always-zero` | R8 |
| [IO-55](4-io/IO-55-pump-off-curve-low-counted-high/) | The Pumping Summary books all off-curve time of Type 1, 2, 3 and 5 pumps as "High" | `pump-offcurve-yes-counted-as-high` | R8 |
| [IO-56](4-io/IO-56-normal-flow-limited-when-dry/) | The Flow Classification Summary counts time a conduit is dry as "Norm Ltd" | `normal-flow-flag-stale-when-dry` | R8 |
| [IO-57](4-io/IO-57-system-runoff-double-counts-runon/) | System runoff and total lateral inflow in the .out count runon twice | `sys-runoff-double-counts-runon` | R8 |
| [IO-58](4-io/IO-58-node-inflow-volume-includes-initial-storage/) | "Total Inflow Volume" of a node includes the water it held at the start | `node-inflow-volume-includes-initial-storage` | R8 |
| [IO-59](4-io/IO-59-report-flow-columns-merge/) | Flows of 100,000 or more run into the neighbouring column of the Node Inflow Summary | `rpt-flow-columns-merge-large-values` | R8 |
| [IO-60](4-io/IO-60-lid-idle-unit-100-percent-error/) | The LID Performance Summary reports a 100 % continuity error for a LID unit that received no water | `lid-summary-reports-100pct-error-for-idle-unit`, `lid-continuity-100pct-when-no-inflow` | R5; R8 |
| [IO-61](4-io/IO-61-report-gallons-factor-7-48/) | Summary tables convert ft³ to gallons with 7.48, the continuity tables with 7.48056 | `report-volume-factor-7-48` | R8 |
| [IO-62](4-io/IO-62-si-street-flow-header/) | The SI Street Flow Summary has no "Count" label over the inlet count | `inlet-si-report-header-missing-count` | project notes |
| [CRASH-01](5-crashes/CRASH-01-datetime-timediff-overflow/) | datetime_timeDiff() overflows an int conversion once a rain series has ended | `datetime-timediff-double-to-int-overflow` | R9 |
| [CRASH-02](5-crashes/CRASH-02-xsection-before-link-heap-overflow/) | An [XSECTIONS] row read before its link's own row writes past the conduit array | `xsect-before-link-type-heap-overflow` | R9 |
| [CRASH-03](5-crashes/CRASH-03-curve-without-points/) | A curve or time series without data points crashes the input check | `curve-without-points-null-deref` | R3 |
| [CRASH-04](5-crashes/CRASH-04-curve-id-only-line-count-pass/) | The object-count pass misreads a curve's type: a name-only line crashes, a quoted "Shape" overflows the heap | `count-pass-curve-type-token-crash`, `curve-id-only-line-fuzz-evidence` | R7; R9 |
| [CRASH-05](5-crashes/CRASH-05-expression-buffer-overflow/) | A long number or name in an expression overflows a 255-byte buffer | `mathexpr-number-and-token-buffer-overflow`, `mathexpr-token-identifier-crash-evidence` | R6; R9 |
| [CRASH-06](5-crashes/CRASH-06-sweep-date-buffer-overflow/) | A SWEEP_START or SWEEP_END value of 24 or more characters overflows a stack buffer | `sstrcat-overflow-sweep-start-end` | R9 |
| [CRASH-07](5-crashes/CRASH-07-xsection-lookup-nan-index/) | Cross-section table lookups convert an unchecked ratio to an int index | `xsect-lookup-unguarded-index-segv` | R9 |
| [CRASH-08](5-crashes/CRASH-08-rule-expression-freed-after-input-error/) | A model with a control-rule EXPRESSION crashes on close after an early input error | `controls-delete-null-expression-after-input-error` | R9 |
| [CRASH-09](5-crashes/CRASH-09-snow-removal-without-subcatchment/) | A snow REMOVAL line with Fsub > 0 but no receiving subcatchment indexes Subcatch[-1] | `snow-removal-fsub-without-subcatch-oob` | R5 |
| [CRASH-10](5-crashes/CRASH-10-rdii-file-node-index-unchecked/) | Node indexes read from a binary RDII interface file are not range-checked | `rdii-iface-node-index-not-bounds-checked` | R5 |
| [CRASH-11](5-crashes/CRASH-11-averages-report-out-of-bounds/) | With AVERAGES YES, 5.3.0 reads past the averaged node results and converts SI depths twice | `avg-max-rpt-depth-oob-and-double-ucf` | R8 |
| [CRASH-12](5-crashes/CRASH-12-steady-link-quality-node-index/) | Steady-flow link quality indexes the Link array with a node index | `sf-link-qual-node-index-as-link-index` | R6 |
| [CRASH-13](5-crashes/CRASH-13-pollutant-flux-before-start/) | Setting a pollutant mass flux before swmm_start crashes (link) or writes out of bounds (node) | `api-pollutant-flux-prestart-crash`, `api-link-latmass-flux-prestart-null-deref`, `api-node-latmass-flux-prestart-unchecked-and-discarded` | R10; R6; R6 |
| [CRASH-14](5-crashes/CRASH-14-getcount-getindex-object-type/) | 5.3.0 swmm_getCount / swmm_getIndex read past their arrays for object types 18 to 99 | `api-getcount-getindex-objtype-oob` | R10 |
| [CRASH-15](5-crashes/CRASH-15-landuse-setter-timeseries-index/) | The land-use buildup setters accept any number as an external-buildup time-series index | `api-landuse-buildup-coeff-tseries-index-oob` | R10 |
| [CRASH-16](5-crashes/CRASH-16-getsavedvalue-index-unchecked/) | swmm_getSavedValue reads outside the object arrays for an out-of-range index | `api-getsavedvalue-index-unchecked` | R10 |
| [CRASH-17](5-crashes/CRASH-17-getname-geterror-buffer-overrun/) | swmm_getError and swmm_getName write one byte past the caller's buffer | `api-getname-geterror-writes-past-buffer` | R10 |
| [CRASH-18](5-crashes/CRASH-18-output-reader-system-result-bad-period/) | SMO_getSystemResult() reads into an unallocated buffer when the period is out of range | `out-reader-getsystemresult-missing-else` | R8 |
| [CRASH-19](5-crashes/CRASH-19-output-reader-null-handle/) | The output reader dereferences a NULL handle right after testing for it | `out-reader-null-handle-deref` | R8 |
| [CRASH-20](5-crashes/CRASH-20-error-string-overflow-524/) | 5.2.4 overflows its 256-byte error-text buffer on a long input token | `errstring-overflow-long-token`, `errstring-overflow-fuzz-evidence` | R7; R9 |
| [CRASH-21](5-crashes/CRASH-21-single-point-rain-series-524/) | 5.2.4 converts 8.64e14 to int when a rain gage's time series has one record | `gage-single-point-series-int-overflow-ub`, `gage-single-point-series-corpus-evidence` | R5; R9 |
| [API-01](6-api/API-01-settings-before-start-reset/) | Forcing values set before swmm_start are accepted, then wiped by swmm_start | `api-prestart-settings-silently-reset` | R10 |
| [API-02](6-api/API-02-call-order-error-sticky/) | A refused out-of-order call blocks the project or aborts the run | `api-call-order-error-sticky` | R10 |
| [API-03](6-api/API-03-api-error-messages-empty/) | 5.3.0's API error codes have no message text | `api-530-error-codes-renumbered-message-empty` | R10 |
| [API-04](6-api/API-04-statistics-getters-raw-accumulators/) | The statistics getters return raw accumulators: sums for averages, seconds for hours, volumes for depths, feet in SI projects | `api-stats-getters-raw-accumulators`, `stats-api-raw-accumulators` | R10; R8 |
| [API-05](6-api/API-05-quality-getters-wrong-units/) | Ponded concentration and link pollutant load come out of the toolkit in the wrong units | `api-subcatch-quality-getters-setters-wrong-units` | R10 |
| [API-06](6-api/API-06-external-buildup-per-step/) | External pollutant buildup set through the API is added again at every runoff step | `api-ext-buildup-per-step-and-getter-scaled` | R6 |
| [API-07](6-api/API-07-buildup-getter-sums-land-uses/) | The subcatchment buildup getter adds up the land uses' buildup densities | `api-subcatch-buildup-sums-landuse-densities`, `api-subcatch-buildup-getter-sums-landuse-densities` | R10; R6 |
| [API-08](6-api/API-08-mass-flux-litres-conversion/) | A pollutant mass flux set through the API delivers 28.3 times the requested mass | `api-qual-mass-flux-missing-lperft3` | R6 |
| [API-09](6-api/API-09-subcatchment-geometry-setters-skip-alpha/) | Setting a subcatchment's area, width or slope before the run does not change its runoff | `api-subcatch-geometry-setters-skip-alpha` | R10 |
| [API-10](6-api/API-10-invert-offset-setters-stale-slope/) | Setting a node invert or a conduit offset before the run leaves the conduit's slope and capacity unchanged | `api-node-invert-link-offset-stale-conduit-slope` | R10 |
| [API-11](6-api/API-11-min-slope-percent-vs-fraction/) | The minimum-slope option reads back as a fraction, is written as given, and has no effect when set before the run | `api-minslope-percent-vs-fraction-and-no-effect` | R10 |
| [API-12](6-api/API-12-max-route-step-returns-min/) | swmm_getValue(swmm_MAXROUTESTEP) always returns the minimum routing step | `api-maxroutestep-returns-minroutestep` | R10 |
| [API-13](6-api/API-13-api-rainfall-not-wet-step/) | Rainfall injected on a subcatchment through the API is computed with the dry time step | `api-subcatch-rainfall-ignored-by-runoff-step` | R4 |
| [API-14](6-api/API-14-gage-rainfall-setter-cogage/) | Gage rainfall set through the API is ignored for a gage that shares its time series with another gage | `api-gage-rainfall-ignored-on-cogage` | R5 |
| [API-15](6-api/API-15-gage-scale-factor-runtime-change/) | A gage scale factor changed during a run applies late to its gage and rescales the gages that share its series | `api-gage-scalefactor-runtime-change-delayed-and-corrupts-cogage` | R5 |
| [API-16](6-api/API-16-output-reader-range-checks/) | The output reader returns another element's results for an out-of-range index, period or attribute | `out-reader-no-range-checks` | R8 |
| [API-17](6-api/API-17-output-reader-type-code-as-float/) | SMO_getPropertyValue() returns node and link type codes as the bits of an integer read into a float | `out-reader-property-typecode-as-float` | R8 |
| [API-18](6-api/API-18-cli-exit-code-zero-on-error/) | The command line programs exit with status 0 when the run fails | `cli-exit-code-always-zero` | project notes |

## Merged into another issue

| Candidate | Merged into | Why |
|---|---|---|
| NUM-27 (`inlet-flooding-credit-vs-backflow-mismatch`) | NUM-02 | Its only material effect is NUM-02's backflow ratios. With correct ratios (5.2.4) the remaining one-step lag between backflow and the flooding credit was 3.8 ft³ of 15,420 ft³ of overflow (0.006 % of inflow). |

## Dropped

These candidates were not written up. The reason is the one recorded when they were dropped.

| Candidate | Reviewer | Why it was dropped |
|---|---|---|
| NUM-21 (`egg-low-depth-section-factor-inconsistent`) | R3 | Checked against the exact standard egg (which reproduces SWMM's full area and radius exactly): the large low-flow errors come from the 51-point tables' resolution below about 2.4 % full, where the egg is a circle of radius H/6. Four stale table entries (`S_Egg`, `Y_Egg`) are real, but correcting them alone makes kinematic-wave depths worse below 1.5 % full; a proper fix needs exact circle geometry at the invert, a design change. Listed in [further-findings.md](further-findings.md). |
| CON-13 (`treatment-removal-per-step-and-masslost-formula`) | project notes | The Mass Reacted ledger is consistent (quality continuity -0.05 % at 5, 30 and 60 s steps). The step dependence of the removal is the documented per-step treatment formulation; the manual's n-th order example multiplies by DT for that reason. |
| BND-06 (`roadway-weir-ignores-flap-gate`) | R1 | Documented behaviour: the Hydraulics Reference Manual (Vol II, 7.5) says a roadway weir has neither a control setting nor a flap gate, and the `[WEIRS]` reference says Gated does not apply to ROADWAY weirs. The only remaining point is that Gated=YES is accepted without a warning. |
| `truncated-pi-and-friction-exponent` | project notes | effect below 1e-6 relative; not a defect worth a patch |
| `subcatch-area-lossy-roundtrip` | project notes | one-ulp float artefact |
| `fmt18-unbalanced-bracket` | project notes | output is correct; maintainability note only |
| `inlet-tcrown-qfactor-not-set-for-channels` | project notes | no reachable consumer of the stale value |
| `inlet-splash-coeffs-seven-rows` | project notes | out-of-bounds row is unreachable (GENERIC special-cased) |
| `outfall-connectivity-validated-dw-only` | project notes | two inflow links at an outfall are valid under KW/STEADY |
| `slot-water-booked-in-link-storage` | project notes | design choice of the slot method |
| `rdii-uh-times-truncated-to-seconds` | project notes | volume preserved; sub-second effect |
| `cms-flow-factor-inconsistent` | project notes | 0.01 % inconsistency, cancels in conversions |
| `silent-exit-mid-read-14600-nodes` | project notes | deck unavailable, mechanism unknown |
| `silent-exit-candidate-mechanisms` | R9 | deck unavailable, mechanism unknown |
| `dw-unconverged-iterate-accepted-stiff-weir-nodes` | R1 | accepting a non-converged iterate is the documented design; reported as nonconvergence |
| `getaofs-above-sfull-shape-dependent-normal-depth` | R3 | low confidence; no wrong result shown beyond an unreachable branch |
| `node-continuity-error-two-definitions` | R8 | two documented definitions; low impact |
| `gwater-odesolve-failure-ignored` | project notes | no input found that makes odesolve fail |
| `odesolve-failure-ignored-subarea-too` | R4 | no input found that makes odesolve fail |
| `inlet-onsag-capture-unthrottled-under-dw` | project notes | low confidence; DW node continuity limits the capture |

## 6.0.0-only candidates

These describe defects that exist only in 6.0.0. They are listed in [further-findings.md](further-findings.md) rather than as issues.

- `v6-treatment-reacted-rate-booked-as-mass` (R6)
- `v6-runoff-quality-remaining-omits-ponded-mass` (R6)
- `v6-flow-units-unknown-keyword-runs-as-cfs` (R9)
- `v6-expression-undefined-variable-evaluates-zero` (R9)
- `v6-functional-storage-negative-a0-accepted` (R9)
