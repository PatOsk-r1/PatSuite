// FL Studio (and other hosts) use the computer keyboard as a MIDI keyboard. If a click inside a
// plugin gives one of its controls the keyboard focus, the host stops receiving those keys until
// the user clicks outside. Every Pat editor calls releaseKeyboard() on itself after building its
// controls so no control ever takes the focus.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pat::ui {

inline void releaseKeyboard(juce::Component& root) {
    root.setWantsKeyboardFocus(false);
    root.setMouseClickGrabsKeyboardFocus(false);
    for (auto* child : root.getChildren()) releaseKeyboard(*child);
}

}  // namespace pat::ui
