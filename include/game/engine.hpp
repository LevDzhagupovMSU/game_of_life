#pragma once

#include "grid.hpp"

#include <cstdint>

namespace game {

class GameEngine {
    Grid grid_;

public:
    explicit GameEngine(Grid initialGrid);

    void step();

    [[nodiscard]] const Grid& grid() const noexcept;
    [[nodiscard]] Grid& grid() noexcept;
        
};

}