#pragma once

#include <cstddef>

namespace game {

enum class CellState {
    dead = 0,
    alive = 1
};

struct Cell {
    CellState state;
    std::size_t x;
    std::size_t y;

    Cell(std::size_t x_, std::size_t y_, CellState state_ = CellState::dead)
        : state(state_), x(x_), y(y_) {}
};

}