#include "game/rules.hpp"
#include "game/cell.hpp"

game::CellState game::ConwayRules::nextState(CellState current, std::uint8_t aliveNeighbors) noexcept {
    if(current == game::CellState::alive){
        return (aliveNeighbors < 2 || aliveNeighbors > 3) ? game::CellState::dead : game::CellState::alive;  
    }

    return (aliveNeighbors == 3) ? game::CellState::alive : game::CellState::dead;
}