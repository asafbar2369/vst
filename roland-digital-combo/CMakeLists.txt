cmake_minimum_required(VERSION 3.22)
project(DigitalCombo VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

# Link the C/C++ runtime statically on Windows (/MT): the plug-in must load on machines that do not
# have the Visual C++ Redistributable installed. Run 6 imported MSVCP140/VCRUNTIME140 (checked with a
# PE import scan of the CI artefact); CI now fails if those imports come back.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DIGITALCOMBO_PORTABLE_TESTS_ONLY "Build C++ regression tests without fetching JUCE or building VST3" OFF)

if(NOT DIGITALCOMBO_PORTABLE_TESTS_ONLY)
# JUCE 9.0.5 (dual licensed AGPLv3 / commercial, see docs/VERIFICATION.md).
# Pinned to the exact commit of the 9.0.5 tag. A local checkout can be used with
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=<path>
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG d60b7a78030661574a615b586d9325ba7a7fa7dd # 9.0.5
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(JUCE)

set(DC_SOURCES
    Source/D50SysEx.cpp
    Source/D50Engine.cpp
    Source/JD800SysEx.cpp
    Source/JdWaves.cpp
    Source/JdEffects.cpp
    Source/JD800Engine.cpp
    Source/Parameters.cpp
    Source/UserWaveLoader.cpp
    Source/PluginProcessor.cpp
    Source/HardwarePanels.cpp
    Source/PerformanceControls.cpp
    Source/CloudUI.cpp
    Source/CloudPages.cpp
    Source/PluginEditor.cpp
)

set(DC_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

juce_add_plugin(DigitalCombo
    COMPANY_NAME "Independent Audio Research"
    BUNDLE_ID "audio.independent.digitalcombo"
    PLUGIN_MANUFACTURER_CODE Iar1
    PLUGIN_CODE Dc5j
    PRODUCT_NAME "Roland Digital Combo"
    DESCRIPTION "Unofficial D-50 / JD-800 style combo synthesizer (not affiliated with Roland)"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3
    VST3_CATEGORIES Instrument Synth
)

target_sources(DigitalCombo PRIVATE ${DC_SOURCES})
target_include_directories(DigitalCombo PRIVATE Source)
target_compile_definitions(DigitalCombo PUBLIC ${DC_DEFINITIONS})
target_link_libraries(DigitalCombo
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---- Automated tests: same sources, console host ----
juce_add_console_app(DigitalComboTests PRODUCT_NAME "DigitalComboTests")
target_sources(DigitalComboTests PRIVATE Tests/TestMain.cpp ${DC_SOURCES})
target_include_directories(DigitalComboTests PRIVATE Source)
target_compile_definitions(DigitalComboTests PRIVATE ${DC_DEFINITIONS} JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(DigitalComboTests
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ---- Host-level test: loads the compiled .vst3 through JUCE's VST3 hosting code ----
juce_add_console_app(DigitalComboHostTest PRODUCT_NAME "DigitalComboHostTest")
target_sources(DigitalComboHostTest PRIVATE Tests/HostSmokeTest.cpp Tests/ScreenCapture.cpp)
target_compile_definitions(DigitalComboHostTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=1
)
target_link_libraries(DigitalComboHostTest
    PRIVATE
        juce::juce_audio_utils
        $<$<PLATFORM_ID:Windows>:gdi32>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

endif() # DIGITALCOMBO_PORTABLE_TESTS_ONLY

# Lightweight checks that compile with no JUCE or audio drivers (also runnable
# on Linux). These do not replace the complete Windows VST3/host test suite.
add_executable(DigitalComboBankCoverageTest Tests/ParamAreaCoverageTest.cpp)
target_include_directories(DigitalComboBankCoverageTest PRIVATE Tests/MinimalJuce Source)
add_executable(DigitalComboSampleWaveTest Tests/SampleWaveTest.cpp)
target_include_directories(DigitalComboSampleWaveTest PRIVATE Source)
add_executable(DigitalComboMidiOwnershipTest Tests/MidiNoteOwnershipTest.cpp)
target_include_directories(DigitalComboMidiOwnershipTest PRIVATE Source)
add_executable(DigitalComboEngineResourcesTest
    Tests/EngineResourceTest.cpp
    Source/D50Engine.cpp Source/D50SysEx.cpp
    Source/JD800Engine.cpp Source/JD800SysEx.cpp
    Source/JdWaves.cpp Source/JdEffects.cpp)
target_include_directories(DigitalComboEngineResourcesTest PRIVATE Source)
enable_testing()
add_test(NAME digitalcombo_midi_ownership COMMAND DigitalComboMidiOwnershipTest)
add_test(NAME digitalcombo_sample_waves COMMAND DigitalComboSampleWaveTest)
add_test(NAME digitalcombo_bank_coverage COMMAND DigitalComboBankCoverageTest)
add_test(NAME digitalcombo_engine_resources COMMAND DigitalComboEngineResourcesTest)
