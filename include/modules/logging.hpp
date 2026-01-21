#pragma once

#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace bot {

class LoggingModule {
public:
    LoggingModule(dpp::cluster& bot);

    // Register slash commands
    std::vector<dpp::slashcommand> get_commands();

    // Handle slash commands
    void handle_command(const dpp::slashcommand_t& event);

    // Event handlers for logging
    void log_message_delete(const dpp::message_delete_t& event);
    void log_message_update(const dpp::message_update_t& event);
    void log_member_join(const dpp::guild_member_add_t& event);
    void log_member_leave(const dpp::guild_member_remove_t& event);
    void log_member_ban(const dpp::guild_ban_add_t& event);
    void log_member_unban(const dpp::guild_ban_remove_t& event);
    void log_voice_state(const dpp::voice_state_update_t& event);

    // Custom log entry
    void log_custom(dpp::snowflake guild_id, const std::string& type, const dpp::embed& embed);

    // Message cache for deleted message logging
    void cache_message(const dpp::message& msg);
    std::optional<dpp::message> get_cached_message(dpp::snowflake message_id);

private:
    dpp::cluster& bot_;
    std::mutex cache_mutex_;

    // Message cache: message_id -> message content
    std::map<dpp::snowflake, dpp::message> message_cache_;
    static constexpr size_t MAX_CACHE_SIZE = 1000;

    // Command handlers
    void cmd_logging(const dpp::slashcommand_t& event);

    // Log channel getters
    std::optional<dpp::snowflake> get_log_channel(dpp::snowflake guild_id, const std::string& type);

    // Helpers
    void send_log(dpp::snowflake channel_id, const dpp::embed& embed);
    bool should_log(dpp::snowflake guild_id, const std::string& event_type);
    bool is_ignored(dpp::snowflake guild_id, dpp::snowflake id);
    void cleanup_cache();
};

} // namespace bot
