# PatTune: real-time pitch correction with arbitrary tunings (phase 5).
juce_add_plugin(PatTune
    PRODUCT_NAME "PatTune"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.tune"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Ptun
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD ${PATSUITE_COPY_VST3}
    VST3_COPY_DIR "${PATSUITE_VST3_DIR}")

target_sources(PatTune PRIVATE
    Processor.cpp
    Presets.cpp
    ${CMAKE_SOURCE_DIR}/ui/tune/TuneEditor.cpp)
pat_plugin_setup(PatTune)
