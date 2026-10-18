#pragma once

#include "cell.hpp"

#include <cstdint>

namespace game {

class ConwayRules {
public:
    [[nodiscard]] static CellState nextState(
        CellState current,
        std::uint8_t aliveNeighbors) noexcept;
};

} 