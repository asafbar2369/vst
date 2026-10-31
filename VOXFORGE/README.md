# VOXFORGE — Windows VST3 build

This folder contains the VOXFORGE JUCE source archive and an automated Windows x64 build workflow.

## Build
Push to `voxforge-installer` or run **Actions → VOXFORGE Windows VST3 + Installer → Run workflow**.
The workflow restores the source archive, builds the VST3 with Visual Studio 2022, creates `VOXFORGE_Setup.exe` with Inno Setup, verifies the output, and uploads a 30-day artifact.

## Installer target
The installer places the VST3 bundle in the system-wide 64-bit VST3 folder:
`C:\Program Files\Common Files\VST3\VOXFORGE.vst3`

## Prototype status
The source is an early starter implementation. A successful CI build proves compilation and packaging only; it does not replace listening tests, DAW validation, parameter automation checks, preset persistence testing, or a full QA pass. The installer is not code-signed.
