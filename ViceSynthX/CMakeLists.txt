cmake_minimum_required(VERSION 3.22)

project(ViceSynthX VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE location
#   Local development without network/GitHub:
#       cmake -S . -B build -DVSX_JUCE_DIR=C:/path/to/JUCE
#   Otherwise JUCE is fetched from the official repository on first configure.
# ---------------------------------------------------------------------------
set(VSX_JUCE_DIR "" CACHE PATH "Path to a local JUCE source tree (optional)")
option(VSX_BUILD_TESTS "Build the full automated test runner" ON)
option(VSX_CORE_ONLY "Build independent engine/AI tests without the unfinished plug-in editor" OFF)
option(VSX_COPY_AFTER_BUILD "Copy the VST3 into the system plug-in folder after building" OFF)

if(VSX_JUCE_DIR)
    if(NOT EXISTS "${VSX_JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "VSX_JUCE_DIR must point to the JUCE source root (containing CMakeLists.txt)")
    endif()
    add_subdirectory("${VSX_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.6
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Source lists
# ---------------------------------------------------------------------------
set(VSX_ENGINE_SOURCES
    Source/Core/Params.cpp
    Source/Core/PatchState.cpp
    Source/Core/Tuning.cpp
    Source/Synth/Wavetable.cpp
    Source/Synth/WavetableFactory.cpp
    Source/Synth/SampleBank.cpp
    Source/Synth/BuiltinSamples.cpp
    Source/Synth/FmVoice.cpp
    Source/Synth/ExtraEngines.cpp
    Source/ThirdParty/msfa/dx7note.cc
    Source/ThirdParty/msfa/env.cc
    Source/ThirdParty/msfa/exp2.cc
    Source/ThirdParty/msfa/fm_core.cc
    Source/ThirdParty/msfa/fm_op_kernel.cc
    Source/ThirdParty/msfa/freqlut.cc
    Source/ThirdParty/msfa/lfo.cc
    Source/ThirdParty/msfa/pitchenv.cc
    Source/ThirdParty/msfa/porta.cpp
    Source/ThirdParty/msfa/sin.cc
    Source/ThirdParty/msfa/tuning.cc
    Source/Synth/Oscillator.cpp
    Source/Synth/Filters.cpp
    Source/Synth/Modulation.cpp
    Source/Synth/Macros.cpp
    Source/Synth/Voice.cpp
    Source/Synth/SynthEngine.cpp
    Source/Synth/SoundLayer.cpp
    Source/Synth/ArpSeq.cpp
    Source/FX/Effects.cpp
    Source/FX/ExtraEffects.cpp
    Source/FX/ReverbEngine.cpp
    Source/FX/EffectsRack.cpp
    Source/Presets/PresetFormat.cpp
    Source/Presets/PresetManager.cpp
    Source/Presets/FactoryPresets.cpp
    Source/Presets/FactoryExpansion.cpp
    Source/Presets/SoundRecipes.cpp
    Source/AI/SecureStorage.cpp
    Source/AI/AIProvider.cpp
    Source/AI/AIController.cpp
    Source/AI/PatchOps.cpp
    Source/AI/OfflineDesigner.cpp
    Source/Analysis/AudioFeatures.cpp
    Source/Analysis/SoundMatcher.cpp
)
set(VSX_PROCESSOR_SOURCES
    Source/PluginProcessor.cpp
)

set(VSX_GUI_SOURCES
    Source/GUI/Theme.cpp
    Source/GUI/Icons.cpp
    Source/GUI/Widgets.cpp
    Source/GUI/MiamiArtwork.cpp
    Source/GUI/WaveDisplay.cpp
    Source/GUI/EnvLfoEditors.cpp
    Source/GUI/FmEditor.cpp
    Source/GUI/OscSourceMenu.cpp
    Source/GUI/SynthPanels.cpp
    Source/GUI/TabPages.cpp
    Source/GUI/VectorPage.cpp
    Source/GUI/LayersPage.cpp
    Source/GUI/WavetableEditor.cpp
    Source/GUI/AIChatPanel.cpp
    Source/GUI/AIDesignerPanel.cpp
    Source/GUI/AISettingsPanel.cpp
    Source/GUI/HeaderHero.cpp
    Source/GUI/BrowserLibrary.cpp
    Source/GUI/StudioView.cpp
    Source/GUI/MacroPerformance.cpp
    Source/GUI/DesignStage.cpp
    Source/PluginEditor.cpp
)

set(VSX_COMMON_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    VSX_VERSION_STRING="${PROJECT_VERSION}")

if(UNIX AND NOT APPLE)
    set(VSX_NEEDS_CURL TRUE)                              # HTTPS for AI providers on Linux
else()
    set(VSX_NEEDS_CURL FALSE)                             # Windows uses WinINet, macOS NSURLSession
endif()

set(VSX_JUCE_MODULES
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_opengl
    juce::juce_cryptography)

# ---------------------------------------------------------------------------
# Plug-in (VST3 + Standalone)
# ---------------------------------------------------------------------------
if(VSX_CORE_ONLY)
    enable_testing()
    juce_add_console_app(ViceSynthXCoreTests PRODUCT_NAME "ViceSynthXCoreTests" NEEDS_CURL ${VSX_NEEDS_CURL})
    target_sources(ViceSynthXCoreTests PRIVATE
        ${VSX_ENGINE_SOURCES}
        Tests/TestMain.cpp
        Tests/TestAI.cpp
        Tests/TestAIProviders.cpp
        Tests/TestAudioAcceptance.cpp
        Tests/TestCoreDSP.cpp)
    target_include_directories(ViceSynthXCoreTests PRIVATE Source Tests)
    target_compile_definitions(ViceSynthXCoreTests PRIVATE ${VSX_COMMON_DEFINITIONS} VSX_TESTING=1)
    target_link_libraries(ViceSynthXCoreTests PRIVATE
        ${VSX_JUCE_MODULES}
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    if(MSVC)
        target_compile_options(ViceSynthXCoreTests PRIVATE /bigobj /utf-8)
        target_link_libraries(ViceSynthXCoreTests PRIVATE crypt32)
    endif()
    add_test(NAME vsx-core COMMAND ViceSynthXCoreTests)
    return()
endif()

juce_add_plugin(ViceSynthX
    COMPANY_NAME "Vice Synth Labs"
    COMPANY_WEBSITE "https://vicesynthlabs.invalid"
    BUNDLE_ID "com.vicesynthlabs.vicesynthx"
    PLUGIN_MANUFACTURER_CODE Vslb
    PLUGIN_CODE Vsx1
    PRODUCT_NAME "VICE SYNTH X"
    DESCRIPTION "Hybrid wavetable / analog-modelled synthesizer with AI sound design"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${VSX_COPY_AFTER_BUILD}
    FORMATS VST3 Standalone
    VST3_CATEGORIES Instrument Synth
    MICROPHONE_PERMISSION_ENABLED FALSE
    NEEDS_CURL ${VSX_NEEDS_CURL})

target_sources(ViceSynthX PRIVATE ${VSX_ENGINE_SOURCES} ${VSX_PROCESSOR_SOURCES} ${VSX_GUI_SOURCES})
target_include_directories(ViceSynthX PRIVATE Source)
target_compile_definitions(ViceSynthX PUBLIC ${VSX_COMMON_DEFINITIONS})
target_link_libraries(ViceSynthX
    PRIVATE
        ${VSX_JUCE_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(ViceSynthX PRIVATE /bigobj /utf-8)
    target_link_libraries(ViceSynthX PRIVATE crypt32)
endif()

# ---------------------------------------------------------------------------
# Headless automated test runner (engine + processor + editor)
# ---------------------------------------------------------------------------
if(VSX_BUILD_TESTS)
    juce_add_console_app(ViceSynthXTests PRODUCT_NAME "ViceSynthXTests" NEEDS_CURL ${VSX_NEEDS_CURL})
    target_sources(ViceSynthXTests PRIVATE
        ${VSX_ENGINE_SOURCES}
        ${VSX_PROCESSOR_SOURCES}
        ${VSX_GUI_SOURCES}
        Tests/TestMain.cpp
        Tests/TestAI.cpp
        Tests/TestAIProviders.cpp
        Tests/TestAudioAcceptance.cpp
        Tests/TestCoreDSP.cpp
        Tests/TestMeasurements.cpp
        Tests/TestReverb.cpp
        Tests/TestFm.cpp
        Tests/TestFilterQuality.cpp
        Tests/TestFxLaws.cpp
        Tests/TestSessionBehaviour.cpp
        Tests/TestClipAB.cpp
        Tests/TestDexedEquivalence.cpp
        Tests/TestExtraEngines.cpp
        Tests/TestTuning.cpp
        Tests/TestExtraEffects.cpp
        Tests/TestLayers.cpp
        Tests/TestFilterFm.cpp
        Tests/TestArpRatchet.cpp
        Tests/TestFilterAB.cpp
        Tests/LegacyFilters.cpp
        Tests/TestSamples.cpp
        Tests/TestBenchmark.cpp
        Tests/TestLiveProviders.cpp
        Tests/TestGui.cpp)
    target_include_directories(ViceSynthXTests PRIVATE Source Tests)
    target_compile_definitions(ViceSynthXTests PRIVATE ${VSX_COMMON_DEFINITIONS} VSX_TESTING=1)
    target_link_libraries(ViceSynthXTests
        PRIVATE
            ${VSX_JUCE_MODULES}
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    if(MSVC)
        target_compile_options(ViceSynthXTests PRIVATE /bigobj /utf-8)
        target_link_libraries(ViceSynthXTests PRIVATE crypt32)
    endif()
endif()
