# Patan: auto-panner driven by a drawn curve.
juce_add_plugin(Patan
    PRODUCT_NAME "Patan"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.patan"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Ptan
    FORMATS ${PAT_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Spatial
    COPY_PLUGIN_AFTER_BUILD ${PATSUITE_COPY_VST3}
    VST3_COPY_DIR "${PATSUITE_VST3_DIR}")

target_sources(Patan PRIVATE
    Processor.cpp
    Presets.cpp
    ${CMAKE_SOURCE_DIR}/ui/pan/PatanEditor.cpp)
pat_plugin_setup(Patan)
