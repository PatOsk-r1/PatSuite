# PatDeEsser: visual de-esser (phase 2).
juce_add_plugin(PatDeEsser
    PRODUCT_NAME "PatDeEsser"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.deesser"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Pdes
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD ${PATSUITE_COPY_VST3}
    VST3_COPY_DIR "${PATSUITE_VST3_DIR}")

target_sources(PatDeEsser PRIVATE
    Processor.cpp
    ${CMAKE_SOURCE_DIR}/ui/deesser/DeEsserEditor.cpp)
pat_plugin_setup(PatDeEsser)
