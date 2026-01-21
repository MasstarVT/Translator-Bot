#pragma once

#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

namespace bot {

class NotificationsModule {
public:
    NotificationsModule(dpp::cluster& bot);
    ~NotificationsModule();

    // Register slash commands
    std::vector<dpp::slashcommand> get_commands();

    // Handle slash commands
    void handle_command(const dpp::slashcommand_t& event);

    // Start/stop notification checkers
    void start();
    void stop();

private:
    dpp::cluster& bot_;
    std::atomic<bool> running_{false};
    std::thread twitch_checker_thread_;
    std::thread youtube_checker_thread_;

    // Command handlers
    void cmd_twitch(const dpp::slashcommand_t& event);
    void cmd_youtube(const dpp::slashcommand_t& event);

    // Twitch subcommands
    void twitch_add(const dpp::slashcommand_t& event);
    void twitch_remove(const dpp::slashcommand_t& event);
    void twitch_list(const dpp::slashcommand_t& event);

    // YouTube subcommands
    void youtube_add(const dpp::slashcommand_t& event);
    void youtube_remove(const dpp::slashcommand_t& event);
    void youtube_list(const dpp::slashcommand_t& event);

    // Notification checkers
    void check_twitch_streams();
    void check_youtube_uploads();

    // API calls (placeholder - requires API keys)
    bool is_twitch_live(const std::string& username);
    std::string get_latest_youtube_video(const std::string& channel_id);

    // Notification sending
    void send_twitch_notification(dpp::snowflake guild_id, dpp::snowflake channel_id,
                                  const std::string& username, dpp::snowflake ping_role_id,
                                  const std::string& custom_message);
    void send_youtube_notification(dpp::snowflake guild_id, dpp::snowflake channel_id,
                                   const std::string& video_id, const std::string& video_title,
                                   dpp::snowflake ping_role_id, const std::string& custom_message);
};

} // namespace bot
