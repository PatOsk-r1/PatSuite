#include "dsp/pan/PanCurve.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace pat::dsp {

namespace {
constexpr float kPi = 3.14159265358979f;

float clampf(float v, float lo, float hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

// Shaped segment position: bend 0 = straight, > 0 = rises late, < 0 = rises early.
float bendT(float t, float bend) noexcept {
    const float c = bend * 8.0f;
    if (std::abs(c) < 1.0e-3f) return t;
    return (std::exp(c * t) - 1.0f) / (std::exp(c) - 1.0f);
}

void appendFloat(std::string& s, float v) {
    char buf[32];
    const auto r = std::to_chars(buf, buf + sizeof(buf), std::round(v * 1000.0f) / 1000.0f, std::chars_format::fixed, 3);
    s.append(buf, r.ptr);
}
}  // namespace

float CurveTable::at(double phase) const noexcept {
    const double p = (phase - std::floor(phase)) * kSize;
    const int i = static_cast<int>(p);
    const float f = static_cast<float>(p - i);
    const int a = i < 0 ? 0 : (i >= kSize ? kSize - 1 : i);
    return v[static_cast<std::size_t>(a)] + f * (v[static_cast<std::size_t>(a) + 1] - v[static_cast<std::size_t>(a)]);
}

PanCurve::PanCurve() { pts_ = shape(Sine); }

void PanCurve::sort() {
    std::stable_sort(pts_.begin(), pts_.end(), [](const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });
    if (!pts_.empty()) pts_.front().x = 0.0f;
}

int PanCurve::add(float x, float y) {
    if (static_cast<int>(pts_.size()) >= kMaxPoints) return -1;
    CurvePoint p;
    p.x = clampf(x, 0.001f, 0.999f);
    p.y = clampf(y, -1.0f, 1.0f);
    // Inherit the look of the segment it splits.
    for (auto it = pts_.rbegin(); it != pts_.rend(); ++it)
        if (it->x <= p.x) {
            p.type = it->type;
            break;
        }
    pts_.push_back(p);
    sort();
    for (int i = 0; i < static_cast<int>(pts_.size()); ++i)
        if (pts_[static_cast<std::size_t>(i)].x == p.x && pts_[static_cast<std::size_t>(i)].y == p.y) return i;
    return -1;
}

void PanCurve::move(int index, float x, float y) {
    if (index < 0 || index >= static_cast<int>(pts_.size())) return;
    auto& p = pts_[static_cast<std::size_t>(index)];
    p.y = clampf(y, -1.0f, 1.0f);
    if (index == 0) return;  // the first point stays at the start of the cycle
    // A point cannot pass its neighbours (keeps indices stable while dragging).
    const float lo = pts_[static_cast<std::size_t>(index) - 1].x + 0.001f;
    const float hi = index + 1 < static_cast<int>(pts_.size()) ? pts_[static_cast<std::size_t>(index) + 1].x - 0.001f : 0.999f;
    p.x = clampf(x, lo, std::max(lo, hi));
}

void PanCurve::remove(int index) {
    if (index <= 0 || index >= static_cast<int>(pts_.size())) return;
    pts_.erase(pts_.begin() + index);
}

void PanCurve::setBend(int index, float bend) {
    if (index >= 0 && index < static_cast<int>(pts_.size())) pts_[static_cast<std::size_t>(index)].bend = clampf(bend, -1.0f, 1.0f);
}

void PanCurve::setType(int index, int type) {
    if (index >= 0 && index < static_cast<int>(pts_.size())) pts_[static_cast<std::size_t>(index)].type = std::clamp(type, 0, 2);
}

void PanCurve::setPoints(std::vector<CurvePoint> pts) {
    if (pts.empty()) pts.push_back({});
    if (pts.size() > kMaxPoints) pts.resize(kMaxPoints);
    for (auto& p : pts) {
        p.x = clampf(p.x, 0.0f, 0.999f);
        p.y = clampf(p.y, -1.0f, 1.0f);
        p.bend = clampf(p.bend, -1.0f, 1.0f);
        p.type = std::clamp(p.type, 0, 2);
    }
    pts_ = std::move(pts);
    sort();
}

float PanCurve::valueAt(float x) const noexcept {
    const int n = static_cast<int>(pts_.size());
    if (n == 0) return 0.0f;
    x -= std::floor(x);
    int i = n - 1;
    for (int k = 0; k + 1 < n; ++k)
        if (x < pts_[static_cast<std::size_t>(k) + 1].x) {
            i = k;
            break;
        }
    const auto& a = pts_[static_cast<std::size_t>(i)];
    const auto& b = pts_[static_cast<std::size_t>((i + 1) % n)];
    const float bx = i + 1 < n ? b.x : 1.0f;
    const float len = bx - a.x;
    const float t = len > 1.0e-6f ? clampf((x - a.x) / len, 0.0f, 1.0f) : 0.0f;
    switch (a.type) {
        case CurvePoint::Hold: return a.y;
        case CurvePoint::Smooth: return a.y + (b.y - a.y) * (0.5f - 0.5f * std::cos(kPi * t));
        default: return a.y + (b.y - a.y) * bendT(t, a.bend);
    }
}

void PanCurve::render(CurveTable& t) const noexcept {
    for (int i = 0; i < CurveTable::kSize; ++i) t.v[static_cast<std::size_t>(i)] = valueAt(static_cast<float>(i) / CurveTable::kSize);
    t.v[CurveTable::kSize] = t.v[0];
}

std::string PanCurve::toString() const {
    std::string s;
    for (const auto& p : pts_) {
        if (!s.empty()) s += ';';
        appendFloat(s, p.x);
        s += ',';
        appendFloat(s, p.y);
        s += ',';
        appendFloat(s, p.bend);
        s += ',';
        s += static_cast<char>('0' + p.type);
    }
    return s;
}

bool PanCurve::fromString(const std::string& s) {
    std::vector<CurvePoint> pts;
    const char* c = s.data();
    const char* end = c + s.size();
    while (c < end) {
        float v[4] = {0, 0, 0, 0};
        for (int k = 0; k < 4; ++k) {
            const auto r = std::from_chars(c, end, v[k]);
            if (r.ec != std::errc()) return false;
            c = r.ptr;
            if (k < 3) {
                if (c >= end || *c != ',') return false;
                ++c;
            }
        }
        pts.push_back({v[0], v[1], v[2], static_cast<int>(v[3])});
        if (c < end) {
            if (*c != ';') return false;
            ++c;
        }
    }
    if (pts.empty()) return false;
    setPoints(std::move(pts));
    return true;
}

const char* PanCurve::shapeName(int shape) noexcept {
    static const char* names[] = {"Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Steps 4", "Steps 8", "Bounce", "Gate", "Ramp", "Random"};
    return names[std::clamp(shape, 0, NumShapes - 1)];
}

std::vector<CurvePoint> PanCurve::shape(int shape, unsigned seed) {
    using P = CurvePoint;
    switch (shape) {
        case Triangle: return {{0, -1, 0, P::Bend}, {0.5f, 1, 0, P::Bend}};
        case Square: return {{0, -1, 0, P::Hold}, {0.5f, 1, 0, P::Hold}};
        case SawUp: return {{0, -1, 0, P::Bend}, {0.97f, 1, 0, P::Bend}};
        case SawDown: return {{0, 1, 0, P::Bend}, {0.97f, -1, 0, P::Bend}};
        case Steps4: return {{0, -1, 0, P::Hold}, {0.25f, -0.33f, 0, P::Hold}, {0.5f, 0.33f, 0, P::Hold}, {0.75f, 1, 0, P::Hold}};
        case Steps8:
            return {{0, -1, 0, P::Hold},       {0.125f, 1, 0, P::Hold},     {0.25f, -0.5f, 0, P::Hold}, {0.375f, 0.5f, 0, P::Hold},
                    {0.5f, -1, 0, P::Hold},    {0.625f, 0.25f, 0, P::Hold}, {0.75f, 1, 0, P::Hold},     {0.875f, -0.25f, 0, P::Hold}};
        case Bounce: return {{0, -1, -0.6f, P::Bend}, {0.5f, 1, 0.6f, P::Bend}};
        case Gate: return {{0, 1, 0, P::Hold}, {0.35f, -1, 0, P::Hold}};
        case Ramp: return {{0, -1, 0.55f, P::Bend}, {0.9f, 1, 0, P::Bend}};
        case Random: {
            std::vector<CurvePoint> pts;
            unsigned s = seed * 2654435761u + 12345u;
            for (int i = 0; i < 8; ++i) {
                s = s * 1664525u + 1013904223u;
                pts.push_back({i / 8.0f, static_cast<float>((s >> 8) % 2001) / 1000.0f - 1.0f, 0, P::Hold});
            }
            return pts;
        }
        default: return {{0, -1, 0, P::Smooth}, {0.5f, 1, 0, P::Smooth}};
    }
}

}  // namespace pat::dsp
