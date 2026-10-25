cmake_minimum_required(VERSION 3.22)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
endif()
option(MG2_COPY_AFTER_BUILD "Plugin nach dem Build in den VST3-Ordner kopieren" ON)
project(MaestroG2 VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MaestroG2
    COMPANY_NAME "Private"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Pvt1
    PLUGIN_CODE Mg2x
    FORMATS VST3 Standalone
    PRODUCT_NAME "Maestro G-2 Rhythm N Sound"
    COPY_PLUGIN_AFTER_BUILD ${MG2_COPY_AFTER_BUILD})

target_sources(MaestroG2 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/G2Dsp.h)

target_compile_features(MaestroG2 PRIVATE cxx_std_17)
target_compile_definitions(MaestroG2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MaestroG2
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
