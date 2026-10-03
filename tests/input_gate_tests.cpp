#include "ui/input_gate.hpp"
#include <cassert>
#include <iostream>

int main() {
    inst::ui::InputGate gate;
    assert(gate.Allow(1, false, 0));
    gate.Arm(0);
    assert(!gate.Allow(1, false, 10));
    assert(!gate.Allow(0, false, 30));
    assert(!gate.Allow(1, false, 100)); // Rapid second B press.
    assert(!gate.Allow(1, false, 300)); // Held press cannot become a fresh press.
    assert(!gate.Allow(0, false, 350)); // Neutral frame is consumed too.
    assert(gate.Allow(1, false, 400));
    gate.Arm(400);
    assert(!gate.Allow(0, true, 700)); // Dialog/keyboard touch release is also required.
    assert(!gate.Allow(0, false, 710));
    assert(gate.Allow(1, false, 720));
    std::cout << "Navigation input release and rapid-button tests passed\n";
}
