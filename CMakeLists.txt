cmake_minimum_required(VERSION 3.22)

project(VST_muni_one VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
    add_compile_definitions(_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)
endif()

# JUCE is expected to be placed as a git submodule at libs/JUCE.
# Initial setup:
#   git init
#   git submodule add https://github.com/juce-framework/JUCE.git libs/JUCE
add_subdirectory(libs/JUCE)

juce_add_plugin(VST_muni_one
    COMPANY_NAME            "muni"
    BUNDLE_ID               "com.muni.vstmunione"
    PLUGIN_MANUFACTURER_CODE Muni
    PLUGIN_CODE              Muni
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "VST_muni_one"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

juce_generate_juce_header(VST_muni_one)

# Embed the dog sample and character images as binary data so the plugin never reads
# from a runtime path (per CLAUDE.md security policy: bundled resources, no dynamic
# file loading).
juce_add_binary_data(VST_muni_oneBinaryData
    SOURCES
        src/sample/dog.wav
        src/img/muni_fbase.png
        src/img/muni_ear.png
        src/img/muni_eye.png
        src/img/muni_eye_brow.png
        src/img/muni_nose.png
)

target_sources(VST_muni_one PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/ParameterIds.h
    src/dsp/SampleBuffer.h
    src/dsp/MuniVoice.h          # MuniSound も同居
    src/gui/MuniFace.cpp
    src/gui/MuniFace.h
    src/gui/HelpPanel.cpp
    src/gui/HelpPanel.h
    src/gui/ParameterOverlay.cpp
    src/gui/ParameterOverlay.h
)

target_include_directories(VST_muni_one PRIVATE src)

target_compile_definitions(VST_muni_one PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(VST_muni_one PRIVATE
    VST_muni_oneBinaryData
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
