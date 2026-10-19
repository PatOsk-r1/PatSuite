# PatMaster: master limiter with a loudness (LUFS) meter.
juce_add_plugin(PatMaster
    PRODUCT_NAME "PatMaster"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.patmaster"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Pmst
    FORMATS ${PAT_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics Mastering
    COPY_PLUGIN_AFTER_BUILD ${PATSUITE_COPY_VST3}
    VST3_COPY_DIR "${PATSUITE_VST3_DIR}")

target_sources(PatMaster PRIVATE
    Processor.cpp
    Presets.cpp
    ${CMAKE_SOURCE_DIR}/ui/master/MasterEditor.cpp)
pat_plugin_setup(PatMaster)
