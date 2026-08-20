#include "Config.hpp"
#include "../Logger/Logger.hpp"
#include "../Network/TcpServer.hpp"


auto main() -> int {
    TcpServer server;

    if (!server.start(SERVER::IP, SERVER::PORT)) {
        Logger::Error("Server failed to start on {}:{}", SERVER::IP, SERVER::PORT);
        return EXIT_FAILURE;
    }

    server.run();

    Logger::Warn("Server exit success", SERVER::IP, SERVER::PORT);
    return EXIT_SUCCESS;
}