cmake_minimum_required(VERSION 3.22)
project(RMX16_Reconstruction VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(RMX16Assets
    SOURCES
        Resources/user_reference_faceplate.png
)

juce_add_plugin(RMX16_Reconstruction
    COMPANY_NAME "Independent Audio Research"
    BUNDLE_ID "audio.independent.rmx16research"
    PLUGIN_MANUFACTURER_CODE InAR
    PLUGIN_CODE Rm16
    PRODUCT_NAME "RMX16 Research Build"
    DESCRIPTION "Independent research reconstruction inspired by the AMS RMX16"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx Reverb
)

juce_generate_juce_header(RMX16_Reconstruction)

target_sources(RMX16_Reconstruction PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(RMX16_Reconstruction PRIVATE Source)
target_compile_features(RMX16_Reconstruction PRIVATE cxx_std_17)
target_compile_definitions(RMX16_Reconstruction PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(RMX16_Reconstruction PRIVATE
    RMX16Assets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
