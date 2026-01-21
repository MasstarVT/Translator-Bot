#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <mutex>
#include <memory>
#include <functional>
#include <dpp/dpp.h>
#include <sqlite3.h>

namespace bot {

class Database {
public:
    Database();
    ~Database();

    // Disable copy
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Initialize database and create tables
    bool initialize(const std::string& db_path = "data/bot.db");
    void close();

    // ==================== Guild Settings ====================
    struct GuildSettings {
        dpp::snowflake guild_id;
        std::string prefix = "!";
        std::string language = "en";
    };

    std::optional<GuildSettings> get_guild_settings(dpp::snowflake guild_id);
    bool set_guild_settings(const GuildSettings& settings);

    // ==================== Auto-Translate ====================
    struct AutoTranslateChannel {
        dpp::snowflake channel_id;
        dpp::snowflake guild_id;
        std::vector<std::string> target_languages;
    };

    std::optional<AutoTranslateChannel> get_auto_translate_channel(dpp::snowflake channel_id);
    std::vector<AutoTranslateChannel> get_guild_auto_translate_channels(dpp::snowflake guild_id);
    bool set_auto_translate_channel(const AutoTranslateChannel& channel);
    bool remove_auto_translate_channel(dpp::snowflake channel_id);

    // ==================== Moderation ====================
    struct ModerationSettings {
        dpp::snowflake guild_id;
        bool anti_spam_enabled = false;
        int spam_threshold = 5;
        std::string spam_action = "warn";  // warn, mute, kick, ban
        bool anti_links_enabled = false;
        bool anti_mentions_enabled = false;
        int mention_threshold = 5;
        dpp::snowflake mute_role_id = 0;
        dpp::snowflake mod_log_channel_id = 0;
    };

    struct Warning {
        int64_t id;
        dpp::snowflake guild_id;
        dpp::snowflake user_id;
        dpp::snowflake moderator_id;
        std::string reason;
        int64_t timestamp;
    };

    struct Mute {
        int64_t id;
        dpp::snowflake guild_id;
        dpp::snowflake user_id;
        dpp::snowflake moderator_id;
        std::string reason;
        int64_t start_time;
        int64_t end_time;
        bool active;
    };

    std::optional<ModerationSettings> get_moderation_settings(dpp::snowflake guild_id);
    bool set_moderation_settings(const ModerationSettings& settings);

    // Filtered words
    std::vector<std::string> get_filtered_words(dpp::snowflake guild_id);
    bool add_filtered_word(dpp::snowflake guild_id, const std::string& word);
    bool remove_filtered_word(dpp::snowflake guild_id, const std::string& word);

    // Automod whitelist
    bool is_whitelisted(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type);
    bool add_whitelist(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type);
    bool remove_whitelist(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type);

    // Warnings
    int64_t add_warning(const Warning& warning);
    std::vector<Warning> get_warnings(dpp::snowflake guild_id, dpp::snowflake user_id);
    int get_warning_count(dpp::snowflake guild_id, dpp::snowflake user_id);
    bool clear_warnings(dpp::snowflake guild_id, dpp::snowflake user_id, int amount = -1);
    bool delete_warning(int64_t warning_id);

    // Mutes
    int64_t add_mute(const Mute& mute);
    std::optional<Mute> get_active_mute(dpp::snowflake guild_id, dpp::snowflake user_id);
    std::vector<Mute> get_expired_mutes();
    bool deactivate_mute(dpp::snowflake guild_id, dpp::snowflake user_id);

    // ==================== Leveling ====================
    struct LevelingSettings {
        dpp::snowflake guild_id;
        bool enabled = true;
        int xp_min = 15;
        int xp_max = 25;
        int xp_cooldown = 60;  // seconds
        int voice_xp = 10;
        int voice_min_users = 2;
        std::string level_up_message = "Congratulations {user}! You reached level {level}!";
        dpp::snowflake level_up_channel_id = 0;  // 0 = same channel
    };

    struct UserXP {
        dpp::snowflake guild_id;
        dpp::snowflake user_id;
        int64_t xp = 0;
        int level = 0;
        int64_t total_messages = 0;
        int64_t voice_minutes = 0;
        int64_t last_xp_time = 0;
    };

    struct LevelReward {
        int64_t id;
        dpp::snowflake guild_id;
        int level;
        dpp::snowflake role_id;
    };

    std::optional<LevelingSettings> get_leveling_settings(dpp::snowflake guild_id);
    bool set_leveling_settings(const LevelingSettings& settings);

    std::optional<UserXP> get_user_xp(dpp::snowflake guild_id, dpp::snowflake user_id);
    bool set_user_xp(const UserXP& user_xp);
    std::vector<UserXP> get_leaderboard(dpp::snowflake guild_id, int limit = 10, int offset = 0);
    int get_user_rank(dpp::snowflake guild_id, dpp::snowflake user_id);
    bool reset_user_xp(dpp::snowflake guild_id, dpp::snowflake user_id);
    bool reset_guild_xp(dpp::snowflake guild_id);

    std::vector<LevelReward> get_level_rewards(dpp::snowflake guild_id);
    bool add_level_reward(dpp::snowflake guild_id, int level, dpp::snowflake role_id);
    bool remove_level_reward(dpp::snowflake guild_id, int level);
    std::vector<LevelReward> get_rewards_for_level(dpp::snowflake guild_id, int level);

    // XP blacklist
    bool is_xp_blacklisted(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type);
    bool add_xp_blacklist(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type);
    bool remove_xp_blacklist(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type);

    // ==================== Custom Commands ====================
    struct CustomCommand {
        int64_t id;
        dpp::snowflake guild_id;
        std::string name;
        std::string response;
        bool is_embed = false;
        std::string embed_color = "#0099ff";
        dpp::snowflake created_by;
        int64_t uses = 0;
    };

    std::optional<CustomCommand> get_custom_command(dpp::snowflake guild_id, const std::string& name);
    std::vector<CustomCommand> get_guild_custom_commands(dpp::snowflake guild_id);
    bool create_custom_command(const CustomCommand& cmd);
    bool update_custom_command(const CustomCommand& cmd);
    bool delete_custom_command(dpp::snowflake guild_id, const std::string& name);
    bool increment_command_uses(dpp::snowflake guild_id, const std::string& name);

    // ==================== Welcome/Goodbye ====================
    struct WelcomeSettings {
        dpp::snowflake guild_id;
        bool enabled = false;
        dpp::snowflake channel_id = 0;
        std::string message = "Welcome {user} to {server}!";
        bool use_embed = true;
        std::string embed_color = "#00ff00";
        bool dm_enabled = false;
        std::string dm_message = "";
        dpp::snowflake auto_role_id = 0;
    };

    struct GoodbyeSettings {
        dpp::snowflake guild_id;
        bool enabled = false;
        dpp::snowflake channel_id = 0;
        std::string message = "{user} has left the server.";
        bool use_embed = true;
        std::string embed_color = "#ff0000";
    };

    std::optional<WelcomeSettings> get_welcome_settings(dpp::snowflake guild_id);
    bool set_welcome_settings(const WelcomeSettings& settings);
    std::optional<GoodbyeSettings> get_goodbye_settings(dpp::snowflake guild_id);
    bool set_goodbye_settings(const GoodbyeSettings& settings);

    // ==================== Reaction Roles ====================
    struct ReactionRoleMessage {
        int64_t id;
        dpp::snowflake guild_id;
        dpp::snowflake channel_id;
        dpp::snowflake message_id;
        std::string title;
        std::string mode = "normal";  // normal, unique, verify
    };

    struct ReactionRole {
        int64_t id;
        int64_t message_config_id;
        std::string emoji;
        dpp::snowflake role_id;
    };

    std::optional<ReactionRoleMessage> get_reaction_role_message(dpp::snowflake message_id);
    std::vector<ReactionRoleMessage> get_guild_reaction_role_messages(dpp::snowflake guild_id);
    int64_t create_reaction_role_message(const ReactionRoleMessage& msg);
    bool delete_reaction_role_message(dpp::snowflake message_id);

    std::vector<ReactionRole> get_reaction_roles(int64_t message_config_id);
    std::optional<ReactionRole> get_reaction_role(int64_t message_config_id, const std::string& emoji);
    bool add_reaction_role(int64_t message_config_id, const std::string& emoji, dpp::snowflake role_id);
    bool remove_reaction_role(int64_t message_config_id, const std::string& emoji);

    // ==================== Logging ====================
    struct LoggingSettings {
        dpp::snowflake guild_id;
        dpp::snowflake message_log_channel = 0;
        dpp::snowflake member_log_channel = 0;
        dpp::snowflake mod_log_channel = 0;
        dpp::snowflake voice_log_channel = 0;
        dpp::snowflake server_log_channel = 0;
        bool log_message_edits = true;
        bool log_message_deletes = true;
        bool log_member_joins = true;
        bool log_member_leaves = true;
        bool log_member_bans = true;
        bool log_voice_state = true;
        bool log_role_changes = true;
        bool log_nickname_changes = true;
    };

    std::optional<LoggingSettings> get_logging_settings(dpp::snowflake guild_id);
    bool set_logging_settings(const LoggingSettings& settings);

    // Logging ignore list
    bool is_logging_ignored(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type);
    bool add_logging_ignore(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type);
    bool remove_logging_ignore(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type);

    // ==================== Music ====================
    struct MusicSettings {
        dpp::snowflake guild_id;
        dpp::snowflake dj_role_id = 0;
        int max_queue_size = 100;
        int max_song_duration = 3600;  // seconds
        bool allow_playlists = true;
    };

    struct Playlist {
        int64_t id;
        dpp::snowflake guild_id;
        dpp::snowflake user_id;
        std::string name;
        bool is_public = false;
    };

    struct PlaylistTrack {
        int64_t id;
        int64_t playlist_id;
        std::string url;
        std::string title;
        int duration;
        int position;
    };

    std::optional<MusicSettings> get_music_settings(dpp::snowflake guild_id);
    bool set_music_settings(const MusicSettings& settings);

    std::vector<Playlist> get_user_playlists(dpp::snowflake user_id);
    std::optional<Playlist> get_playlist(dpp::snowflake user_id, const std::string& name);
    int64_t create_playlist(const Playlist& playlist);
    bool delete_playlist(int64_t playlist_id);

    std::vector<PlaylistTrack> get_playlist_tracks(int64_t playlist_id);
    bool add_playlist_track(const PlaylistTrack& track);
    bool remove_playlist_track(int64_t playlist_id, int position);
    bool clear_playlist_tracks(int64_t playlist_id);

    // ==================== Notifications ====================
    struct TwitchNotification {
        int64_t id;
        dpp::snowflake guild_id;
        std::string twitch_username;
        dpp::snowflake channel_id;
        dpp::snowflake ping_role_id = 0;
        std::string custom_message = "";
        bool is_live = false;
    };

    struct YouTubeNotification {
        int64_t id;
        dpp::snowflake guild_id;
        std::string youtube_channel_id;
        dpp::snowflake discord_channel_id;
        dpp::snowflake ping_role_id = 0;
        std::string custom_message = "";
        std::string last_video_id = "";
    };

    std::vector<TwitchNotification> get_twitch_notifications(dpp::snowflake guild_id);
    bool add_twitch_notification(const TwitchNotification& notif);
    bool remove_twitch_notification(dpp::snowflake guild_id, const std::string& username);
    bool update_twitch_live_status(dpp::snowflake guild_id, const std::string& username, bool is_live);
    std::vector<TwitchNotification> get_all_twitch_notifications();

    std::vector<YouTubeNotification> get_youtube_notifications(dpp::snowflake guild_id);
    bool add_youtube_notification(const YouTubeNotification& notif);
    bool remove_youtube_notification(dpp::snowflake guild_id, const std::string& channel_id);
    bool update_youtube_last_video(dpp::snowflake guild_id, const std::string& channel_id, const std::string& video_id);
    std::vector<YouTubeNotification> get_all_youtube_notifications();

    // ==================== Raw Query ====================
    bool execute(const std::string& sql);
    bool execute_with_params(const std::string& sql, const std::vector<std::string>& params);

private:
    sqlite3* db_ = nullptr;
    std::mutex db_mutex_;

    bool create_tables();
    bool migrate_from_json();

    // Helper methods
    template<typename T>
    T get_column(sqlite3_stmt* stmt, int col);
};

// Global database instance
Database& get_database();

} // namespace bot
