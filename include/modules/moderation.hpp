#pragma once

#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>

namespace bot {

class ModerationModule {
public:
    ModerationModule(dpp::cluster& bot);

    // Register slash commands
    std::vector<dpp::slashcommand> get_commands();

    // Handle slash commands
    void handle_command(const dpp::slashcommand_t& event);

    // Handle message for auto-moderation
    void handle_message(const dpp::message_create_t& event);

    // Moderation actions
    void warn_user(dpp::snowflake guild_id, dpp::snowflake user_id, dpp::snowflake mod_id, const std::string& reason);
    void mute_user(dpp::snowflake guild_id, dpp::snowflake user_id, dpp::snowflake mod_id, std::chrono::seconds duration, const std::string& reason);
    void unmute_user(dpp::snowflake guild_id, dpp::snowflake user_id);
    void kick_user(dpp::snowflake guild_id, dpp::snowflake user_id, dpp::snowflake mod_id, const std::string& reason);
    void ban_user(dpp::snowflake guild_id, dpp::snowflake user_id, dpp::snowflake mod_id, const std::string& reason, uint32_t delete_days = 0);
    void unban_user(dpp::snowflake guild_id, dpp::snowflake user_id);

    // Mute expiration checker
    void check_expired_mutes();
    void start_mute_checker();

private:
    dpp::cluster& bot_;
    std::mutex spam_mutex_;
    std::map<dpp::snowflake, std::vector<std::chrono::steady_clock::time_point>> message_timestamps_;

    // Command handlers
    void cmd_warn(const dpp::slashcommand_t& event);
    void cmd_warnings(const dpp::slashcommand_t& event);
    void cmd_clearwarnings(const dpp::slashcommand_t& event);
    void cmd_mute(const dpp::slashcommand_t& event);
    void cmd_unmute(const dpp::slashcommand_t& event);
    void cmd_kick(const dpp::slashcommand_t& event);
    void cmd_ban(const dpp::slashcommand_t& event);
    void cmd_unban(const dpp::slashcommand_t& event);
    void cmd_automod(const dpp::slashcommand_t& event);

    // Auto-moderation checks
    bool check_spam(const dpp::message_create_t& event);
    bool check_filtered_words(const dpp::message_create_t& event);
    bool check_links(const dpp::message_create_t& event);
    bool check_mentions(const dpp::message_create_t& event);

    // Helpers
    void log_mod_action(dpp::snowflake guild_id, const std::string& action, dpp::snowflake user_id, dpp::snowflake mod_id, const std::string& reason);
    void take_automod_action(dpp::snowflake guild_id, dpp::snowflake user_id, const std::string& action, const std::string& reason);
};

} // namespace bot
