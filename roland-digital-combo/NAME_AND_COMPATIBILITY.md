# Roland Digital Combo — branding and compatibility

The user-facing plugin product name, GUI title, host report name, Windows installer name, and CI bundle path are **Roland Digital Combo**.

To preserve existing projects, these identifiers intentionally remain unchanged:

- JUCE target: `DigitalCombo`
- VST3 `PLUGIN_CODE` / `PLUGIN_MANUFACTURER_CODE`, company name, bundle ID
- APVTS root tag `DigitalCombo`, XML state and patch tags `DigitalComboState` / `DigitalComboPatch`
- Existing patch location `Documents/Digital Combo/Patches` and `.dcpatch` format

Existing patch data should therefore remain readable. The changed bundle filename and product title may require DAW rescanning; project compatibility must be verified on Windows/FL Studio 25 before making a final claim.

Roland is a third-party trademark. This is an independent, unofficial research project; no endorsement, original ROM, or original factory samples are included. Use of the mark in public distribution requires separate legal review.

Historical validator logs document earlier builds that identified themselves as `Digital Combo`; they are intentionally unchanged. A VST3 rebuild and host validation of the renamed product are still required.
