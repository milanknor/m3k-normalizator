cmake_minimum_required(VERSION 3.22)
project(M3KNormalizator VERSION 0.2.5)

set(CMAKE_CXX_STANDARD 17)

# Statically link MSVC runtime so the plugin has no external DLL dependencies
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(JUCE)

juce_add_plugin(M3KNormalizator
    COMPANY_NAME "MilanAudio"
    PLUGIN_MANUFACTURER_CODE Mlan
    PLUGIN_CODE M3kn
    FORMATS VST3 Standalone
    PRODUCT_NAME "M3K Normalizator"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(M3KNormalizator)

target_sources(M3KNormalizator PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(M3KNormalizator PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(M3KNormalizator PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
