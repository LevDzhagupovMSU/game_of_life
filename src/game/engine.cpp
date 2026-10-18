#include <cstddef>
#include <vector>
#include <unordered_set>

#include "game/engine.hpp"
#include "game/cell.hpp"
#include "game/grid.hpp"
#include "game/rules.hpp"


game::GameEngine::GameEngine(game::Grid initialGrid) : grid_(std::move(initialGrid)) {};

void game::GameEngine::step(){
    std::unordered_set<std::size_t> cells_to_update;
    std::unordered_set<std::size_t> new_alive_cells;
    new_alive_cells.reserve(grid_.alive_cells().size());

    for(const auto& alive_cell_index : grid_.alive_cells()){
        cells_to_update.insert(alive_cell_index);

        std::vector<std::size_t> neighbours = grid_.get_neighbours(grid_.cells()[alive_cell_index]);
        for(const auto& neighbour : neighbours){
            cells_to_update.insert(neighbour);
        }
    }

    for(const auto& val : cells_to_update){
        std::vector<std::size_t> neighbours = grid_.get_neighbours(grid_.cells()[val]);
        
        std::uint8_t alive_num = 0;
        for(const auto neighbour: neighbours){
            if(grid_.cells()[neighbour].state == game::CellState::alive)
                alive_num++;
        }

        auto next_state = game::ConwayRules::nextState(grid_.cells()[val].state, alive_num);
        if(next_state == game::CellState::alive)
            new_alive_cells.insert(val);
    }
    
    grid_.reset();
    grid_.alive_cells() = std::move(new_alive_cells);
    grid_.set();
}

const game::Grid& game::GameEngine::grid() const noexcept {
    return grid_;
}

game::Grid& game::GameEngine::grid() noexcept {
    return grid_;
}
