#include <cstddef>
#include <iostream>
#include <utility>

#include "game/engine.hpp"
#include "game/grid.hpp"

namespace {

void printGrid(const game::Grid& grid, std::size_t generation) {
    std::cout << "Generation " << generation << '\n';
    for (std::size_t y = 0; y < grid.height(); ++y) {
        for (std::size_t x = 0; x < grid.width(); ++x) {
            std::cout << (grid.at(x, y) == game::CellState::alive ? '#' : '.');
        }
        std::cout << '\n';
    }
    std::cout << '\n';
}

}

int main() {
    game::Grid initial_grid(20, 10);
    initial_grid.set(1, 1, game::CellState::alive);
    initial_grid.set(2, 2, game::CellState::alive);
    initial_grid.set(0, 3, game::CellState::alive);
    initial_grid.set(1, 3, game::CellState::alive);
    initial_grid.set(2, 3, game::CellState::alive);

    game::GameEngine engine(std::move(initial_grid));
    constexpr std::size_t generations = 100;

    for (std::size_t generation = 0; generation <= generations; ++generation) {
        printGrid(engine.grid(), generation);
        if (generation < generations) {
            engine.step();
        }
    }

    return 0;
}