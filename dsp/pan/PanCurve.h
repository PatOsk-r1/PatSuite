// The drawn movement of Patan: a loop of points over one cycle.
//
//   x: position in the cycle, 0..1 (the first point is always at 0; the curve wraps around)
//   y: -1 = full left (or silent / narrow) .. +1 = full right (or full level / wide)
//   bend: -1..1, how the segment that starts at this point bends (0 = straight)
//   type: Bend (straight or bent), Smooth (S-curve), Hold (step: stays until the next point)
//
// The curve is rendered into a fixed table that the audio thread reads; text form for presets:
// "x,y,bend,type;x,y,bend,type;..."
#pragma once

#include <array>
#include <string>
#include <vector>

namespace pat::dsp {

struct CurvePoint {
    enum Type { Bend = 0, Smooth = 1, Hold = 2 };
    float x = 0, y = 0, bend = 0;
    int type = Bend;
};

struct CurveTable {
    static constexpr int kSize = 1024;
    std::array<float, kSize + 1> v{};  // one extra sample for interpolation (== v[0])
    float at(double phase) const noexcept;  // phase 0..1
};

class PanCurve {
public:
    static constexpr int kMaxPoints = 64;

    PanCurve();  // default: a sine-like left-right sweep
    const std::vector<CurvePoint>& points() const noexcept { return pts_; }

    // Editing (message thread). Indices refer to points() after sorting by x.
    int add(float x, float y);  // returns the new index
    void move(int index, float x, float y);
    void remove(int index);  // the first point cannot be removed
    void setBend(int index, float bend);
    void setType(int index, int type);
    void setPoints(std::vector<CurvePoint> pts);

    float valueAt(float x) const noexcept;  // -1..1
    void render(CurveTable& t) const noexcept;

    std::string toString() const;
    bool fromString(const std::string& s);  // false (and unchanged) if the text is not a curve

    // Ready-made shapes for the shape menu (and presets).
    enum Shape { Sine, Triangle, Square, SawUp, SawDown, Steps4, Steps8, Bounce, Gate, Ramp, Random, NumShapes };
    static const char* shapeName(int shape) noexcept;
    static std::vector<CurvePoint> shape(int shape, unsigned seed = 1);

private:
    void sort();
    std::vector<CurvePoint> pts_;
};

}  // namespace pat::dsp
