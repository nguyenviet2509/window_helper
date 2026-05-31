#pragma once
// Mode-A stationary sweep: pick a point in an annulus around the player
// (assumed at the client-rect center) for SHIFT+right-click attack-in-place.
// Radius dùng phân phối Gaussian (cluster quanh tâm) thay vì uniform để né ML detector.
#include <windows.h>
#include <random>
#include <utility>
#include <algorithm>
#include <cmath>

class AttackSweep {
public:
    AttackSweep(int rMin, int rMax)
        : rMin_(rMin), rMax_(rMax),
          rng_(std::random_device{}()),
          angle_(0.0, 6.2831853),
          radius_(meanFor(rMin, rMax), sigmaFor(rMin, rMax)) {}

    std::pair<int, int> pickAttackPosition(HWND game) {
        RECT r{};
        if (game) GetClientRect(game, &r);
        else { r.right = 800; r.bottom = 600; }
        int cx = (r.right + r.left) / 2;
        int cy = (r.bottom + r.top) / 2;
        double a = angle_(rng_);
        double rad = sampleRadius();
        return { cx + static_cast<int>(std::cos(a) * rad),
                 cy + static_cast<int>(std::sin(a) * rad) };
    }

    void setRange(int rMin, int rMax) {
        rMin_ = rMin; rMax_ = rMax;
        radius_ = std::normal_distribution<double>(meanFor(rMin, rMax), sigmaFor(rMin, rMax));
    }

private:
    int rMin_, rMax_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> angle_;
    std::normal_distribution<double> radius_;

    static double meanFor(int rMin, int rMax) {
        return (rMin + rMax) / 2.0;
    }
    // σ = range/6 → ±3σ phủ toàn dải [rMin, rMax] (~99.7% sample không bị clamp).
    static double sigmaFor(int rMin, int rMax) {
        return std::max(1.0, (rMax - rMin) / 6.0);
    }

    // Resample tối đa 5 lần khi out-of-range; fallback clamp ở lần cuối.
    double sampleRadius() {
        for (int i = 0; i < 5; ++i) {
            double r = radius_(rng_);
            if (r >= rMin_ && r <= rMax_) return r;
        }
        double r = radius_(rng_);
        return std::clamp(r, static_cast<double>(rMin_), static_cast<double>(rMax_));
    }
};
