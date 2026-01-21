#pragma once

#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>

namespace bot {

class LevelingModule {
public:
    LevelingModule(dpp::cluster& bot);

    // Register slash commands
    std::vector<dpp::slashcommand> get_commands();

    // Handle slash commands
    void handle_command(const dpp::slashcommand_t& event);

    // Handle message for XP
    void handle_message(const dpp::message_create_t& event);

    // Handle voice state for voice XP
    void handle_voice_state(const dpp::voice_state_update_t& event);

    // XP calculation
    static int calculate_level(int64_t xp);
    static int64_t xp_for_level(int level);
    static int64_t xp_to_next_level(int64_t current_xp);

    // Voice XP tracking
    void start_voice_xp_tracker();
    void check_voice_channels();

private:
    dpp::cluster& bot_;
    std::mutex voice_mutex_;

    // Track users in voice channels: guild_id -> {user_id -> join_time}
    std::map<dpp::snowflake, std::map<dpp::snowflake, std::chrono::steady_clock::time_point>> voice_users_;

    // Command handlers
    void cmd_rank(const dpp::slashcommand_t& event);
    void cmd_leaderboard(const dpp::slashcommand_t& event);
    void cmd_setxp(const dpp::slashcommand_t& event);
    void cmd_addxp(const dpp::slashcommand_t& event);
    void cmd_resetxp(const dpp::slashcommand_t& event);
    void cmd_levelconfig(const dpp::slashcommand_t& event);
    void cmd_levelreward(const dpp::slashcommand_t& event);

    // XP management
    void add_xp(dpp::snowflake guild_id, dpp::snowflake user_id, int amount);
    void check_level_up(dpp::snowflake guild_id, dpp::snowflake user_id, int old_level, int new_level, dpp::snowflake channel_id);
    void grant_level_rewards(dpp::snowflake guild_id, dpp::snowflake user_id, int level);

    // Helpers
    dpp::embed create_rank_card(dpp::snowflake guild_id, dpp::snowflake user_id, const std::string& username, const std::string& avatar_url);
};

} // namespace bot
