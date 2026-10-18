#include "server/message.hpp"

#include "game/grid.hpp"

#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>

server::ClientMessage server::Parse::from_payload(const std::string& payload) {
    const auto json = nlohmann::json::parse(payload);
    ClientMessage message;
    message.width = json.at("width").get<std::size_t>();
    message.height = json.at("height").get<std::size_t>();

    if (message.height != 0 && message.width > std::numeric_limits<std::size_t>::max() / message.height)
        throw std::invalid_argument("Grid dimensions overflow");

    const auto cell_count = message.width * message.height;
    const auto& cells = json.at("grid");
    if (!cells.is_array() || cells.size() != cell_count)
        throw std::invalid_argument("Grid cell count does not match dimensions");

    message.grid.reserve(cell_count);
    for (const auto& value : cells) {
        const auto state = value.get<int>();
        if (state != 0 && state != 1)
            throw std::invalid_argument("Cell state must be 0 or 1");
        message.grid.push_back(state == 0 ? game::CellState::dead : game::CellState::alive);
    }
    return message;
}

server::WorkerResult server::Parse::from_grid(const game::Grid& grid, std::uint16_t session_id) {
    WorkerResult result;
    result.session_id = session_id;
    result.state.reserve(grid.cells().size());
    for (const auto& cell : grid.cells())
        result.state.push_back(cell.state);
    return result;
}
