#pragma once

#include <string>
#include <optional>
#include <map>

namespace bot {

class Config {
public:
    Config();

    // Load configuration from .env file
    bool load(const std::string& env_path = ".env");

    // Get configuration values
    std::string get_token() const { return token_; }
    std::string get_database_path() const { return database_path_; }
    int get_thread_pool_size() const { return thread_pool_size_; }

    // Module enable/disable
    bool is_module_enabled(const std::string& module_name) const;

    // Optional API keys for extended features
    std::optional<std::string> get_twitch_client_id() const;
    std::optional<std::string> get_twitch_client_secret() const;
    std::optional<std::string> get_youtube_api_key() const;

    // Validation
    bool is_valid() const;

private:
    std::string token_;
    std::string database_path_ = "data/bot.db";
    int thread_pool_size_ = 4;

    std::map<std::string, bool> enabled_modules_;

    std::optional<std::string> twitch_client_id_;
    std::optional<std::string> twitch_client_secret_;
    std::optional<std::string> youtube_api_key_;

    std::string get_env_value(const std::string& key) const;
    std::map<std::string, std::string> env_values_;
};

// Global config instance
Config& get_config();

} // namespace bot
