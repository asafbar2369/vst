# MSFA (Music Synthesizer for Android) FM core — third-party notice

Files in this folder, except `tuning.h`, `tuning.cc` and `libMTSClient.h`, are copied unmodified from
Dexed (https://github.com/asb2m10/dexed, `source/msfa`, commit 1ed5a4d03157c35db098dfa367372c8b3c0ab82e).

- Copyright 2012-2013 Google Inc.; Copyright 2016-2025 Pascal Gauthier; Copyright 2019 Jean Pierre Cimalando.
- Licensed under the Apache License, Version 2.0 (see `LICENSE-APACHE-2.0.txt` and each file's header).

Replaced or added by VICE SYNTH X (original code, not taken from Dexed):
- `tuning.h`, `tuning.cc` — standard 12-TET tuning only (Dexed's own tuning files carry no Apache header and fall
  under Dexed's GPLv3; they are not used).
- `libMTSClient.h` — stub: MTS-ESP microtuning is not used.
- `../debugger/Logger.h` — no-op TRACE macro (replaces Dexed's GPLv3 logger).

Not used: Dexed's GPLv3 code, including the default "Mark I" engine (`source/engine/EngineMkI.*`) and the OPL engine.
VICE SYNTH X uses MSFA's Apache-2.0 `FmCore` ("Modern" engine in Dexed). Using Mark I would require distributing the
whole plug-in under GPLv3; that is an owner decision and has not been made.
