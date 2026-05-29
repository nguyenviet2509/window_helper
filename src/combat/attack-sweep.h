#pragma once
// Mode-A stationary sweep: pick a random point in an annulus around the player
// (assumed at the client-rect center) for SHIFT+right-click attack-in-place.
#include <windows.h>
#include <random>
#include <utility>

class AttackSweep {
public:
    AttackSweep(int rMin, int rMax)
        : rMin_(rMin), rMax_(rMax),
          rng_(std::random_device{}()),
          angle_(0.0, 6.2831853),
          radius_(static_cast<double>(rMin), static_cast<double>(rMax)) {}

    std::pair<int, int> pickAttackPosition(HWND game) {
        RECT r{};
        if (game) GetClientRect(game, &r);
        else { r.right = 800; r.bottom = 600; }
        int cx = (r.right + r.left) / 2;
        int cy = (r.bottom + r.top) / 2;
        double a = angle_(rng_);
        double rad = radius_(rng_);
        return { cx + static_cast<int>(std::cos(a) * rad),
                 cy + static_cast<int>(std::sin(a) * rad) };
    }

    void setRange(int rMin, int rMax) {
        rMin_ = rMin; rMax_ = rMax;
        radius_ = std::uniform_real_distribution<double>(
            static_cast<double>(rMin), static_cast<double>(rMax));
    }

private:
    int rMin_, rMax_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> angle_;
    std::uniform_real_distribution<double> radius_;
};
