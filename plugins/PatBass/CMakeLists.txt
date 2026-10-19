# PatBass: synth for every kind of bass.
juce_add_plugin(PatBass
    PRODUCT_NAME "PatBass"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.patbass"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Pbas
    FORMATS ${PAT_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD ${PATSUITE_COPY_VST3}
    VST3_COPY_DIR "${PATSUITE_VST3_DIR}")

target_sources(PatBass PRIVATE
    Processor.cpp
    Presets.cpp
    ${CMAKE_SOURCE_DIR}/ui/bass/BassEditor.cpp)
pat_plugin_setup(PatBass)
