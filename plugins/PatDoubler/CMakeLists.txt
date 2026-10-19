# PatDoubler: vocal doubler (phase 4).
juce_add_plugin(PatDoubler
    PRODUCT_NAME "PatDoubler"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.doubler"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Pdbl
    FORMATS ${PAT_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Modulation
    COPY_PLUGIN_AFTER_BUILD ${PATSUITE_COPY_VST3}
    VST3_COPY_DIR "${PATSUITE_VST3_DIR}")

target_sources(PatDoubler PRIVATE
    Processor.cpp
    Presets.cpp
    ${CMAKE_SOURCE_DIR}/ui/doubler/DoublerEditor.cpp)
pat_plugin_setup(PatDoubler)
