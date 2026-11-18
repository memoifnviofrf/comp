cmake_minimum_required(VERSION 3.22)

project(VocalCompressor VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE を自動でダウンロード(初回のみ時間がかかります)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VocalCompressor
    COMPANY_NAME "MyCompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Myco
    PLUGIN_CODE Vcmp
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx Dynamics
    PRODUCT_NAME "Vocal Compressor"
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(VocalCompressor PRIVATE
    Source/Biquad.h
    Source/CompressorParameters.h
    Source/CompressorDetector.h   Source/CompressorDetector.cpp
    Source/SidechainProcessor.h   Source/SidechainProcessor.cpp
    Source/Metering.h             Source/Metering.cpp
    Source/CompressorDSP.h        Source/CompressorDSP.cpp
    Source/PresetManager.h        Source/PresetManager.cpp
    Source/PluginProcessor.h      Source/PluginProcessor.cpp
    Source/PluginEditor.h         Source/PluginEditor.cpp
)
target_include_directories(VocalCompressor PRIVATE Source)

target_compile_definitions(VocalCompressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(VocalCompressor
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# --- DSP テスト(任意): cmake -B build -DVC_BUILD_TESTS=ON ---
# 本体と同じ DSP ソースにリンクする、GUI なしのコンソールプログラムです。
option(VC_BUILD_TESTS "Build the headless DSP test program" OFF)
if(VC_BUILD_TESTS)
    add_executable(VocalCompressorTests
        tests/dsp_tests.cpp
        Source/CompressorDetector.cpp Source/SidechainProcessor.cpp Source/Metering.cpp Source/CompressorDSP.cpp
        Source/PresetManager.cpp)
    target_include_directories(VocalCompressorTests PRIVATE Source)
    target_compile_definitions(VocalCompressorTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(VocalCompressorTests PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)
endif()
