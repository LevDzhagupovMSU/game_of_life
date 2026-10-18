#include <cstddef>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "game/grid.hpp"
#include "game/cell.hpp"
#include "server/message.hpp"


std::size_t game::Grid::cellCount() {
    if (state_.width != 0 && state_.height > std::numeric_limits<std::size_t>::max() / state_.width) {
        throw std::length_error("Grid dimensions are too large");
    }
    return state_.width * state_.height;
}

game::Grid::GridState::GridState(std::size_t width_, std::size_t height_) : width(width_), height(height_){
    if (width_ < 3 || height_ < 3) {
        throw std::invalid_argument("Torus dimensions must be at least 3");
    }

    if (width_ != 0 && height_ > std::numeric_limits<std::size_t>::max() / width_) {
        throw std::length_error("Grid dimensions are too large");
    }
    const std::size_t count = width_ * height_;
    game::Grid::GridState::cells.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        cells.emplace_back(i % width_, i / width_);
    }
}

game::Grid::GridState::GridState(std::size_t width_, std::size_t height_, const std::vector<CellState>& cells_states)
    : width(width_), height(height_) {
    if (width_ < 3 || height_ < 3) {
        throw std::invalid_argument("Torus dimensions must be at least 3");
    }

    if (width_ != 0 && height_ > std::numeric_limits<std::size_t>::max() / width_) {
        throw std::length_error("Grid dimensions are too large");
    }
    const std::size_t count = width_ * height_;
    if (cells_states.size() != count) {
        throw std::invalid_argument("Cell state count does not match grid dimensions");
    }
    game::Grid::GridState::cells.reserve(count);
    for(std::size_t i = 0; i < count; ++i){
        cells.emplace_back(i % width_, i / width_, cells_states[i]);
    }
}

game::Grid::Grid(std::size_t width, std::size_t height) : state_(width, height) {}

game::Grid::Grid(server::ClientMessage client_message) : state_(client_message.width, client_message.height, client_message.grid){
    const std::size_t count = state_.width * state_.height;
    for(std::size_t i = 0; i < count; ++i){
        if(state_.cells[i].state == game::CellState::alive)
            alive_cells_.insert(i);
    }
}

const std::size_t game::Grid::width() const noexcept { return state_.width; }

const std::size_t game::Grid::height() const noexcept { return state_.height; }

std::vector<game::Cell>& game::Grid::cells() noexcept { return state_.cells; }
const std::vector<game::Cell>& game::Grid::cells() const noexcept { return state_.cells; }

std::unordered_set<std::size_t>& game::Grid::alive_cells() noexcept { return alive_cells_; }

game::CellState game::Grid::at(std::size_t x, std::size_t y) const {
    return state_.cells[indexOf(x, y)].state;
}

void game::Grid::set(std::size_t x, std::size_t y, CellState state) {
    const std::size_t index = indexOf(x, y);
    game::Cell& cell = state_.cells[index];
    if (cell.state == state) {
        return;
    }

    cell.state = state;
    if (state == game::CellState::alive) {
        alive_cells_.insert(index);
    } else {
        const auto alive = std::find(alive_cells_.begin(), alive_cells_.end(), index);
        if (alive != alive_cells_.end()) {
            alive_cells_.erase(alive);
        }
    }
}


void game::Grid::set(std::size_t index, CellState state) {
    if (index >= state_.cells.size()) {
        throw std::out_of_range("Cell index is out of range");
    }
    game::Cell& cell = state_.cells[index];
    if (cell.state == state) {
        return;
    }

    cell.state = state;
    if (state == game::CellState::alive) {
        alive_cells_.insert(index);
    } else {
        const auto alive = std::find(alive_cells_.begin(), alive_cells_.end(), index);
        if (alive != alive_cells_.end()) {
            alive_cells_.erase(alive);
        }
    }
}

std::size_t game::Grid::indexOf(std::size_t x, std::size_t y) const {
    if (x >= state_.width || y >= state_.height) {
        throw std::out_of_range("Grid coordinates are out of range");
    }
    return y * state_.width + x;
}

std::vector<std::size_t> game::Grid::get_neighbours(const game::Cell& cell) const {
    (void)indexOf(cell.x, cell.y);

    std::vector<std::size_t> result;
    result.reserve(8);

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }

            const std::size_t y = dy < 0
                ? (cell.y == 0 ? state_.height - 1 : cell.y - 1)
                : (dy > 0 ? (cell.y + 1 == state_.height ? 0 : cell.y + 1) : cell.y);
            const std::size_t x = dx < 0
                ? (cell.x == 0 ? state_.width - 1 : cell.x - 1)
                : (dx > 0 ? (cell.x + 1 == state_.width ? 0 : cell.x + 1) : cell.x);

            result.push_back(indexOf(x, y));
        }
    }

    return result;
}

void game::Grid::set(){
    for(const auto& alive_cell : alive_cells_){
        state_.cells[alive_cell].state = game::CellState::alive;
    }    
}

void game::Grid::reset(){
    for(const auto alive_cell : alive_cells_){
        state_.cells[alive_cell].state = game::CellState::dead;
    }
    alive_cells_.clear();
}
