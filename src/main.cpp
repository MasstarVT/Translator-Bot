#include "bot.hpp"
#include <iostream>
#include <csignal>

static bool g_running = true;

void signal_handler(int signum) {
    std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
}

int main() {
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "Discord Bot starting..." << std::endl;
    std::cout << "==========================" << std::endl;

    // Get bot instance and initialize
    auto& bot = bot::get_bot();

    if (!bot.initialize()) {
        std::cerr << "Failed to initialize bot" << std::endl;
        return 1;
    }

    // Run the bot
    bot.run();

    // Cleanup
    bot.shutdown();

    std::cout << "Bot shutdown complete" << std::endl;
    return 0;
}
