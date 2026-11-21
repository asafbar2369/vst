# P0 controls checkpoint (not release acceptance)

Based on Claude 12d801d6b1adbc6af5ae69e08add48bd54d417f6 and Codex 8x boundary regression correction.

- Existing master_volume ID and index remain the independent post-FX master (-60 dB means silence, neutral 0 dB). A 20 ms linear gain ramp reaches exact zero.
- Appended preset_volume provides static post-FX calibration (-60 to +18 dB); old preset master_volume migrates to this trim. Factory trim requests above +6 dB are no longer silently truncated, including White Noise Sweep's +9.2 dB. This is not root clipping remediation or full phrase/true-peak calibration.
- Appended global_fx_bypass and master are preserved across patch/INIT/undo/redo/AI operations, omitted from presets, and saved separately in project state. Version-1 project output gain migrates into trim with neutral master.
- PANIC and patch changes request audio-thread voice/arp/oversampler/effect reset. GUI MIDI has epochs to discard old queued events. Audible audition FIFOs and timers are removed; legacy API is inert. Concurrent patch mutation gates output, but a full immutable audio-thread patch transaction and all MIDI ordering edge cases still require audit.
- Header MASTER VOL, PANIC, FX BYPASS and direct FM access. Removed LIBRARY/STAGE editor pages; legacy view values map to STUDIO. Existing libraries remain.
- SAVE PRESET is visible only in PRESETS. Duplicate user filenames get a numbered new filename. Write failures leave the current preset metadata unchanged. No sample files copied by this action.
- SETTINGS saves a whitelist of interface preferences and non-secret AI settings atomically. RESET requires confirmation, resets UI and AI to offline defaults, and preserves patch, master, samples, presets, credentials and host audio devices. Master stays project/standalone project state, not global preferences. Cancel performs no change.

Validation pending Windows CI for the exact commit. Tests cover supported quality latency, preset migration, master isolation/recall/mute, hard-stop tails and settings persistence/isolation.

Remaining release blockers: root clipping and full stage instrumentation, all factory musical-phrase/true-peak calibration, complete hard-stop concurrent/MIDI/loop/arp matrix, global bypass transition smoothing, save dialog lifetime/invalid-path/duplicate regression matrix, actual MSFA licensing audit and reference Dexed WAV parity, 500 curated presets, listening and FL Studio acceptance. Existing output soft ceiling is still present and must not be mistaken for proof of clean internal audio. No release or ZIP approval.
