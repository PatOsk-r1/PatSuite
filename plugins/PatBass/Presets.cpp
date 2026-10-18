// Factory presets of PatBass, grouped by genre. Each preset starts from a sound family (808,
// reese, wobble, growl, acid, pluck, sub...) and changes what makes it different. Values in real
// units; anything left out keeps its default. Levels are evened out with the "volume" values in
// kLevels (measured with a held note, see the snapshot diagnostics).
#include "plugins/PatBass/Processor.h"

#include <map>

namespace {
struct P {
    pat::ui::FactoryPreset f;
    P(const char* name, const char* category) : f{juce::String::fromUTF8(name), juce::String::fromUTF8(category), {}} {}
    P& set(const char* id, float v) {
        for (auto& [k, val] : f.values)
            if (k == id) {
                val = v;
                return *this;
            }
        f.values.push_back({id, v});
        return *this;
    }
    // Shapes: 0 sine, 0.33 triangle, 0.66 saw, 1 square.
    P& osc(float shape, int unison = 1, float detune = 0.2f) { return set("a_shape", shape).set("unison", static_cast<float>(unison)).set("detune", detune); }
    P& oscB(float shape, int oct, int semi = 0, float fine = 0, float level = 0.6f) {
        return set("b_on", 1).set("b_shape", shape).set("b_oct", static_cast<float>(oct)).set("b_semi", static_cast<float>(semi)).set("b_fine", fine).set("b_level", level);
    }
    P& sub(float level, int octIndex = 0, int shape = 0) {
        return set("sub_on", level > 0 ? 1.0f : 0.0f).set("sub_level", level).set("sub_oct", static_cast<float>(octIndex)).set("sub_shape", static_cast<float>(shape));
    }
    P& filter(int type, float cutoff, float reso, float env = 0.3f, float decay = 0.3f, float sustain = 0.0f) {
        return set("f_type", static_cast<float>(type)).set("cutoff", cutoff).set("reso", reso).set("f_env", env).set("fe_d", decay).set("fe_s", sustain);
    }
    P& amp(float a, float d, float s, float r) { return set("ae_a", a).set("ae_d", d).set("ae_s", s).set("ae_r", r); }
    P& punch(float semis, float ms, float click = 0.0f) { return set("punch", semis).set("punch_time", ms).set("click", click); }
    P& glide(float ms) { return set("glide", ms); }
    P& wobble(int div, float cutAmt, int shape = 0, float vowelAmt = 0.0f, float shapeAmt = 0.0f) {
        return set("lfo_div", static_cast<float>(div)).set("lfo_cut", cutAmt).set("lfo_shape", static_cast<float>(shape)).set("lfo_vowel", vowelAmt).set("lfo_shapeamt", shapeAmt);
    }
    P& dist(int type, float drive, float mix = 1.0f) { return set("dist_type", static_cast<float>(type)).set("drive", drive).set("dist_mix", mix); }
    P& fm(float amount, int ratio) { return set("a_fm", amount).set("a_fmratio", static_cast<float>(ratio)); }
    P& glue(float g) { return set("glue", g); }
    P& width(float w) { return set("width", w); }
    P& poly() { return set("mono", 0); }
};

// Filter types: 0 LP24, 1 LP12, 2 band, 3 HP, 4 vowel. Dist: 0 soft, 1 hard, 2 fold, 3 crush, 4 tube.
// LFO divisions: 0 1/1, 1 1/2, 2 1/4, 3 1/8, 4 1/16, 5 1/32, 6 1/2T, 7 1/4T, 8 1/8T, 9 1/16T, 10 1/4D, 11 1/8D.
// LFO shapes: 0 sine, 1 triangle, 2 saw, 3 square, 4 random. FM ratios: 0 0.5, 1 1, 2 1.5, 3 2, 4 3, 5 4, 6 5, 7 7.

P b808(const char* name, const char* cat, float punch, float decay, float drive, int distType, float glideMs) {
    P p(name, cat);
    p.osc(0.0f).set("a_level", 0.9f).sub(0.0f).filter(1, 20000, 0.0f, 0.0f).amp(0.001f, decay, 0.0f, 0.25f).punch(punch, 45, 0.15f).glide(glideMs);
    p.dist(distType, drive).set("clean_lows", 0).set("width", 0.0f);
    return p;
}

P reese(const char* name, const char* cat, float detune, float cutoff, float drive, float lfoCut = 0.0f, int div = 2) {
    P p(name, cat);
    p.osc(0.66f, 3, detune).oscB(0.66f, 0, 0, 12.0f, 0.7f).sub(0.55f).filter(0, cutoff, 0.25f, 0.1f, 0.4f, 0.6f).amp(0.003f, 1.0f, 1.0f, 0.15f);
    p.dist(0, drive).glue(0.3f).width(0.8f);
    if (lfoCut != 0.0f) p.wobble(div, lfoCut, 0);
    return p;
}

P wobble(const char* name, const char* cat, int div, float amount, int ftype, float cutoff, float drive, int shape = 0) {
    P p(name, cat);
    p.osc(0.66f, 2, 0.12f).sub(0.6f).filter(ftype, cutoff, 0.45f, 0.0f, 0.3f, 0.0f).amp(0.002f, 1.0f, 1.0f, 0.1f).wobble(div, amount, shape);
    p.dist(0, drive).glue(0.45f).width(0.6f);
    if (ftype == 4) p.set("vowel", 0.2f).set("lfo_vowel", amount * 0.8f);
    return p;
}

P growl(const char* name, const char* cat, float fmAmt, int ratio, int div, float vowelAmt, float drive, int distType = 0) {
    P p(name, cat);
    p.osc(0.66f, 2, 0.1f).fm(fmAmt, ratio).sub(0.55f).filter(4, 900, 0.5f, 0.0f).set("vowel", 0.3f).amp(0.002f, 1.0f, 1.0f, 0.1f);
    p.wobble(div, 0.3f, 1, vowelAmt, 0.3f).dist(distType, drive).glue(0.5f).width(0.7f);
    return p;
}

P acid(const char* name, const char* cat, float shape, float cutoff, float reso, float env, float decay, float glideMs) {
    P p(name, cat);
    p.osc(shape).sub(0.25f).filter(0, cutoff, reso, env, decay, 0.0f).amp(0.001f, 0.4f, 0.7f, 0.08f).glide(glideMs).set("f_drive", 0.4f);
    p.dist(4, 0.25f, 0.6f).set("keytrack", 0.5f).width(0.2f);
    return p;
}

P pluck(const char* name, const char* cat, float shape, float cutoff, float env, float decay, float subLevel) {
    P p(name, cat);
    p.osc(shape, 1).sub(subLevel).filter(0, cutoff, 0.2f, env, decay, 0.0f).amp(0.001f, decay * 2.0f, 0.0f, 0.12f).set("mono", 1).set("legato", 0);
    return p;
}

P subBass(const char* name, const char* cat, int shape, float drive, float attack, float glideMs) {
    P p(name, cat);
    p.osc(0.0f).set("a_level", 0.0f).sub(0.9f, 0, shape).amp(attack, 1.0f, 1.0f, 0.1f).glide(glideMs).dist(4, drive, 0.5f).set("clean_lows", 0).width(0.0f);
    return p;
}

std::vector<P> build() {
    std::vector<P> v;
    v.emplace_back("Init", "");
    // ---------------------------------------------------------------- Trap / 808
    v.push_back(b808("808 Classic Boom", "Trap", 12, 2.5f, 0.15f, 0, 0));
    v.push_back(b808("808 Long Tail", "Trap", 12, 4.0f, 0.1f, 0, 0));
    v.push_back(b808("808 Distorted", "Trap", 14, 2.5f, 0.6f, 0, 0));
    v.push_back(b808("808 Hard Clip", "Trap", 12, 2.0f, 0.7f, 1, 0));
    v.push_back(b808("808 Glide Slide", "Trap", 10, 3.0f, 0.3f, 0, 140));
    v.push_back(b808("808 Fast Glide", "Trap", 10, 2.5f, 0.3f, 0, 60));
    v.push_back(b808("808 Punchy Short", "Trap", 18, 0.8f, 0.25f, 0, 0));
    v.push_back(b808("808 Tube Warm", "Trap", 12, 2.5f, 0.45f, 4, 0));
    v.push_back(b808("808 Big Knock", "Trap", 24, 1.5f, 0.35f, 4, 0).punch(24, 30, 0.6f));
    v.push_back(b808("808 Clean Sine", "Trap", 7, 3.0f, 0.0f, 0, 0));
    v.push_back(b808("808 Fold Grit", "Trap", 12, 2.5f, 0.4f, 2, 0));
    v.push_back(b808("808 Crushed", "Trap", 12, 2.0f, 0.35f, 3, 0));
    v.push_back(b808("808 Rage Distortion", "Trap", 16, 3.0f, 0.9f, 1, 90).glue(0.4f));
    v.push_back(b808("808 Triangle Body", "Trap", 12, 2.5f, 0.3f, 0, 0).osc(0.33f));
    v.push_back(b808("808 Square Bite", "Trap", 12, 2.0f, 0.3f, 0, 0).osc(0.15f));
    v.push_back(b808("808 Mumble Slide", "Trap", 8, 3.5f, 0.25f, 4, 220));
    v.push_back(b808("808 Bouncy", "Trap", 20, 1.0f, 0.3f, 0, 0).amp(0.001f, 1.0f, 0.2f, 0.2f));
    v.push_back(b808("808 Sub Only", "Trap", 6, 3.0f, 0.0f, 0, 0).width(0.0f));
    v.push_back(b808("808 Plug Pluck", "Trap", 14, 0.5f, 0.2f, 0, 0));
    v.push_back(b808("808 Wide Top", "Trap", 12, 2.5f, 0.5f, 0, 0).set("clean_lows", 1).set("width", 0.8f));
    v.push_back(b808("808 Octave Slides", "Trap", 12, 3.0f, 0.35f, 0, 180).oscB(0.0f, 1, 0, 0, 0.25f));
    v.push_back(b808("808 Muffled", "Trap", 10, 2.5f, 0.2f, 0, 0).filter(1, 300, 0.0f, 0.0f));
    v.push_back(b808("808 Detuned Wide", "Trap", 12, 2.5f, 0.4f, 0, 0).osc(0.15f, 3, 0.2f).set("clean_lows", 1).width(0.9f));
    v.push_back(b808("808 Click Heavy", "Trap", 16, 2.0f, 0.4f, 0, 0).punch(16, 30, 0.9f));
    v.push_back(b808("808 Bright Harmonics", "Trap", 12, 2.5f, 0.5f, 2, 0).fm(0.25f, 3));
    v.push_back(b808("808 Vintage Machine", "Trap", 9, 2.0f, 0.25f, 4, 0).osc(0.1f));
    // ---------------------------------------------------------------- Drill
    v.push_back(b808("Drill Slide 808", "Drill", 10, 2.0f, 0.4f, 0, 180));
    v.push_back(b808("Drill Glide Long", "Drill", 10, 3.0f, 0.35f, 4, 260));
    v.push_back(b808("Drill Gritty Slide", "Drill", 12, 2.0f, 0.75f, 1, 160));
    v.push_back(b808("UK Drill Bounce", "Drill", 14, 1.2f, 0.4f, 0, 120));
    v.push_back(b808("NY Drill Heavy", "Drill", 12, 2.5f, 0.6f, 4, 200).glue(0.5f));
    v.push_back(b808("Drill Crispy", "Drill", 16, 1.5f, 0.5f, 2, 140));
    v.push_back(b808("Drill Dark Slide", "Drill", 9, 3.0f, 0.3f, 0, 300).filter(1, 600, 0.0f, 0.0f));
    v.push_back(b808("Drill Short Stab", "Drill", 16, 0.6f, 0.45f, 0, 100));
    v.push_back(b808("Drill Octave Slide", "Drill", 12, 2.0f, 0.4f, 0, 220).oscB(0.0f, 1, 0, 0, 0.3f));
    v.push_back(b808("Drill Click Knock", "Drill", 20, 1.8f, 0.5f, 1, 150).punch(20, 35, 0.7f));
    // ---------------------------------------------------------------- Dubstep
    v.push_back(wobble("Wobble 1/8 Classic", "Dubstep", 3, 0.7f, 0, 300, 0.4f));
    v.push_back(wobble("Wobble 1/4 Slow", "Dubstep", 2, 0.7f, 0, 300, 0.4f));
    v.push_back(wobble("Wobble 1/16 Fast", "Dubstep", 4, 0.6f, 0, 350, 0.45f));
    v.push_back(wobble("Wobble Triplets", "Dubstep", 8, 0.7f, 0, 300, 0.45f));
    v.push_back(wobble("Yoi Talker", "Dubstep", 3, 0.7f, 4, 800, 0.4f));
    v.push_back(wobble("Yoi Slow Talk", "Dubstep", 2, 0.8f, 4, 700, 0.35f));
    v.push_back(wobble("Square Wobble", "Dubstep", 3, 0.7f, 0, 280, 0.5f, 3));
    v.push_back(wobble("Saw LFO Ramp", "Dubstep", 2, 0.8f, 0, 250, 0.5f, 2));
    v.push_back(wobble("Random Chop", "Dubstep", 4, 0.8f, 0, 300, 0.5f, 4));
    v.push_back(growl("Growl FM 2", "Dubstep", 0.6f, 3, 3, 0.7f, 0.5f));
    v.push_back(growl("Growl FM 3 Hard", "Dubstep", 0.8f, 4, 3, 0.8f, 0.6f, 1));
    v.push_back(growl("Growl Metal 7", "Dubstep", 0.7f, 7, 4, 0.6f, 0.5f, 2));
    v.push_back(growl("Riddim Chop", "Dubstep", 0.4f, 3, 4, 0.9f, 0.7f, 1));
    v.push_back(growl("Brostep Screech", "Dubstep", 0.9f, 6, 8, 0.8f, 0.7f, 2));
    v.push_back(wobble("Tearout Square", "Dubstep", 4, 0.9f, 0, 400, 0.8f, 3).dist(1, 0.8f));
    // ---------------------------------------------------------------- DnB
    v.push_back(reese("Reese Classic", "DnB", 0.35f, 900, 0.3f));
    v.push_back(reese("Reese Dark", "DnB", 0.3f, 500, 0.25f));
    v.push_back(reese("Reese Moving", "DnB", 0.4f, 700, 0.35f, 0.3f, 1));
    v.push_back(reese("Reese Wide Hoover", "DnB", 0.6f, 1500, 0.4f).width(1.0f));
    v.push_back(reese("Neuro Reese", "DnB", 0.35f, 800, 0.6f, 0.4f, 3).set("f_type", 4).set("lfo_vowel", 0.5f));
    v.push_back(growl("Neuro Talk", "DnB", 0.5f, 3, 3, 0.7f, 0.5f));
    v.push_back(subBass("DnB Sub Sine", "DnB", 0, 0.1f, 0.003f, 0));
    v.push_back(subBass("Liquid Sub", "DnB", 0, 0.05f, 0.01f, 40));
    v.push_back(reese("Jump Up Wub", "DnB", 0.3f, 600, 0.5f, 0.6f, 2));
    v.push_back(pluck("DnB Pluck Bass", "DnB", 0.66f, 300, 0.7f, 0.2f, 0.5f));
    v.push_back(reese("Reese Crushed", "DnB", 0.35f, 900, 0.5f).dist(3, 0.4f, 0.5f));
    v.push_back(reese("Jungle Reese", "DnB", 0.25f, 1100, 0.2f).set("unison", 2));
    // ---------------------------------------------------------------- House
    v.push_back(pluck("House Pluck Bass", "House", 0.66f, 350, 0.6f, 0.15f, 0.45f));
    v.push_back(pluck("Deep House Round", "House", 0.33f, 500, 0.3f, 0.3f, 0.5f));
    v.push_back(pluck("Organ Bass", "House", 0.85f, 800, 0.4f, 0.2f, 0.4f).oscB(0.33f, 1, 0, 0, 0.35f));
    v.push_back(pluck("Tech House Rumble", "House", 0.66f, 250, 0.5f, 0.25f, 0.55f).dist(4, 0.3f));
    v.push_back(pluck("Bass House Wub", "House", 0.66f, 400, 0.7f, 0.15f, 0.5f).dist(1, 0.6f).glue(0.5f));
    v.push_back(pluck("Garage Bounce", "House", 0.0f, 600, 0.4f, 0.2f, 0.4f).fm(0.3f, 3));
    v.push_back(pluck("Disco Octaves", "House", 0.66f, 700, 0.5f, 0.15f, 0.3f).oscB(0.66f, 1, 0, 0, 0.4f));
    v.push_back(pluck("Rubber House", "House", 0.2f, 450, 0.6f, 0.2f, 0.45f).set("reso", 0.5f));
    v.push_back(pluck("Funky House Saw", "House", 0.66f, 550, 0.5f, 0.18f, 0.35f).glide(40));
    v.push_back(pluck("Speed Garage Wob", "House", 0.66f, 400, 0.4f, 0.3f, 0.5f).wobble(3, 0.4f));
    // ---------------------------------------------------------------- Techno
    v.push_back(acid("Acid 303 Squelch", "Techno", 0.66f, 400, 0.75f, 0.8f, 0.2f, 60));
    v.push_back(acid("Acid Square Line", "Techno", 1.0f, 350, 0.7f, 0.7f, 0.25f, 50));
    v.push_back(acid("Acid Screamer", "Techno", 0.66f, 500, 0.9f, 0.9f, 0.15f, 70).dist(1, 0.4f));
    v.push_back(pluck("Techno Rumble", "Techno", 0.0f, 150, 0.4f, 0.4f, 0.6f).dist(4, 0.5f));
    v.push_back(pluck("Techno Stab Bass", "Techno", 0.66f, 300, 0.6f, 0.08f, 0.4f).dist(0, 0.3f));
    v.push_back(subBass("Industrial Sub", "Techno", 1, 0.6f, 0.002f, 0));
    v.push_back(pluck("Minimal Click Bass", "Techno", 0.33f, 400, 0.5f, 0.1f, 0.4f).punch(5, 15, 0.4f));
    v.push_back(pluck("Hard Techno Kick Bass", "Techno", 0.0f, 300, 0.4f, 0.2f, 0.6f).punch(24, 25, 0.5f).dist(1, 0.6f));
    v.push_back(acid("Acid Slow Burn", "Techno", 0.66f, 250, 0.65f, 0.6f, 0.5f, 90));
    v.push_back(acid("Acid Triangle Bubble", "Techno", 0.33f, 600, 0.8f, 0.6f, 0.2f, 40));
    // ---------------------------------------------------------------- Reggaeton / Latin
    v.push_back(b808("Reggaeton 808", "Reggaeton", 12, 1.2f, 0.3f, 0, 0));
    v.push_back(b808("Dembow Bounce", "Reggaeton", 16, 0.8f, 0.35f, 4, 0));
    v.push_back(b808("Perreo Sub", "Reggaeton", 8, 1.5f, 0.2f, 0, 0).width(0.0f));
    v.push_back(pluck("Moombahton Pluck", "Reggaeton", 0.66f, 400, 0.6f, 0.2f, 0.5f).dist(0, 0.3f));
    v.push_back(b808("Latin Trap Slide", "Reggaeton", 10, 2.5f, 0.4f, 0, 150));
    v.push_back(pluck("Cumbia Bajo", "Reggaeton", 0.33f, 700, 0.4f, 0.25f, 0.4f).oscB(0.0f, 0, 7, 0, 0.2f));
    v.push_back(pluck("Bachata Round", "Reggaeton", 0.2f, 900, 0.3f, 0.35f, 0.3f));
    v.push_back(b808("Dancehall Boom", "Reggaeton", 14, 1.0f, 0.3f, 4, 0));
    // ---------------------------------------------------------------- Hip-Hop / Rap
    v.push_back(subBass("Boom Bap Sub", "Hip-Hop", 0, 0.2f, 0.003f, 0));
    v.push_back(pluck("Boom Bap Upright", "Hip-Hop", 0.33f, 600, 0.4f, 0.3f, 0.3f).set("noise", 0.03f));
    v.push_back(b808("West Coast Whistle Low", "Hip-Hop", 4, 3.0f, 0.1f, 0, 80).oscB(0.0f, 2, 0, 0, 0.15f));
    v.push_back(pluck("G-Funk Moog", "Hip-Hop", 0.66f, 500, 0.5f, 0.4f, 0.4f).glide(70).set("reso", 0.35f));
    v.push_back(pluck("Old School Square", "Hip-Hop", 1.0f, 700, 0.3f, 0.35f, 0.35f));
    v.push_back(b808("Southern 808", "Hip-Hop", 12, 3.0f, 0.3f, 4, 0));
    v.push_back(subBass("Jazz Hop Sub", "Hip-Hop", 1, 0.1f, 0.01f, 30));
    v.push_back(pluck("Rap Dusty Bass", "Hip-Hop", 0.33f, 450, 0.3f, 0.35f, 0.4f).dist(3, 0.2f, 0.4f));
    v.push_back(b808("Phonk Cowbell 808", "Hip-Hop", 14, 2.0f, 0.6f, 1, 60).fm(0.3f, 4));
    v.push_back(b808("Memphis Rumble", "Hip-Hop", 10, 3.0f, 0.5f, 3, 0));
    // ---------------------------------------------------------------- Funk / Synthwave
    v.push_back(pluck("Funk Slap Synth", "Funk & Synthwave", 1.0f, 400, 0.8f, 0.12f, 0.3f).set("reso", 0.4f).punch(3, 10, 0.3f));
    v.push_back(pluck("Synthwave Octave Saw", "Funk & Synthwave", 0.66f, 900, 0.4f, 0.2f, 0.2f).oscB(0.66f, 1, 0, 6, 0.4f));
    v.push_back(pluck("Outrun Pulse", "Funk & Synthwave", 0.9f, 1000, 0.3f, 0.2f, 0.25f).set("a_pw", 0.3f));
    v.push_back(pluck("80s Analog Brass Bass", "Funk & Synthwave", 0.66f, 400, 0.7f, 0.4f, 0.3f).osc(0.66f, 2, 0.15f));
    v.push_back(pluck("Talkbox Funk", "Funk & Synthwave", 0.66f, 800, 0.0f, 0.3f, 0.3f).filter(4, 900, 0.5f, 0.0f).set("vowel", 0.2f).set("f_env", 0.6f));
    v.push_back(pluck("Disco Funk Rubber", "Funk & Synthwave", 0.33f, 500, 0.6f, 0.15f, 0.35f).glide(30));
    v.push_back(pluck("Retro Arp Bass", "Funk & Synthwave", 1.0f, 1200, 0.3f, 0.1f, 0.2f));
    v.push_back(acid("Electro Funk Squelch", "Funk & Synthwave", 1.0f, 600, 0.6f, 0.5f, 0.2f, 30));
    // ---------------------------------------------------------------- Lo-Fi
    v.push_back(subBass("Lo-Fi Warm Sub", "Lo-Fi", 1, 0.2f, 0.01f, 30).filter(1, 400, 0.0f, 0.0f));
    v.push_back(pluck("Lo-Fi Upright", "Lo-Fi", 0.33f, 500, 0.3f, 0.4f, 0.3f).dist(3, 0.15f, 0.4f).set("noise", 0.02f));
    v.push_back(pluck("Lo-Fi Mellow", "Lo-Fi", 0.2f, 350, 0.2f, 0.5f, 0.3f));
    v.push_back(pluck("Cassette Bass", "Lo-Fi", 0.33f, 450, 0.3f, 0.3f, 0.35f).dist(4, 0.3f, 0.5f).set("lfo_pitch", 0.01f).set("lfo_sync", 0).set("lfo_rate", 0.6f));
    v.push_back(pluck("Rainy Day Bass", "Lo-Fi", 0.0f, 300, 0.2f, 0.6f, 0.3f).glide(80));
    v.push_back(pluck("Vinyl Thump", "Lo-Fi", 0.0f, 250, 0.3f, 0.25f, 0.5f).dist(3, 0.2f, 0.3f));
    // ---------------------------------------------------------------- Future Bass / Pop / EDM
    v.push_back(reese("Future Bass Saw Stack", "Future & Pop", 0.4f, 2500, 0.2f).filter(0, 2500, 0.2f, 0.3f, 0.3f, 0.6f));
    v.push_back(pluck("Pop Pluck Bass", "Future & Pop", 0.66f, 700, 0.5f, 0.2f, 0.4f));
    v.push_back(subBass("Pop Clean Sub", "Future & Pop", 0, 0.0f, 0.003f, 0));
    v.push_back(pluck("EDM Big Room Bass", "Future & Pop", 0.66f, 500, 0.6f, 0.2f, 0.55f).osc(0.66f, 3, 0.25f).dist(0, 0.4f).glue(0.5f));
    v.push_back(wobble("Future Wobble Chords", "Future & Pop", 3, 0.5f, 0, 900, 0.2f).poly().set("lfo_retrig", 0));
    v.push_back(pluck("Midtempo Saw", "Future & Pop", 0.66f, 400, 0.5f, 0.3f, 0.5f).dist(1, 0.4f));
    v.push_back(pluck("Hyperpop Bitcrush", "Future & Pop", 1.0f, 900, 0.4f, 0.2f, 0.4f).dist(3, 0.6f));
    v.push_back(reese("Wavy Detune", "Future & Pop", 0.5f, 1800, 0.2f, 0.2f, 0));
    // ---------------------------------------------------------------- Amapiano / Afro
    v.push_back(b808("Log Drum Classic", "Amapiano & Afro", 7, 0.35f, 0.3f, 4, 60).osc(0.4f).filter(0, 900, 0.3f, 0.5f, 0.15f, 0.0f).sub(0.3f));
    v.push_back(b808("Log Drum Wooden", "Amapiano & Afro", 5, 0.25f, 0.2f, 0, 40).osc(0.33f).punch(5, 25, 0.35f));
    v.push_back(b808("Log Drum Gritty", "Amapiano & Afro", 9, 0.4f, 0.55f, 1, 80).osc(0.5f).filter(0, 1200, 0.35f, 0.4f, 0.2f, 0.0f));
    v.push_back(b808("Log Drum Long Slide", "Amapiano & Afro", 6, 0.8f, 0.35f, 4, 160).osc(0.4f));
    v.push_back(pluck("Afrobeats Round Bass", "Amapiano & Afro", 0.2f, 700, 0.3f, 0.25f, 0.45f).glide(30));
    v.push_back(b808("Afro Swing 808", "Amapiano & Afro", 10, 1.5f, 0.3f, 0, 70));
    // ---------------------------------------------------------------- Phonk
    v.push_back(b808("Phonk Drift 808", "Phonk", 14, 3.0f, 0.85f, 1, 120).glue(0.4f));
    v.push_back(b808("Phonk Distorted Slide", "Phonk", 12, 3.0f, 0.9f, 0, 220));
    v.push_back(b808("Phonk Crushed Tape", "Phonk", 12, 2.5f, 0.6f, 3, 100).set("noise", 0.03f));
    v.push_back(b808("Brazilian Phonk Boom", "Phonk", 18, 1.5f, 0.8f, 1, 0).punch(18, 40, 0.5f));
    v.push_back(b808("Cowbell Phonk Bass", "Phonk", 10, 2.0f, 0.7f, 2, 80).fm(0.4f, 5));
    v.push_back(b808("Phonk Fold Monster", "Phonk", 14, 2.5f, 0.75f, 2, 140));
    // ---------------------------------------------------------------- Cinematic
    v.push_back(reese("Braam Bass", "Cinematic", 0.5f, 400, 0.7f).amp(0.05f, 3.0f, 0.8f, 1.5f).dist(1, 0.6f).poly());
    v.push_back(b808("Sub Drop", "Cinematic", 36, 5.0f, 0.2f, 4, 0).punch(36, 500, 0.3f));
    v.push_back(reese("Dark Drone Bass", "Cinematic", 0.2f, 250, 0.3f, 0.15f, 0).amp(1.0f, 2.0f, 1.0f, 2.0f));
    v.push_back(pluck("Pulse Tension", "Cinematic", 1.0f, 400, 0.3f, 0.1f, 0.4f).wobble(4, 0.0f).set("lfo_vol", 0.6f).set("lfo_shape", 3));
    v.push_back(b808("Distorted Impact", "Cinematic", 24, 2.0f, 0.9f, 2, 0).punch(24, 120, 0.8f));
    v.push_back(subBass("Low Rumble", "Cinematic", 1, 0.4f, 0.5f, 0).amp(0.5f, 2.0f, 1.0f, 2.0f).set("noise", 0.05f));
    // ---------------------------------------------------------------- Producers / Torbey Beats
    v.push_back(b808("808 del Cerro", "Producers/Torbey Beats", 14, 3.0f, 0.55f, 4, 120));
    v.push_back(wobble("Wobble de Ómnibus Lleno", "Producers/Torbey Beats", 3, 0.75f, 4, 700, 0.5f));
    v.push_back(reese("Reese de la Rambla", "Producers/Torbey Beats", 0.4f, 800, 0.35f, 0.25f, 1));
    v.push_back(b808("Bajo de Choripán", "Producers/Torbey Beats", 18, 1.2f, 0.45f, 0, 0));
    v.push_back(acid("Acid de Pilsen Tibia", "Producers/Torbey Beats", 0.66f, 380, 0.8f, 0.85f, 0.2f, 60));
    v.push_back(pluck("Pluck de Mate Lavado", "Producers/Torbey Beats", 0.33f, 600, 0.4f, 0.2f, 0.4f));
    v.push_back(growl("Gruñido del Perro de la Esquina", "Producers/Torbey Beats", 0.7f, 3, 3, 0.8f, 0.6f, 1));
    v.push_back(b808("Slide Ruta Interbalnearia", "Producers/Torbey Beats", 10, 3.0f, 0.4f, 0, 250));
    v.push_back(subBass("Sub del Río de la Plata", "Producers/Torbey Beats", 0, 0.1f, 0.005f, 20));
    v.push_back(pluck("Candombe Groove Bass", "Producers/Torbey Beats", 0.33f, 700, 0.5f, 0.15f, 0.35f).punch(5, 20, 0.3f));
    // ---------------------------------------------------------------- Producers / Pat Osk
    v.push_back(b808("Pat Osk Signature 808", "Producers/Pat Osk", 16, 2.8f, 0.5f, 4, 90).glue(0.35f));
    v.push_back(growl("Chajá Enojado", "Producers/Pat Osk", 0.8f, 4, 4, 0.9f, 0.65f, 2));
    v.push_back(reese("Bondi Nocturno", "Producers/Pat Osk", 0.45f, 600, 0.4f, 0.3f, 6));
    v.push_back(b808("Garrafa de Supergás 808", "Producers/Pat Osk", 20, 2.0f, 0.7f, 1, 0).punch(20, 40, 0.6f));
    v.push_back(wobble("Yoi de Feriante", "Producers/Pat Osk", 8, 0.8f, 4, 650, 0.45f));
    v.push_back(acid("Grappamiel Ácida", "Producers/Pat Osk", 1.0f, 420, 0.85f, 0.8f, 0.18f, 70));
    v.push_back(b808("Ta Re Pesado", "Producers/Pat Osk", 12, 3.5f, 0.85f, 1, 160).glue(0.5f));
    v.push_back(pluck("Bajo de Tablado", "Producers/Pat Osk", 0.66f, 500, 0.6f, 0.18f, 0.45f).dist(4, 0.35f));
    v.push_back(subBass("Sub Celeste", "Producers/Pat Osk", 0, 0.15f, 0.004f, 40));
    v.push_back(reese("Hoover de Tienda Inglesa", "Producers/Pat Osk", 0.7f, 1800, 0.45f).width(1.0f).glide(60));
    return v;
}

// Volume (dB) per preset so they all sound about as loud (measured).
const std::map<std::string, float> kLevels = {
#include "plugins/PatBass/PresetLevels.inc"
};
}  // namespace

std::vector<pat::ui::FactoryPreset> BassProcessor::factoryPresets() {
    std::vector<pat::ui::FactoryPreset> out;
    for (auto& p : build()) {
        const auto it = kLevels.find(p.f.name.toStdString());
        if (it != kLevels.end()) p.set("volume", it->second);
        out.push_back(std::move(p.f));
    }
    return out;
}
