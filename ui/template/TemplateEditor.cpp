#include "ui/template/TemplateEditor.h"

using namespace pat::ui;

TemplateEditor::TemplateEditor(TemplateProcessor& p)
    : PluginFrame(p, p.state, "PatTemplate", "phase 1 test bench", colours::cyan, 900, 560),
      proc_(p),
      spectrum_(settings(), p.analysis, [&p] { return p.sampleRate.load(); }, colours::cyan),
      gain_(p.state, "gain", "GAIN", colours::cyan, settings(), NeonKnob::Size::Big),
      inMeter_(settings(), "IN", colours::cyan),
      outMeter_(settings(), "OUT", colours::cyan) {
    for (juce::Component* c : std::initializer_list<juce::Component*>{&spectrum_, &gain_, &inMeter_, &outMeter_}) content().addAndMakeVisible(c);
    spectrum_.setHighlight(4000.0f, 9000.0f, colours::pink);  // preview of the de-esser band
    clock().add([this](float dt) {
        spectrum_.tick(dt);
        inMeter_.update(proc_.inPeak.load(), dt);
        outMeter_.update(proc_.outPeak.load(), dt);
    });
    finishConstruction();
}

void TemplateEditor::viewChanged(bool pro) {
    inMeter_.setVisible(pro);
    outMeter_.setVisible(pro);
}

void TemplateEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(16);
    if (proView()) {
        auto meters = r.removeFromRight(70);
        inMeter_.setBounds(meters.removeFromLeft(35));
        outMeter_.setBounds(meters);
        r.removeFromRight(10);
        gain_.setBounds(r.removeFromBottom(150).withSizeKeepingCentre(180, 150));
    } else {
        gain_.setBounds(r.removeFromBottom(190).withSizeKeepingCentre(220, 190));
    }
    r.removeFromBottom(10);
    spectrum_.setBounds(r);
}

void TemplateEditor::simulateForSnapshot(float seconds) {
    spectrum_.useTestSignal(true);
    for (float t = 0; t < seconds; t += 1.0f / 30.0f) {
        spectrum_.tick(1.0f / 30.0f);
        inMeter_.update(0.5f, 1.0f / 30.0f);
        outMeter_.update(0.7f, 1.0f / 30.0f);
    }
}
