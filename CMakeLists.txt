# PatSuite: shared code (common/), audio engines (dsp/), editors (ui/) and one target per plugin
# (plugins/<Name>/). Every plugin is VST3 + Standalone.
cmake_minimum_required(VERSION 3.25)
project(PatSuite VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PATSUITE_BUILD_PLUGINS "Build the plugins" ON)
option(PATSUITE_BUILD_TESTS "Build the unit tests" ON)
option(PATSUITE_COPY_VST3 "Copy each VST3 to PATSUITE_VST3_DIR after building" OFF)
set(PATSUITE_VST3_DIR "C:/Program Files/Common Files/VST3" CACHE PATH "VST3 folder FL Studio scans")

set(PAT_THIRD_PARTY ${CMAKE_SOURCE_DIR}/third_party)
if(NOT EXISTS ${PAT_THIRD_PARTY}/JUCE/CMakeLists.txt)
    message(FATAL_ERROR "Dependencies missing. Run: powershell -ExecutionPolicy Bypass -File tools/fetch-deps.ps1")
endif()

add_library(pat_warnings INTERFACE)
if(MSVC)
    target_compile_options(pat_warnings INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(pat_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------------------------
# common/core: pure C++ (no JUCE), unit-tested — lock-free FIFO, performance rule, tunings, FFT
add_library(pat_core STATIC
    common/core/Fft.cpp
    common/core/PerfTier.cpp
    common/core/TuningTable.cpp
)
target_include_directories(pat_core PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(pat_core PRIVATE pat_warnings)
if(MSVC)
    target_compile_options(pat_core PRIVATE $<$<NOT:$<CONFIG:Debug>>:/O2 /fp:fast>)
endif()

# dsp: the audio engines, pure C++ (no JUCE), unit-tested
add_library(pat_dsp STATIC
    dsp/deesser/DeEsserEngine.cpp
    dsp/sidechain/SidechainEngine.cpp
    dsp/doubler/DoublerEngine.cpp
    dsp/tune/Scales.cpp
    dsp/tune/PitchDetector.cpp
    dsp/tune/PsolaShifter.cpp
    dsp/tune/TuneEngine.cpp
)
target_link_libraries(pat_dsp PUBLIC pat_core PRIVATE pat_warnings)
if(MSVC)
    target_compile_options(pat_dsp PRIVATE $<$<NOT:$<CONFIG:Debug>>:/O2 /fp:fast>)
endif()

# ---------------------------------------------------------------------------------------------
if(PATSUITE_BUILD_PLUGINS)
    add_subdirectory(${PAT_THIRD_PARTY}/JUCE ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)

    # common/ui: the shared look and the visual infrastructure (JUCE). Compiled into each plugin
    # as an INTERFACE source list so every plugin gets its own copy with its own JucePlugin_*.
    add_library(pat_ui INTERFACE)
    target_sources(pat_ui INTERFACE
        ${CMAKE_SOURCE_DIR}/common/ui/Theme.cpp
        ${CMAKE_SOURCE_DIR}/common/ui/PerfSettings.cpp
        ${CMAKE_SOURCE_DIR}/common/ui/FrameClock.cpp
        ${CMAKE_SOURCE_DIR}/common/ui/LayeredCanvas.cpp
        ${CMAKE_SOURCE_DIR}/common/ui/NeonKnob.cpp
        ${CMAKE_SOURCE_DIR}/common/ui/LevelMeter.cpp
        ${CMAKE_SOURCE_DIR}/common/ui/SpectrumView.cpp
        ${CMAKE_SOURCE_DIR}/common/ui/PluginFrame.cpp
        ${CMAKE_SOURCE_DIR}/common/ui/SegmentedChoice.cpp
    )
    if(WIN32)
        target_link_libraries(pat_ui INTERFACE dxgi)  # GPU memory query for the performance level
    endif()

    # Common settings for every plugin target.
    function(pat_plugin_setup target)
        target_compile_definitions(${target} PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0)
        target_link_libraries(${target}
            PRIVATE pat_core pat_dsp pat_ui pat_warnings juce::juce_audio_utils juce::juce_dsp
            PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
    endfunction()

    add_subdirectory(plugins/PatTemplate)
    add_subdirectory(plugins/PatDeEsser)
    add_subdirectory(plugins/PatSidechain)
    add_subdirectory(plugins/PatDoubler)
    add_subdirectory(plugins/PatTune)
endif()

# ---------------------------------------------------------------------------------------------
if(PATSUITE_BUILD_TESTS)
    enable_testing()
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(${PAT_THIRD_PARTY}/googletest ${CMAKE_BINARY_DIR}/googletest EXCLUDE_FROM_ALL)
    add_executable(pat_tests tests/CoreTests.cpp tests/DeEsserTests.cpp tests/SidechainTests.cpp tests/DoublerTests.cpp tests/TuneTests.cpp)
    target_link_libraries(pat_tests PRIVATE pat_core pat_dsp pat_warnings GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(pat_tests DISCOVERY_MODE PRE_TEST)
endif()
