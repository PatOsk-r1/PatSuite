# PatSquash: three-band upward + downward compressor.
juce_add_plugin(PatSquash
    PRODUCT_NAME "PatSquash"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.patsquash"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Psqs
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD ${PATSUITE_COPY_VST3}
    VST3_COPY_DIR "${PATSUITE_VST3_DIR}")

target_sources(PatSquash PRIVATE
    Processor.cpp
    Presets.cpp
    ${CMAKE_SOURCE_DIR}/ui/squash/SquashEditor.cpp)
pat_plugin_setup(PatSquash)
