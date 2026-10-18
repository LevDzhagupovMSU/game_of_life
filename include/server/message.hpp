#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

#include "game/cell.hpp"

namespace game { class Grid; }

namespace server {
// Данные, которые Session получает от клиента, плюс метаданные,
// добавленные сервером для маршрутизации и поиска матча.
struct ClientMessage {
    std::size_t width;
    std::size_t height;
    std::vector<game::CellState> grid;
};

// Результат вычисления для отправки клиенту. tick обозначает тик
// состояния, содержащегося в state.
struct WorkerResult {
    std::uint16_t session_id = 0;
    std::vector<game::CellState> state;
};

class Parse {
public:
    static ClientMessage from_payload(const std::string& payload);
    static WorkerResult from_grid(const game::Grid& grid, std::uint16_t session_id);
};

} // namespace server

