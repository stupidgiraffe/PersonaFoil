#pragma once
#include <cstdint>

namespace inst::ui {
    // A transition consumes the current press and requires a neutral frame plus
    // a short quiet period before another layout can act on controller input.
    class InputGate {
    public:
        void Arm(std::uint64_t nowMs) { blocked = true; until = nowMs + 250; }
        bool Allow(std::uint64_t buttons, bool touch, std::uint64_t nowMs) {
            if (!blocked) return true;
            if (buttons == 0 && !touch && nowMs >= until) blocked = false;
            return false;
        }
    private:
        bool blocked = false;
        std::uint64_t until = 0;
    };
}
