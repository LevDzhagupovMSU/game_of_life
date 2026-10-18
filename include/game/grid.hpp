#pragma once

#include "cell.hpp"
#include "server/message.hpp"

#include <cstddef>
#include <unordered_set>
#include <vector>

namespace game {

class Grid {
    struct GridState {
        std::size_t width = 1;
        std::size_t height = 1;
        std::vector<Cell> cells;

        GridState(size_t width_, size_t height_);
        GridState(size_t width_, size_t height_, const std::vector<CellState>& cells_states);
    } state_;
    std::unordered_set<std::size_t> alive_cells_;

    [[nodiscard]] std::size_t indexOf(std::size_t x, std::size_t y) const;
    std::size_t cellCount();
public:
    explicit Grid(std::size_t width, std::size_t height);
    Grid(const GridState& grid_state);
    Grid(server::ClientMessage);

    [[nodiscard]] const std::size_t width() const noexcept;
    [[nodiscard]] const std::size_t height() const noexcept;
    [[nodiscard]] std::vector<Cell>& cells() noexcept;
    [[nodiscard]] const std::vector<Cell>& cells() const noexcept;
    [[nodiscard]] std::unordered_set<std::size_t>& alive_cells() noexcept;

    [[nodiscard]] CellState at(std::size_t x, std::size_t y) const;
    void set(std::size_t x, std::size_t y, CellState state);
    void set(std::size_t index, CellState state);
    void set();

    void reset();

    std::vector<std::size_t> get_neighbours(const Cell& cell) const;
};

}
