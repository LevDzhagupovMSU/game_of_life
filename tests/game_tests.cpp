#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

#include "game/engine.hpp"
#include "game/grid.hpp"
#include "game/rules.hpp"
#include "server/message.hpp"
#include "server/generator_id.hpp"
#include <nlohmann/json.hpp>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Exception, class Function>
void requireThrows(Function&& function, const char* message) {
    try { function(); }
    catch (const Exception&) { return; }
    throw std::runtime_error(message);
}

std::unordered_set<std::size_t> aliveIndices(const game::Grid& grid) {
    std::unordered_set<std::size_t> result;
    for (std::size_t index = 0; index < grid.cells().size(); ++index)
        if (grid.cells()[index].state == game::CellState::alive) result.insert(index);
    return result;
}

void testRectangularGridCoordinatesAndNeighbors() {
    game::Grid grid(5, 4);
    for (std::size_t i = 0; i < grid.cells().size(); ++i) {
        require(grid.cells()[i].x == i % 5, "Grid cell x coordinate is incorrect");
        require(grid.cells()[i].y == i / 5, "Grid cell y coordinate is incorrect");
    }
    const auto neighbors = grid.get_neighbours(grid.cells()[19]);
    const std::unordered_set<std::size_t> expected{13, 14, 10, 18, 15, 3, 4, 0};
    require(std::unordered_set<std::size_t>(neighbors.begin(), neighbors.end()) == expected,
            "Neighbors on a rectangular torus must wrap correctly");
}

void testClientMessageGridConstructor() {
    server::ClientMessage message{3, 3, {
        game::CellState::dead, game::CellState::alive, game::CellState::dead,
        game::CellState::dead, game::CellState::dead, game::CellState::alive,
        game::CellState::alive, game::CellState::dead, game::CellState::dead
    }};
    game::Grid grid(message);
    require(grid.width() == 3 && grid.height() == 3, "Client message dimensions must be preserved");
    require(grid.at(1, 0) == game::CellState::alive && grid.at(2, 1) == game::CellState::alive,
            "Client message cell states must be copied into the grid");
    require(grid.alive_cells().size() == 3, "Alive cell index set must match initial grid");
    requireThrows<std::invalid_argument>([] {
        game::Grid invalid(server::ClientMessage{3, 3, {game::CellState::alive}});
        (void)invalid;
    }, "Client message with a mismatched cell count must be rejected");
}

void testGridRejectsInvalidDimensionsAndIndex() {
    requireThrows<std::invalid_argument>([] { game::Grid grid(2, 5); (void)grid; },
            "Torus dimensions smaller than three must be rejected");
    game::Grid grid(3, 3);
    requireThrows<std::out_of_range>([&] { grid.set(9, game::CellState::alive); },
            "Setting a cell past the end must be rejected");
    requireThrows<std::length_error>([] {
        game::Grid grid(std::numeric_limits<std::size_t>::max(), 3); (void)grid;
    }, "Grid dimension multiplication overflow must be rejected");
}

void testTorusNeighborsWrapAtCorner() {
    game::Grid grid(5, 4);
    const auto neighbors = grid.get_neighbours(grid.cells()[0]);
    const std::unordered_set<std::size_t> expected{19, 15, 16, 4, 1, 9, 5, 6};
    require(neighbors.size() == 8, "A cell must have eight neighbors on this torus");
    require(std::unordered_set<std::size_t>(neighbors.begin(), neighbors.end()) == expected,
            "Neighbors at the corner must wrap across both torus edges");
}

void testParsePayloadAndGridResult() {
    const auto message = server::Parse::from_payload(R"({"width":3,"height":3,"grid":[0,1,0,0,0,1,1,0,0]})");
    game::Grid grid(message);
    const auto result = server::Parse::from_grid(grid, 42);
    require(result.session_id == 42, "Parse result must preserve the session id");
    require(result.state == message.grid, "Parse result must preserve grid cell states");
    requireThrows<std::invalid_argument>([] {
        (void)server::Parse::from_payload(R"({"width":3,"height":3,"grid":[0,1]})");
    }, "Parse must reject a grid with a mismatched number of cells");
    requireThrows<nlohmann::json::exception>([] {
        (void)server::Parse::from_payload("not json");
    }, "Parse must reject malformed JSON");
}

void testSessionIdsBeyond16Bits() {
    server::IDGenerator generator;
    for (std::uint64_t expected = 1; expected <= 65537; ++expected)
        require(generator.next() == expected, "Session IDs must remain unique past 16-bit range");

    game::Grid grid(3, 3);
    const auto payload = server::Parse::to_payload(server::Parse::from_grid(grid, 65537));
    require(nlohmann::json::parse(payload).at("session_id").get<std::uint64_t>() == 65537,
            "Response must retain a 64-bit session ID");
}

void testConwayRules() {
    using game::CellState;
    require(game::ConwayRules::nextState(CellState::alive, 2) == CellState::alive, "Live cell with two neighbors survives");
    require(game::ConwayRules::nextState(CellState::alive, 3) == CellState::alive, "Live cell with three neighbors survives");
    require(game::ConwayRules::nextState(CellState::alive, 1) == CellState::dead, "Live cell with one neighbor dies");
    require(game::ConwayRules::nextState(CellState::alive, 4) == CellState::dead, "Live cell with four neighbors dies");
    require(game::ConwayRules::nextState(CellState::dead, 3) == CellState::alive, "Dead cell with three neighbors is born");
    require(game::ConwayRules::nextState(CellState::dead, 2) == CellState::dead, "Dead cell with two neighbors stays dead");
}

void testBlinkerCrossesTorusEdgeAndOscillates() {
    game::Grid initial_grid(7, 7);
    initial_grid.set(6, 0, game::CellState::alive);
    initial_grid.set(0, 0, game::CellState::alive);
    initial_grid.set(1, 0, game::CellState::alive);
    game::GameEngine engine(std::move(initial_grid));
    engine.step();
    const std::unordered_set<std::size_t> vertical{0, 7, 42};
    require(aliveIndices(engine.grid()) == vertical, "Edge-spanning blinker must become vertical");
    engine.step();
    const std::unordered_set<std::size_t> horizontal{0, 1, 6};
    require(aliveIndices(engine.grid()) == horizontal, "Blinker must return to its original orientation");
}

void testEmptyGridRemainsEmpty() {
    game::GameEngine engine(game::Grid(4, 5));
    engine.step();
    require(engine.grid().alive_cells().empty(), "An empty grid must remain empty");
}

}

int main() {
    try {
        testRectangularGridCoordinatesAndNeighbors();
        testClientMessageGridConstructor();
        testGridRejectsInvalidDimensionsAndIndex();
        testTorusNeighborsWrapAtCorner();
        testParsePayloadAndGridResult();
        testSessionIdsBeyond16Bits();
        testConwayRules();
        testBlinkerCrossesTorusEdgeAndOscillates();
        testEmptyGridRemainsEmpty();
    } catch (const std::exception& error) {
        std::cerr << "Test failed: " << error.what() << "\n";
        return 1;
    }
    std::cout << "All game tests passed\n";
    return 0;
}
