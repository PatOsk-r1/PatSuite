#include "dsp/tune/Scales.h"

#include <cmath>

namespace pat::dsp {
namespace {

// Western (12-tone equal temperament)
constexpr double kChromatic[] = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100};
constexpr double kMajor[] = {0, 200, 400, 500, 700, 900, 1100};
constexpr double kMinor[] = {0, 200, 300, 500, 700, 800, 1000};
constexpr double kHarmonicMinor[] = {0, 200, 300, 500, 700, 800, 1100};
constexpr double kMelodicMinor[] = {0, 200, 300, 500, 700, 900, 1100};
constexpr double kDorian[] = {0, 200, 300, 500, 700, 900, 1000};
constexpr double kPhrygian[] = {0, 100, 300, 500, 700, 800, 1000};
constexpr double kLydian[] = {0, 200, 400, 600, 700, 900, 1100};
constexpr double kMixolydian[] = {0, 200, 400, 500, 700, 900, 1000};
constexpr double kLocrian[] = {0, 100, 300, 500, 600, 800, 1000};
constexpr double kMajorPenta[] = {0, 200, 400, 700, 900};
constexpr double kMinorPenta[] = {0, 300, 500, 700, 1000};
constexpr double kBlues[] = {0, 300, 500, 600, 700, 1000};
// Arabic maqamat, 24-EDO (a half-flat = 50 cents below the natural note)
constexpr double kRast[] = {0, 200, 350, 500, 700, 900, 1050};
constexpr double kBayati[] = {0, 150, 300, 500, 700, 800, 1000};
constexpr double kHijaz[] = {0, 100, 400, 500, 700, 800, 1000};
constexpr double kSaba[] = {0, 150, 300, 400, 700, 800, 1000};
constexpr double kNahawand[] = {0, 200, 300, 500, 700, 800, 1100};
constexpr double kKurd[] = {0, 100, 300, 500, 700, 800, 1000};
constexpr double kSikah[] = {0, 150, 350, 500, 700, 850, 1050};
constexpr double kAjam[] = {0, 200, 400, 500, 700, 900, 1100};
// Japanese
constexpr double kInSen[] = {0, 100, 500, 700, 1000};
constexpr double kHirajoshi[] = {0, 200, 300, 700, 800};
constexpr double kIwato[] = {0, 100, 500, 600, 1000};
constexpr double kYo[] = {0, 200, 500, 700, 900};
constexpr double kKumoi[] = {0, 200, 300, 700, 900};
constexpr double kRyukyu[] = {0, 400, 500, 700, 1100};

#define PAT_SCALE(name, group, arr) ScaleDef{name, group, arr, static_cast<int>(sizeof(arr) / sizeof(arr[0]))}
const ScaleDef kScales[] = {
    PAT_SCALE("Chromatic", "Western", kChromatic),
    PAT_SCALE("Major", "Western", kMajor),
    PAT_SCALE("Minor", "Western", kMinor),
    PAT_SCALE("Harmonic Minor", "Western", kHarmonicMinor),
    PAT_SCALE("Melodic Minor", "Western", kMelodicMinor),
    PAT_SCALE("Dorian", "Western", kDorian),
    PAT_SCALE("Phrygian", "Western", kPhrygian),
    PAT_SCALE("Lydian", "Western", kLydian),
    PAT_SCALE("Mixolydian", "Western", kMixolydian),
    PAT_SCALE("Locrian", "Western", kLocrian),
    PAT_SCALE("Major Pentatonic", "Western", kMajorPenta),
    PAT_SCALE("Minor Pentatonic", "Western", kMinorPenta),
    PAT_SCALE("Blues", "Western", kBlues),
    PAT_SCALE("Maqam Rast", "Arabic (maqam)", kRast),
    PAT_SCALE("Maqam Bayati", "Arabic (maqam)", kBayati),
    PAT_SCALE("Maqam Hijaz", "Arabic (maqam)", kHijaz),
    PAT_SCALE("Maqam Saba", "Arabic (maqam)", kSaba),
    PAT_SCALE("Maqam Nahawand", "Arabic (maqam)", kNahawand),
    PAT_SCALE("Maqam Kurd", "Arabic (maqam)", kKurd),
    PAT_SCALE("Maqam Sikah", "Arabic (maqam)", kSikah),
    PAT_SCALE("Maqam Ajam", "Arabic (maqam)", kAjam),
    PAT_SCALE("In Sen / Insen", "Japanese", kInSen),
    PAT_SCALE("Hirajoshi", "Japanese", kHirajoshi),
    PAT_SCALE("Iwato", "Japanese", kIwato),
    PAT_SCALE("Yo", "Japanese", kYo),
    PAT_SCALE("Kumoi", "Japanese", kKumoi),
    PAT_SCALE("Ryukyu", "Japanese", kRyukyu),
    ScaleDef{"Custom (drawn)", "Custom", nullptr, 0},
};
#undef PAT_SCALE

constexpr const char* kKeys[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

}  // namespace

int numScales() noexcept { return static_cast<int>(sizeof(kScales) / sizeof(kScales[0])); }
const ScaleDef& scale(int index) noexcept { return kScales[index < 0 ? 0 : (index >= numScales() ? numScales() - 1 : index)]; }
int customScaleIndex() noexcept { return numScales() - 1; }
const char* keyName(int key) noexcept { return kKeys[((key % 12) + 12) % 12]; }

TuningTable buildTable(int scaleIndex, int key, std::uint32_t customMask, double a4Hz) noexcept {
    // Root frequency: the key's note in octave 4, from the A4 reference.
    const double root = a4Hz * std::exp2((static_cast<double>(((key % 12) + 12) % 12) - 9.0) / 12.0);
    const ScaleDef& s = scale(scaleIndex);
    if (s.cents != nullptr) return TuningTable(s.cents, s.count, root);
    double cents[24];
    int n = 0;
    for (int i = 0; i < 24; ++i)
        if (customMask & (1u << i)) cents[n++] = 50.0 * i;
    if (n == 0) {  // an empty drawing means "no correction"
        cents[0] = 0;
        return TuningTable(cents, 0, root);
    }
    return TuningTable(cents, n, root);
}

}  // namespace pat::dsp
