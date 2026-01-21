#include "config.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <memory>
#include <mutex>

namespace bot {

Config::Config() {
    // Default enabled modules
    enabled_modules_["translation"] = true;
    enabled_modules_["moderation"] = true;
    enabled_modules_["leveling"] = true;
    enabled_modules_["custom_commands"] = true;
    enabled_modules_["welcome"] = true;
    enabled_modules_["music"] = true;
    enabled_modules_["reaction_roles"] = true;
    enabled_modules_["logging"] = true;
    enabled_modules_["notifications"] = true;
}

bool Config::load(const std::string& env_path) {
    std::ifstream file(env_path);
    if (!file.is_open()) {
        std::cerr << "Could not open .env file: " << env_path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find the = separator
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        trim(key);
        trim(value);

        // Remove quotes if present
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                   (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

        env_values_[key] = value;
    }

    // Extract required values
    token_ = get_env_value("DISCORD_BOT_TOKEN");
    if (token_.empty()) {
        std::cerr << "DISCORD_BOT_TOKEN not found in .env file!" << std::endl;
        return false;
    }

    // Optional values
    std::string db_path = get_env_value("DATABASE_PATH");
    if (!db_path.empty()) {
        database_path_ = db_path;
    }

    std::string pool_size = get_env_value("THREAD_POOL_SIZE");
    if (!pool_size.empty()) {
        try {
            thread_pool_size_ = std::stoi(pool_size);
        } catch (...) {
            // Keep default
        }
    }

    // Module enable/disable
    auto check_module = [this](const std::string& key, const std::string& module) {
        std::string value = get_env_value(key);
        if (!value.empty()) {
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);
            enabled_modules_[module] = (value == "true" || value == "1" || value == "yes");
        }
    };

    check_module("ENABLE_TRANSLATION", "translation");
    check_module("ENABLE_MODERATION", "moderation");
    check_module("ENABLE_LEVELING", "leveling");
    check_module("ENABLE_CUSTOM_COMMANDS", "custom_commands");
    check_module("ENABLE_WELCOME", "welcome");
    check_module("ENABLE_MUSIC", "music");
    check_module("ENABLE_REACTION_ROLES", "reaction_roles");
    check_module("ENABLE_LOGGING", "logging");
    check_module("ENABLE_NOTIFICATIONS", "notifications");

    // Optional API keys
    std::string twitch_id = get_env_value("TWITCH_CLIENT_ID");
    std::string twitch_secret = get_env_value("TWITCH_CLIENT_SECRET");
    std::string youtube_key = get_env_value("YOUTUBE_API_KEY");

    if (!twitch_id.empty()) twitch_client_id_ = twitch_id;
    if (!twitch_secret.empty()) twitch_client_secret_ = twitch_secret;
    if (!youtube_key.empty()) youtube_api_key_ = youtube_key;

    return true;
}

bool Config::is_module_enabled(const std::string& module_name) const {
    auto it = enabled_modules_.find(module_name);
    return it != enabled_modules_.end() && it->second;
}

std::optional<std::string> Config::get_twitch_client_id() const {
    return twitch_client_id_;
}

std::optional<std::string> Config::get_twitch_client_secret() const {
    return twitch_client_secret_;
}

std::optional<std::string> Config::get_youtube_api_key() const {
    return youtube_api_key_;
}

bool Config::is_valid() const {
    return !token_.empty();
}

std::string Config::get_env_value(const std::string& key) const {
    auto it = env_values_.find(key);
    return it != env_values_.end() ? it->second : "";
}

// Global config instance
static std::unique_ptr<Config> g_config;
static std::once_flag g_config_init;

Config& get_config() {
    std::call_once(g_config_init, []() {
        g_config = std::make_unique<Config>();
    });
    return *g_config;
}

} // namespace bot
