#include "database.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace bot {

using json = nlohmann::json;

Database::Database() {}

Database::~Database() {
    close();
}

bool Database::initialize(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    // Create data directory if it doesn't exist
    std::filesystem::path path(db_path);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // Enable foreign keys and WAL mode for better performance
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);

    if (!create_tables()) {
        std::cerr << "Failed to create tables" << std::endl;
        return false;
    }

    // Migrate existing JSON settings if present
    migrate_from_json();

    std::cout << "Database initialized successfully" << std::endl;
    return true;
}

void Database::close() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::create_tables() {
    const char* sql = R"(
        -- Guild settings
        CREATE TABLE IF NOT EXISTS guilds (
            guild_id INTEGER PRIMARY KEY,
            prefix TEXT DEFAULT '!',
            language TEXT DEFAULT 'en'
        );

        -- Auto-translate channels
        CREATE TABLE IF NOT EXISTS auto_translate_channels (
            channel_id INTEGER PRIMARY KEY,
            guild_id INTEGER NOT NULL,
            target_languages TEXT NOT NULL
        );

        -- Moderation settings
        CREATE TABLE IF NOT EXISTS moderation_settings (
            guild_id INTEGER PRIMARY KEY,
            anti_spam_enabled INTEGER DEFAULT 0,
            spam_threshold INTEGER DEFAULT 5,
            spam_action TEXT DEFAULT 'warn',
            anti_links_enabled INTEGER DEFAULT 0,
            anti_mentions_enabled INTEGER DEFAULT 0,
            mention_threshold INTEGER DEFAULT 5,
            mute_role_id INTEGER DEFAULT 0,
            mod_log_channel_id INTEGER DEFAULT 0
        );

        -- Filtered words
        CREATE TABLE IF NOT EXISTS filtered_words (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            word TEXT NOT NULL,
            UNIQUE(guild_id, word)
        );

        -- Automod whitelist
        CREATE TABLE IF NOT EXISTS automod_whitelist (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            target_id INTEGER NOT NULL,
            target_type TEXT NOT NULL,
            UNIQUE(guild_id, target_id, target_type)
        );

        -- Warnings
        CREATE TABLE IF NOT EXISTS warnings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            user_id INTEGER NOT NULL,
            moderator_id INTEGER NOT NULL,
            reason TEXT,
            timestamp INTEGER NOT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_warnings_guild_user ON warnings(guild_id, user_id);

        -- Mutes
        CREATE TABLE IF NOT EXISTS mutes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            user_id INTEGER NOT NULL,
            moderator_id INTEGER NOT NULL,
            reason TEXT,
            start_time INTEGER NOT NULL,
            end_time INTEGER NOT NULL,
            active INTEGER DEFAULT 1
        );
        CREATE INDEX IF NOT EXISTS idx_mutes_active ON mutes(active, end_time);

        -- Leveling settings
        CREATE TABLE IF NOT EXISTS leveling_settings (
            guild_id INTEGER PRIMARY KEY,
            enabled INTEGER DEFAULT 1,
            xp_min INTEGER DEFAULT 15,
            xp_max INTEGER DEFAULT 25,
            xp_cooldown INTEGER DEFAULT 60,
            voice_xp INTEGER DEFAULT 10,
            voice_min_users INTEGER DEFAULT 2,
            level_up_message TEXT DEFAULT 'Congratulations {user}! You reached level {level}!',
            level_up_channel_id INTEGER DEFAULT 0
        );

        -- User XP
        CREATE TABLE IF NOT EXISTS user_xp (
            guild_id INTEGER NOT NULL,
            user_id INTEGER NOT NULL,
            xp INTEGER DEFAULT 0,
            level INTEGER DEFAULT 0,
            total_messages INTEGER DEFAULT 0,
            voice_minutes INTEGER DEFAULT 0,
            last_xp_time INTEGER DEFAULT 0,
            PRIMARY KEY (guild_id, user_id)
        );
        CREATE INDEX IF NOT EXISTS idx_user_xp_leaderboard ON user_xp(guild_id, xp DESC);

        -- Level rewards
        CREATE TABLE IF NOT EXISTS level_rewards (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            level INTEGER NOT NULL,
            role_id INTEGER NOT NULL,
            UNIQUE(guild_id, level, role_id)
        );

        -- XP blacklist
        CREATE TABLE IF NOT EXISTS xp_blacklist (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            target_id INTEGER NOT NULL,
            target_type TEXT NOT NULL,
            UNIQUE(guild_id, target_id, target_type)
        );

        -- Custom commands
        CREATE TABLE IF NOT EXISTS custom_commands (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            response TEXT NOT NULL,
            is_embed INTEGER DEFAULT 0,
            embed_color TEXT DEFAULT '#0099ff',
            created_by INTEGER NOT NULL,
            uses INTEGER DEFAULT 0,
            UNIQUE(guild_id, name)
        );

        -- Welcome settings
        CREATE TABLE IF NOT EXISTS welcome_settings (
            guild_id INTEGER PRIMARY KEY,
            enabled INTEGER DEFAULT 0,
            channel_id INTEGER DEFAULT 0,
            message TEXT DEFAULT 'Welcome {user} to {server}!',
            use_embed INTEGER DEFAULT 1,
            embed_color TEXT DEFAULT '#00ff00',
            dm_enabled INTEGER DEFAULT 0,
            dm_message TEXT DEFAULT '',
            auto_role_id INTEGER DEFAULT 0
        );

        -- Goodbye settings
        CREATE TABLE IF NOT EXISTS goodbye_settings (
            guild_id INTEGER PRIMARY KEY,
            enabled INTEGER DEFAULT 0,
            channel_id INTEGER DEFAULT 0,
            message TEXT DEFAULT '{user} has left the server.',
            use_embed INTEGER DEFAULT 1,
            embed_color TEXT DEFAULT '#ff0000'
        );

        -- Reaction role messages
        CREATE TABLE IF NOT EXISTS reaction_role_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            channel_id INTEGER NOT NULL,
            message_id INTEGER NOT NULL UNIQUE,
            title TEXT,
            mode TEXT DEFAULT 'normal'
        );

        -- Reaction roles
        CREATE TABLE IF NOT EXISTS reaction_roles (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            message_config_id INTEGER NOT NULL,
            emoji TEXT NOT NULL,
            role_id INTEGER NOT NULL,
            FOREIGN KEY (message_config_id) REFERENCES reaction_role_messages(id) ON DELETE CASCADE,
            UNIQUE(message_config_id, emoji)
        );

        -- Logging settings
        CREATE TABLE IF NOT EXISTS logging_settings (
            guild_id INTEGER PRIMARY KEY,
            message_log_channel INTEGER DEFAULT 0,
            member_log_channel INTEGER DEFAULT 0,
            mod_log_channel INTEGER DEFAULT 0,
            voice_log_channel INTEGER DEFAULT 0,
            server_log_channel INTEGER DEFAULT 0,
            log_message_edits INTEGER DEFAULT 1,
            log_message_deletes INTEGER DEFAULT 1,
            log_member_joins INTEGER DEFAULT 1,
            log_member_leaves INTEGER DEFAULT 1,
            log_member_bans INTEGER DEFAULT 1,
            log_voice_state INTEGER DEFAULT 1,
            log_role_changes INTEGER DEFAULT 1,
            log_nickname_changes INTEGER DEFAULT 1
        );

        -- Logging ignore list
        CREATE TABLE IF NOT EXISTS logging_ignore (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            target_id INTEGER NOT NULL,
            target_type TEXT NOT NULL,
            UNIQUE(guild_id, target_id, target_type)
        );

        -- Music settings
        CREATE TABLE IF NOT EXISTS music_settings (
            guild_id INTEGER PRIMARY KEY,
            dj_role_id INTEGER DEFAULT 0,
            max_queue_size INTEGER DEFAULT 100,
            max_song_duration INTEGER DEFAULT 3600,
            allow_playlists INTEGER DEFAULT 1
        );

        -- Playlists
        CREATE TABLE IF NOT EXISTS playlists (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            user_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            is_public INTEGER DEFAULT 0,
            UNIQUE(user_id, name)
        );

        -- Playlist tracks
        CREATE TABLE IF NOT EXISTS playlist_tracks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            playlist_id INTEGER NOT NULL,
            url TEXT NOT NULL,
            title TEXT NOT NULL,
            duration INTEGER DEFAULT 0,
            position INTEGER NOT NULL,
            FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE
        );

        -- Twitch notifications
        CREATE TABLE IF NOT EXISTS twitch_notifications (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            twitch_username TEXT NOT NULL,
            channel_id INTEGER NOT NULL,
            ping_role_id INTEGER DEFAULT 0,
            custom_message TEXT DEFAULT '',
            is_live INTEGER DEFAULT 0,
            UNIQUE(guild_id, twitch_username)
        );

        -- YouTube notifications
        CREATE TABLE IF NOT EXISTS youtube_notifications (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            guild_id INTEGER NOT NULL,
            youtube_channel_id TEXT NOT NULL,
            discord_channel_id INTEGER NOT NULL,
            ping_role_id INTEGER DEFAULT 0,
            custom_message TEXT DEFAULT '',
            last_video_id TEXT DEFAULT '',
            UNIQUE(guild_id, youtube_channel_id)
        );
    )";

    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &error_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << error_msg << std::endl;
        sqlite3_free(error_msg);
        return false;
    }

    return true;
}

bool Database::migrate_from_json() {
    // Check if bot_settings.json exists
    std::ifstream file("bot_settings.json");
    if (!file.is_open()) {
        return true;  // No file to migrate
    }

    try {
        json data;
        file >> data;
        file.close();

        // Migrate auto_translate_channels
        if (data.contains("auto_translate_channels")) {
            for (auto& [key, value] : data["auto_translate_channels"].items()) {
                dpp::snowflake channel_id = std::stoull(key);
                std::vector<std::string> langs;

                if (value.is_string()) {
                    langs.push_back(value.get<std::string>());
                } else if (value.is_array()) {
                    for (const auto& lang : value) {
                        langs.push_back(lang.get<std::string>());
                    }
                }

                AutoTranslateChannel channel;
                channel.channel_id = channel_id;
                channel.guild_id = 0;  // Will be populated on first use
                channel.target_languages = langs;
                set_auto_translate_channel(channel);
            }
        }

        std::cout << "Migrated settings from bot_settings.json" << std::endl;

        // Rename old file to backup
        std::rename("bot_settings.json", "bot_settings.json.backup");

    } catch (const std::exception& e) {
        std::cerr << "Error migrating from JSON: " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool Database::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << error_msg << std::endl;
        sqlite3_free(error_msg);
        return false;
    }
    return true;
}

// ==================== Guild Settings ====================

std::optional<Database::GuildSettings> Database::get_guild_settings(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT guild_id, prefix, language FROM guilds WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    GuildSettings settings;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        settings.guild_id = sqlite3_column_int64(stmt, 0);
        settings.prefix = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        settings.language = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        sqlite3_finalize(stmt);
        return settings;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::set_guild_settings(const GuildSettings& settings) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO guilds (guild_id, prefix, language)
        VALUES (?, ?, ?)
        ON CONFLICT(guild_id) DO UPDATE SET prefix = excluded.prefix, language = excluded.language
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(settings.guild_id));
    sqlite3_bind_text(stmt, 2, settings.prefix.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, settings.language.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// ==================== Auto-Translate ====================

std::optional<Database::AutoTranslateChannel> Database::get_auto_translate_channel(dpp::snowflake channel_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT channel_id, guild_id, target_languages FROM auto_translate_channels WHERE channel_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(channel_id));

    AutoTranslateChannel channel;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        channel.channel_id = sqlite3_column_int64(stmt, 0);
        channel.guild_id = sqlite3_column_int64(stmt, 1);
        std::string langs = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        // Parse comma-separated languages
        std::istringstream ss(langs);
        std::string lang;
        while (std::getline(ss, lang, ',')) {
            channel.target_languages.push_back(lang);
        }

        sqlite3_finalize(stmt);
        return channel;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<Database::AutoTranslateChannel> Database::get_guild_auto_translate_channels(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<AutoTranslateChannel> channels;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT channel_id, guild_id, target_languages FROM auto_translate_channels WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return channels;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AutoTranslateChannel channel;
        channel.channel_id = sqlite3_column_int64(stmt, 0);
        channel.guild_id = sqlite3_column_int64(stmt, 1);
        std::string langs = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        std::istringstream ss(langs);
        std::string lang;
        while (std::getline(ss, lang, ',')) {
            channel.target_languages.push_back(lang);
        }

        channels.push_back(channel);
    }

    sqlite3_finalize(stmt);
    return channels;
}

bool Database::set_auto_translate_channel(const AutoTranslateChannel& channel) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    // Join languages with comma
    std::string langs;
    for (size_t i = 0; i < channel.target_languages.size(); ++i) {
        if (i > 0) langs += ",";
        langs += channel.target_languages[i];
    }

    const char* sql = R"(
        INSERT INTO auto_translate_channels (channel_id, guild_id, target_languages)
        VALUES (?, ?, ?)
        ON CONFLICT(channel_id) DO UPDATE SET guild_id = excluded.guild_id, target_languages = excluded.target_languages
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(channel.channel_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(channel.guild_id));
    sqlite3_bind_text(stmt, 3, langs.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::remove_auto_translate_channel(dpp::snowflake channel_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM auto_translate_channels WHERE channel_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(channel_id));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// ==================== Moderation ====================

std::optional<Database::ModerationSettings> Database::get_moderation_settings(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM moderation_settings WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    ModerationSettings settings;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        settings.guild_id = sqlite3_column_int64(stmt, 0);
        settings.anti_spam_enabled = sqlite3_column_int(stmt, 1) != 0;
        settings.spam_threshold = sqlite3_column_int(stmt, 2);
        settings.spam_action = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        settings.anti_links_enabled = sqlite3_column_int(stmt, 4) != 0;
        settings.anti_mentions_enabled = sqlite3_column_int(stmt, 5) != 0;
        settings.mention_threshold = sqlite3_column_int(stmt, 6);
        settings.mute_role_id = sqlite3_column_int64(stmt, 7);
        settings.mod_log_channel_id = sqlite3_column_int64(stmt, 8);
        sqlite3_finalize(stmt);
        return settings;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::set_moderation_settings(const ModerationSettings& settings) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO moderation_settings (guild_id, anti_spam_enabled, spam_threshold, spam_action,
            anti_links_enabled, anti_mentions_enabled, mention_threshold, mute_role_id, mod_log_channel_id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(guild_id) DO UPDATE SET
            anti_spam_enabled = excluded.anti_spam_enabled,
            spam_threshold = excluded.spam_threshold,
            spam_action = excluded.spam_action,
            anti_links_enabled = excluded.anti_links_enabled,
            anti_mentions_enabled = excluded.anti_mentions_enabled,
            mention_threshold = excluded.mention_threshold,
            mute_role_id = excluded.mute_role_id,
            mod_log_channel_id = excluded.mod_log_channel_id
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(settings.guild_id));
    sqlite3_bind_int(stmt, 2, settings.anti_spam_enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 3, settings.spam_threshold);
    sqlite3_bind_text(stmt, 4, settings.spam_action.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, settings.anti_links_enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 6, settings.anti_mentions_enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 7, settings.mention_threshold);
    sqlite3_bind_int64(stmt, 8, static_cast<int64_t>(settings.mute_role_id));
    sqlite3_bind_int64(stmt, 9, static_cast<int64_t>(settings.mod_log_channel_id));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<std::string> Database::get_filtered_words(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<std::string> words;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT word FROM filtered_words WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return words;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        words.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }

    sqlite3_finalize(stmt);
    return words;
}

bool Database::add_filtered_word(dpp::snowflake guild_id, const std::string& word) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT OR IGNORE INTO filtered_words (guild_id, word) VALUES (?, ?)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::remove_filtered_word(dpp::snowflake guild_id, const std::string& word) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM filtered_words WHERE guild_id = ? AND word = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 2, word.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// Whitelist helpers
bool Database::is_whitelisted(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT 1 FROM automod_whitelist WHERE guild_id = ? AND target_id = ? AND target_type = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(id));
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);

    bool found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

bool Database::add_whitelist(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT OR IGNORE INTO automod_whitelist (guild_id, target_id, target_type) VALUES (?, ?, ?)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(id));
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::remove_whitelist(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM automod_whitelist WHERE guild_id = ? AND target_id = ? AND target_type = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(id));
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// Warnings
int64_t Database::add_warning(const Warning& warning) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT INTO warnings (guild_id, user_id, moderator_id, reason, timestamp) VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(warning.guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(warning.user_id));
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(warning.moderator_id));
    sqlite3_bind_text(stmt, 4, warning.reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, warning.timestamp);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int64_t id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return id;
}

std::vector<Database::Warning> Database::get_warnings(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<Warning> warnings;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, user_id, moderator_id, reason, timestamp FROM warnings WHERE guild_id = ? AND user_id = ? ORDER BY timestamp DESC";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return warnings;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(user_id));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Warning w;
        w.id = sqlite3_column_int64(stmt, 0);
        w.guild_id = sqlite3_column_int64(stmt, 1);
        w.user_id = sqlite3_column_int64(stmt, 2);
        w.moderator_id = sqlite3_column_int64(stmt, 3);
        w.reason = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        w.timestamp = sqlite3_column_int64(stmt, 5);
        warnings.push_back(w);
    }

    sqlite3_finalize(stmt);
    return warnings;
}

int Database::get_warning_count(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) FROM warnings WHERE guild_id = ? AND user_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(user_id));

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

bool Database::clear_warnings(dpp::snowflake guild_id, dpp::snowflake user_id, int amount) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    std::string sql;
    if (amount < 0) {
        sql = "DELETE FROM warnings WHERE guild_id = ? AND user_id = ?";
    } else {
        sql = "DELETE FROM warnings WHERE id IN (SELECT id FROM warnings WHERE guild_id = ? AND user_id = ? ORDER BY timestamp DESC LIMIT ?)";
    }

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(user_id));
    if (amount >= 0) {
        sqlite3_bind_int(stmt, 3, amount);
    }

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::delete_warning(int64_t warning_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM warnings WHERE id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, warning_id);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// Mutes
int64_t Database::add_mute(const Mute& mute) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT INTO mutes (guild_id, user_id, moderator_id, reason, start_time, end_time, active) VALUES (?, ?, ?, ?, ?, ?, 1)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(mute.guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(mute.user_id));
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(mute.moderator_id));
    sqlite3_bind_text(stmt, 4, mute.reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, mute.start_time);
    sqlite3_bind_int64(stmt, 6, mute.end_time);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int64_t id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return id;
}

std::optional<Database::Mute> Database::get_active_mute(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, user_id, moderator_id, reason, start_time, end_time, active FROM mutes WHERE guild_id = ? AND user_id = ? AND active = 1";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(user_id));

    Mute m;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        m.id = sqlite3_column_int64(stmt, 0);
        m.guild_id = sqlite3_column_int64(stmt, 1);
        m.user_id = sqlite3_column_int64(stmt, 2);
        m.moderator_id = sqlite3_column_int64(stmt, 3);
        m.reason = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        m.start_time = sqlite3_column_int64(stmt, 5);
        m.end_time = sqlite3_column_int64(stmt, 6);
        m.active = sqlite3_column_int(stmt, 7) != 0;
        sqlite3_finalize(stmt);
        return m;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<Database::Mute> Database::get_expired_mutes() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<Mute> mutes;

    sqlite3_stmt* stmt;
    int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql = "SELECT id, guild_id, user_id, moderator_id, reason, start_time, end_time, active FROM mutes WHERE active = 1 AND end_time <= ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return mutes;
    }

    sqlite3_bind_int64(stmt, 1, now);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Mute m;
        m.id = sqlite3_column_int64(stmt, 0);
        m.guild_id = sqlite3_column_int64(stmt, 1);
        m.user_id = sqlite3_column_int64(stmt, 2);
        m.moderator_id = sqlite3_column_int64(stmt, 3);
        m.reason = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        m.start_time = sqlite3_column_int64(stmt, 5);
        m.end_time = sqlite3_column_int64(stmt, 6);
        m.active = sqlite3_column_int(stmt, 7) != 0;
        mutes.push_back(m);
    }

    sqlite3_finalize(stmt);
    return mutes;
}

bool Database::deactivate_mute(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "UPDATE mutes SET active = 0 WHERE guild_id = ? AND user_id = ? AND active = 1";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(user_id));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// ==================== Leveling ====================

std::optional<Database::LevelingSettings> Database::get_leveling_settings(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM leveling_settings WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    LevelingSettings settings;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        settings.guild_id = sqlite3_column_int64(stmt, 0);
        settings.enabled = sqlite3_column_int(stmt, 1) != 0;
        settings.xp_min = sqlite3_column_int(stmt, 2);
        settings.xp_max = sqlite3_column_int(stmt, 3);
        settings.xp_cooldown = sqlite3_column_int(stmt, 4);
        settings.voice_xp = sqlite3_column_int(stmt, 5);
        settings.voice_min_users = sqlite3_column_int(stmt, 6);
        settings.level_up_message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        settings.level_up_channel_id = sqlite3_column_int64(stmt, 8);
        sqlite3_finalize(stmt);
        return settings;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::set_leveling_settings(const LevelingSettings& settings) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO leveling_settings (guild_id, enabled, xp_min, xp_max, xp_cooldown, voice_xp, voice_min_users, level_up_message, level_up_channel_id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(guild_id) DO UPDATE SET
            enabled = excluded.enabled, xp_min = excluded.xp_min, xp_max = excluded.xp_max,
            xp_cooldown = excluded.xp_cooldown, voice_xp = excluded.voice_xp, voice_min_users = excluded.voice_min_users,
            level_up_message = excluded.level_up_message, level_up_channel_id = excluded.level_up_channel_id
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(settings.guild_id));
    sqlite3_bind_int(stmt, 2, settings.enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 3, settings.xp_min);
    sqlite3_bind_int(stmt, 4, settings.xp_max);
    sqlite3_bind_int(stmt, 5, settings.xp_cooldown);
    sqlite3_bind_int(stmt, 6, settings.voice_xp);
    sqlite3_bind_int(stmt, 7, settings.voice_min_users);
    sqlite3_bind_text(stmt, 8, settings.level_up_message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 9, static_cast<int64_t>(settings.level_up_channel_id));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::optional<Database::UserXP> Database::get_user_xp(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM user_xp WHERE guild_id = ? AND user_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(user_id));

    UserXP user_xp;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_xp.guild_id = sqlite3_column_int64(stmt, 0);
        user_xp.user_id = sqlite3_column_int64(stmt, 1);
        user_xp.xp = sqlite3_column_int64(stmt, 2);
        user_xp.level = sqlite3_column_int(stmt, 3);
        user_xp.total_messages = sqlite3_column_int64(stmt, 4);
        user_xp.voice_minutes = sqlite3_column_int64(stmt, 5);
        user_xp.last_xp_time = sqlite3_column_int64(stmt, 6);
        sqlite3_finalize(stmt);
        return user_xp;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::set_user_xp(const UserXP& user_xp) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO user_xp (guild_id, user_id, xp, level, total_messages, voice_minutes, last_xp_time)
        VALUES (?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(guild_id, user_id) DO UPDATE SET
            xp = excluded.xp, level = excluded.level, total_messages = excluded.total_messages,
            voice_minutes = excluded.voice_minutes, last_xp_time = excluded.last_xp_time
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(user_xp.guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(user_xp.user_id));
    sqlite3_bind_int64(stmt, 3, user_xp.xp);
    sqlite3_bind_int(stmt, 4, user_xp.level);
    sqlite3_bind_int64(stmt, 5, user_xp.total_messages);
    sqlite3_bind_int64(stmt, 6, user_xp.voice_minutes);
    sqlite3_bind_int64(stmt, 7, user_xp.last_xp_time);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Database::UserXP> Database::get_leaderboard(dpp::snowflake guild_id, int limit, int offset) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<UserXP> leaderboard;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM user_xp WHERE guild_id = ? ORDER BY xp DESC LIMIT ? OFFSET ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return leaderboard;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int(stmt, 2, limit);
    sqlite3_bind_int(stmt, 3, offset);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UserXP user_xp;
        user_xp.guild_id = sqlite3_column_int64(stmt, 0);
        user_xp.user_id = sqlite3_column_int64(stmt, 1);
        user_xp.xp = sqlite3_column_int64(stmt, 2);
        user_xp.level = sqlite3_column_int(stmt, 3);
        user_xp.total_messages = sqlite3_column_int64(stmt, 4);
        user_xp.voice_minutes = sqlite3_column_int64(stmt, 5);
        user_xp.last_xp_time = sqlite3_column_int64(stmt, 6);
        leaderboard.push_back(user_xp);
    }

    sqlite3_finalize(stmt);
    return leaderboard;
}

int Database::get_user_rank(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) + 1 FROM user_xp WHERE guild_id = ? AND xp > (SELECT xp FROM user_xp WHERE guild_id = ? AND user_id = ?)";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(user_id));

    int rank = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rank = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return rank;
}

bool Database::reset_user_xp(dpp::snowflake guild_id, dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM user_xp WHERE guild_id = ? AND user_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(user_id));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::reset_guild_xp(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM user_xp WHERE guild_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Database::LevelReward> Database::get_level_rewards(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<LevelReward> rewards;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, level, role_id FROM level_rewards WHERE guild_id = ? ORDER BY level ASC";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return rewards;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LevelReward r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.guild_id = sqlite3_column_int64(stmt, 1);
        r.level = sqlite3_column_int(stmt, 2);
        r.role_id = sqlite3_column_int64(stmt, 3);
        rewards.push_back(r);
    }

    sqlite3_finalize(stmt);
    return rewards;
}

bool Database::add_level_reward(dpp::snowflake guild_id, int level, dpp::snowflake role_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT OR REPLACE INTO level_rewards (guild_id, level, role_id) VALUES (?, ?, ?)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int(stmt, 2, level);
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(role_id));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::remove_level_reward(dpp::snowflake guild_id, int level) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM level_rewards WHERE guild_id = ? AND level = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int(stmt, 2, level);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Database::LevelReward> Database::get_rewards_for_level(dpp::snowflake guild_id, int level) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<LevelReward> rewards;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, level, role_id FROM level_rewards WHERE guild_id = ? AND level <= ? ORDER BY level ASC";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return rewards;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int(stmt, 2, level);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LevelReward r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.guild_id = sqlite3_column_int64(stmt, 1);
        r.level = sqlite3_column_int(stmt, 2);
        r.role_id = sqlite3_column_int64(stmt, 3);
        rewards.push_back(r);
    }

    sqlite3_finalize(stmt);
    return rewards;
}

// XP Blacklist
bool Database::is_xp_blacklisted(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT 1 FROM xp_blacklist WHERE guild_id = ? AND target_id = ? AND target_type = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(id));
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);

    bool found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

bool Database::add_xp_blacklist(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT OR IGNORE INTO xp_blacklist (guild_id, target_id, target_type) VALUES (?, ?, ?)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(id));
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::remove_xp_blacklist(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM xp_blacklist WHERE guild_id = ? AND target_id = ? AND target_type = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(id));
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// ==================== Custom Commands ====================

std::optional<Database::CustomCommand> Database::get_custom_command(dpp::snowflake guild_id, const std::string& name) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, name, response, is_embed, embed_color, created_by, uses FROM custom_commands WHERE guild_id = ? AND name = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);

    CustomCommand cmd;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        cmd.id = sqlite3_column_int64(stmt, 0);
        cmd.guild_id = sqlite3_column_int64(stmt, 1);
        cmd.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        cmd.response = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        cmd.is_embed = sqlite3_column_int(stmt, 4) != 0;
        cmd.embed_color = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        cmd.created_by = sqlite3_column_int64(stmt, 6);
        cmd.uses = sqlite3_column_int64(stmt, 7);
        sqlite3_finalize(stmt);
        return cmd;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<Database::CustomCommand> Database::get_guild_custom_commands(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<CustomCommand> commands;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, name, response, is_embed, embed_color, created_by, uses FROM custom_commands WHERE guild_id = ? ORDER BY name ASC";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return commands;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CustomCommand cmd;
        cmd.id = sqlite3_column_int64(stmt, 0);
        cmd.guild_id = sqlite3_column_int64(stmt, 1);
        cmd.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        cmd.response = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        cmd.is_embed = sqlite3_column_int(stmt, 4) != 0;
        cmd.embed_color = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        cmd.created_by = sqlite3_column_int64(stmt, 6);
        cmd.uses = sqlite3_column_int64(stmt, 7);
        commands.push_back(cmd);
    }

    sqlite3_finalize(stmt);
    return commands;
}

bool Database::create_custom_command(const CustomCommand& cmd) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT INTO custom_commands (guild_id, name, response, is_embed, embed_color, created_by, uses) VALUES (?, ?, ?, ?, ?, ?, 0)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(cmd.guild_id));
    sqlite3_bind_text(stmt, 2, cmd.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, cmd.response.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, cmd.is_embed ? 1 : 0);
    sqlite3_bind_text(stmt, 5, cmd.embed_color.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, static_cast<int64_t>(cmd.created_by));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::update_custom_command(const CustomCommand& cmd) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "UPDATE custom_commands SET response = ?, is_embed = ?, embed_color = ? WHERE guild_id = ? AND name = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, cmd.response.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, cmd.is_embed ? 1 : 0);
    sqlite3_bind_text(stmt, 3, cmd.embed_color.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(cmd.guild_id));
    sqlite3_bind_text(stmt, 5, cmd.name.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::delete_custom_command(dpp::snowflake guild_id, const std::string& name) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM custom_commands WHERE guild_id = ? AND name = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::increment_command_uses(dpp::snowflake guild_id, const std::string& name) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "UPDATE custom_commands SET uses = uses + 1 WHERE guild_id = ? AND name = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// ==================== Welcome/Goodbye ====================

std::optional<Database::WelcomeSettings> Database::get_welcome_settings(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM welcome_settings WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    WelcomeSettings settings;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        settings.guild_id = sqlite3_column_int64(stmt, 0);
        settings.enabled = sqlite3_column_int(stmt, 1) != 0;
        settings.channel_id = sqlite3_column_int64(stmt, 2);
        settings.message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        settings.use_embed = sqlite3_column_int(stmt, 4) != 0;
        settings.embed_color = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        settings.dm_enabled = sqlite3_column_int(stmt, 6) != 0;
        auto dm_msg = sqlite3_column_text(stmt, 7);
        settings.dm_message = dm_msg ? reinterpret_cast<const char*>(dm_msg) : "";
        settings.auto_role_id = sqlite3_column_int64(stmt, 8);
        sqlite3_finalize(stmt);
        return settings;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::set_welcome_settings(const WelcomeSettings& settings) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO welcome_settings (guild_id, enabled, channel_id, message, use_embed, embed_color, dm_enabled, dm_message, auto_role_id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(guild_id) DO UPDATE SET
            enabled = excluded.enabled, channel_id = excluded.channel_id, message = excluded.message,
            use_embed = excluded.use_embed, embed_color = excluded.embed_color,
            dm_enabled = excluded.dm_enabled, dm_message = excluded.dm_message, auto_role_id = excluded.auto_role_id
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(settings.guild_id));
    sqlite3_bind_int(stmt, 2, settings.enabled ? 1 : 0);
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(settings.channel_id));
    sqlite3_bind_text(stmt, 4, settings.message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, settings.use_embed ? 1 : 0);
    sqlite3_bind_text(stmt, 6, settings.embed_color.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, settings.dm_enabled ? 1 : 0);
    sqlite3_bind_text(stmt, 8, settings.dm_message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 9, static_cast<int64_t>(settings.auto_role_id));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::optional<Database::GoodbyeSettings> Database::get_goodbye_settings(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM goodbye_settings WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    GoodbyeSettings settings;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        settings.guild_id = sqlite3_column_int64(stmt, 0);
        settings.enabled = sqlite3_column_int(stmt, 1) != 0;
        settings.channel_id = sqlite3_column_int64(stmt, 2);
        settings.message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        settings.use_embed = sqlite3_column_int(stmt, 4) != 0;
        settings.embed_color = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        sqlite3_finalize(stmt);
        return settings;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::set_goodbye_settings(const GoodbyeSettings& settings) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO goodbye_settings (guild_id, enabled, channel_id, message, use_embed, embed_color)
        VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(guild_id) DO UPDATE SET
            enabled = excluded.enabled, channel_id = excluded.channel_id, message = excluded.message,
            use_embed = excluded.use_embed, embed_color = excluded.embed_color
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(settings.guild_id));
    sqlite3_bind_int(stmt, 2, settings.enabled ? 1 : 0);
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(settings.channel_id));
    sqlite3_bind_text(stmt, 4, settings.message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, settings.use_embed ? 1 : 0);
    sqlite3_bind_text(stmt, 6, settings.embed_color.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// ==================== Reaction Roles ====================

std::optional<Database::ReactionRoleMessage> Database::get_reaction_role_message(dpp::snowflake message_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, channel_id, message_id, title, mode FROM reaction_role_messages WHERE message_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(message_id));

    ReactionRoleMessage msg;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.guild_id = sqlite3_column_int64(stmt, 1);
        msg.channel_id = sqlite3_column_int64(stmt, 2);
        msg.message_id = sqlite3_column_int64(stmt, 3);
        auto title = sqlite3_column_text(stmt, 4);
        msg.title = title ? reinterpret_cast<const char*>(title) : "";
        msg.mode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        sqlite3_finalize(stmt);
        return msg;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<Database::ReactionRoleMessage> Database::get_guild_reaction_role_messages(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<ReactionRoleMessage> messages;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, channel_id, message_id, title, mode FROM reaction_role_messages WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return messages;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ReactionRoleMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.guild_id = sqlite3_column_int64(stmt, 1);
        msg.channel_id = sqlite3_column_int64(stmt, 2);
        msg.message_id = sqlite3_column_int64(stmt, 3);
        auto title = sqlite3_column_text(stmt, 4);
        msg.title = title ? reinterpret_cast<const char*>(title) : "";
        msg.mode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        messages.push_back(msg);
    }

    sqlite3_finalize(stmt);
    return messages;
}

int64_t Database::create_reaction_role_message(const ReactionRoleMessage& msg) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT INTO reaction_role_messages (guild_id, channel_id, message_id, title, mode) VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(msg.guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(msg.channel_id));
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(msg.message_id));
    sqlite3_bind_text(stmt, 4, msg.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, msg.mode.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int64_t id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return id;
}

bool Database::delete_reaction_role_message(dpp::snowflake message_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM reaction_role_messages WHERE message_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(message_id));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Database::ReactionRole> Database::get_reaction_roles(int64_t message_config_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<ReactionRole> roles;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, message_config_id, emoji, role_id FROM reaction_roles WHERE message_config_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return roles;
    }

    sqlite3_bind_int64(stmt, 1, message_config_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ReactionRole r;
        r.id = sqlite3_column_int64(stmt, 0);
        r.message_config_id = sqlite3_column_int64(stmt, 1);
        r.emoji = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.role_id = sqlite3_column_int64(stmt, 3);
        roles.push_back(r);
    }

    sqlite3_finalize(stmt);
    return roles;
}

std::optional<Database::ReactionRole> Database::get_reaction_role(int64_t message_config_id, const std::string& emoji) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, message_config_id, emoji, role_id FROM reaction_roles WHERE message_config_id = ? AND emoji = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, message_config_id);
    sqlite3_bind_text(stmt, 2, emoji.c_str(), -1, SQLITE_TRANSIENT);

    ReactionRole r;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        r.id = sqlite3_column_int64(stmt, 0);
        r.message_config_id = sqlite3_column_int64(stmt, 1);
        r.emoji = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.role_id = sqlite3_column_int64(stmt, 3);
        sqlite3_finalize(stmt);
        return r;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::add_reaction_role(int64_t message_config_id, const std::string& emoji, dpp::snowflake role_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT OR REPLACE INTO reaction_roles (message_config_id, emoji, role_id) VALUES (?, ?, ?)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, message_config_id);
    sqlite3_bind_text(stmt, 2, emoji.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(role_id));

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::remove_reaction_role(int64_t message_config_id, const std::string& emoji) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM reaction_roles WHERE message_config_id = ? AND emoji = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, message_config_id);
    sqlite3_bind_text(stmt, 2, emoji.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// ==================== Logging ====================

std::optional<Database::LoggingSettings> Database::get_logging_settings(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM logging_settings WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    LoggingSettings settings;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        settings.guild_id = sqlite3_column_int64(stmt, 0);
        settings.message_log_channel = sqlite3_column_int64(stmt, 1);
        settings.member_log_channel = sqlite3_column_int64(stmt, 2);
        settings.mod_log_channel = sqlite3_column_int64(stmt, 3);
        settings.voice_log_channel = sqlite3_column_int64(stmt, 4);
        settings.server_log_channel = sqlite3_column_int64(stmt, 5);
        settings.log_message_edits = sqlite3_column_int(stmt, 6) != 0;
        settings.log_message_deletes = sqlite3_column_int(stmt, 7) != 0;
        settings.log_member_joins = sqlite3_column_int(stmt, 8) != 0;
        settings.log_member_leaves = sqlite3_column_int(stmt, 9) != 0;
        settings.log_member_bans = sqlite3_column_int(stmt, 10) != 0;
        settings.log_voice_state = sqlite3_column_int(stmt, 11) != 0;
        settings.log_role_changes = sqlite3_column_int(stmt, 12) != 0;
        settings.log_nickname_changes = sqlite3_column_int(stmt, 13) != 0;
        sqlite3_finalize(stmt);
        return settings;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::set_logging_settings(const LoggingSettings& settings) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO logging_settings (guild_id, message_log_channel, member_log_channel, mod_log_channel,
            voice_log_channel, server_log_channel, log_message_edits, log_message_deletes,
            log_member_joins, log_member_leaves, log_member_bans, log_voice_state,
            log_role_changes, log_nickname_changes)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(guild_id) DO UPDATE SET
            message_log_channel = excluded.message_log_channel,
            member_log_channel = excluded.member_log_channel,
            mod_log_channel = excluded.mod_log_channel,
            voice_log_channel = excluded.voice_log_channel,
            server_log_channel = excluded.server_log_channel,
            log_message_edits = excluded.log_message_edits,
            log_message_deletes = excluded.log_message_deletes,
            log_member_joins = excluded.log_member_joins,
            log_member_leaves = excluded.log_member_leaves,
            log_member_bans = excluded.log_member_bans,
            log_voice_state = excluded.log_voice_state,
            log_role_changes = excluded.log_role_changes,
            log_nickname_changes = excluded.log_nickname_changes
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(settings.guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(settings.message_log_channel));
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(settings.member_log_channel));
    sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(settings.mod_log_channel));
    sqlite3_bind_int64(stmt, 5, static_cast<int64_t>(settings.voice_log_channel));
    sqlite3_bind_int64(stmt, 6, static_cast<int64_t>(settings.server_log_channel));
    sqlite3_bind_int(stmt, 7, settings.log_message_edits ? 1 : 0);
    sqlite3_bind_int(stmt, 8, settings.log_message_deletes ? 1 : 0);
    sqlite3_bind_int(stmt, 9, settings.log_member_joins ? 1 : 0);
    sqlite3_bind_int(stmt, 10, settings.log_member_leaves ? 1 : 0);
    sqlite3_bind_int(stmt, 11, settings.log_member_bans ? 1 : 0);
    sqlite3_bind_int(stmt, 12, settings.log_voice_state ? 1 : 0);
    sqlite3_bind_int(stmt, 13, settings.log_role_changes ? 1 : 0);
    sqlite3_bind_int(stmt, 14, settings.log_nickname_changes ? 1 : 0);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::is_logging_ignored(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT 1 FROM logging_ignore WHERE guild_id = ? AND target_id = ? AND target_type = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(id));
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);

    bool found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

bool Database::add_logging_ignore(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT OR IGNORE INTO logging_ignore (guild_id, target_id, target_type) VALUES (?, ?, ?)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(id));
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::remove_logging_ignore(dpp::snowflake guild_id, dpp::snowflake id, const std::string& type) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM logging_ignore WHERE guild_id = ? AND target_id = ? AND target_type = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(id));
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// ==================== Music ====================

std::optional<Database::MusicSettings> Database::get_music_settings(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM music_settings WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    MusicSettings settings;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        settings.guild_id = sqlite3_column_int64(stmt, 0);
        settings.dj_role_id = sqlite3_column_int64(stmt, 1);
        settings.max_queue_size = sqlite3_column_int(stmt, 2);
        settings.max_song_duration = sqlite3_column_int(stmt, 3);
        settings.allow_playlists = sqlite3_column_int(stmt, 4) != 0;
        sqlite3_finalize(stmt);
        return settings;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool Database::set_music_settings(const MusicSettings& settings) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = R"(
        INSERT INTO music_settings (guild_id, dj_role_id, max_queue_size, max_song_duration, allow_playlists)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(guild_id) DO UPDATE SET
            dj_role_id = excluded.dj_role_id, max_queue_size = excluded.max_queue_size,
            max_song_duration = excluded.max_song_duration, allow_playlists = excluded.allow_playlists
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(settings.guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(settings.dj_role_id));
    sqlite3_bind_int(stmt, 3, settings.max_queue_size);
    sqlite3_bind_int(stmt, 4, settings.max_song_duration);
    sqlite3_bind_int(stmt, 5, settings.allow_playlists ? 1 : 0);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Database::Playlist> Database::get_user_playlists(dpp::snowflake user_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<Playlist> playlists;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, user_id, name, is_public FROM playlists WHERE user_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return playlists;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(user_id));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Playlist p;
        p.id = sqlite3_column_int64(stmt, 0);
        p.guild_id = sqlite3_column_int64(stmt, 1);
        p.user_id = sqlite3_column_int64(stmt, 2);
        p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        p.is_public = sqlite3_column_int(stmt, 4) != 0;
        playlists.push_back(p);
    }

    sqlite3_finalize(stmt);
    return playlists;
}

std::optional<Database::Playlist> Database::get_playlist(dpp::snowflake user_id, const std::string& name) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, user_id, name, is_public FROM playlists WHERE user_id = ? AND name = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(user_id));
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);

    Playlist p;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        p.id = sqlite3_column_int64(stmt, 0);
        p.guild_id = sqlite3_column_int64(stmt, 1);
        p.user_id = sqlite3_column_int64(stmt, 2);
        p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        p.is_public = sqlite3_column_int(stmt, 4) != 0;
        sqlite3_finalize(stmt);
        return p;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

int64_t Database::create_playlist(const Playlist& playlist) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT INTO playlists (guild_id, user_id, name, is_public) VALUES (?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(playlist.guild_id));
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(playlist.user_id));
    sqlite3_bind_text(stmt, 3, playlist.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, playlist.is_public ? 1 : 0);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int64_t id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return id;
}

bool Database::delete_playlist(int64_t playlist_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM playlists WHERE id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, playlist_id);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Database::PlaylistTrack> Database::get_playlist_tracks(int64_t playlist_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<PlaylistTrack> tracks;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, playlist_id, url, title, duration, position FROM playlist_tracks WHERE playlist_id = ? ORDER BY position ASC";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return tracks;
    }

    sqlite3_bind_int64(stmt, 1, playlist_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PlaylistTrack t;
        t.id = sqlite3_column_int64(stmt, 0);
        t.playlist_id = sqlite3_column_int64(stmt, 1);
        t.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        t.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        t.duration = sqlite3_column_int(stmt, 4);
        t.position = sqlite3_column_int(stmt, 5);
        tracks.push_back(t);
    }

    sqlite3_finalize(stmt);
    return tracks;
}

bool Database::add_playlist_track(const PlaylistTrack& track) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT INTO playlist_tracks (playlist_id, url, title, duration, position) VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, track.playlist_id);
    sqlite3_bind_text(stmt, 2, track.url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, track.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, track.duration);
    sqlite3_bind_int(stmt, 5, track.position);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::remove_playlist_track(int64_t playlist_id, int position) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM playlist_tracks WHERE playlist_id = ? AND position = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, playlist_id);
    sqlite3_bind_int(stmt, 2, position);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::clear_playlist_tracks(int64_t playlist_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM playlist_tracks WHERE playlist_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, playlist_id);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

// ==================== Notifications ====================

std::vector<Database::TwitchNotification> Database::get_twitch_notifications(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<TwitchNotification> notifications;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, twitch_username, channel_id, ping_role_id, custom_message, is_live FROM twitch_notifications WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return notifications;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TwitchNotification n;
        n.id = sqlite3_column_int64(stmt, 0);
        n.guild_id = sqlite3_column_int64(stmt, 1);
        n.twitch_username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        n.channel_id = sqlite3_column_int64(stmt, 3);
        n.ping_role_id = sqlite3_column_int64(stmt, 4);
        auto msg = sqlite3_column_text(stmt, 5);
        n.custom_message = msg ? reinterpret_cast<const char*>(msg) : "";
        n.is_live = sqlite3_column_int(stmt, 6) != 0;
        notifications.push_back(n);
    }

    sqlite3_finalize(stmt);
    return notifications;
}

bool Database::add_twitch_notification(const TwitchNotification& notif) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT OR REPLACE INTO twitch_notifications (guild_id, twitch_username, channel_id, ping_role_id, custom_message, is_live) VALUES (?, ?, ?, ?, ?, 0)";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(notif.guild_id));
    sqlite3_bind_text(stmt, 2, notif.twitch_username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(notif.channel_id));
    sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(notif.ping_role_id));
    sqlite3_bind_text(stmt, 5, notif.custom_message.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::remove_twitch_notification(dpp::snowflake guild_id, const std::string& username) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM twitch_notifications WHERE guild_id = ? AND twitch_username = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::update_twitch_live_status(dpp::snowflake guild_id, const std::string& username, bool is_live) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "UPDATE twitch_notifications SET is_live = ? WHERE guild_id = ? AND twitch_username = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, is_live ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Database::TwitchNotification> Database::get_all_twitch_notifications() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<TwitchNotification> notifications;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, twitch_username, channel_id, ping_role_id, custom_message, is_live FROM twitch_notifications";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return notifications;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TwitchNotification n;
        n.id = sqlite3_column_int64(stmt, 0);
        n.guild_id = sqlite3_column_int64(stmt, 1);
        n.twitch_username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        n.channel_id = sqlite3_column_int64(stmt, 3);
        n.ping_role_id = sqlite3_column_int64(stmt, 4);
        auto msg = sqlite3_column_text(stmt, 5);
        n.custom_message = msg ? reinterpret_cast<const char*>(msg) : "";
        n.is_live = sqlite3_column_int(stmt, 6) != 0;
        notifications.push_back(n);
    }

    sqlite3_finalize(stmt);
    return notifications;
}

std::vector<Database::YouTubeNotification> Database::get_youtube_notifications(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<YouTubeNotification> notifications;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, youtube_channel_id, discord_channel_id, ping_role_id, custom_message, last_video_id FROM youtube_notifications WHERE guild_id = ?";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return notifications;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        YouTubeNotification n;
        n.id = sqlite3_column_int64(stmt, 0);
        n.guild_id = sqlite3_column_int64(stmt, 1);
        n.youtube_channel_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        n.discord_channel_id = sqlite3_column_int64(stmt, 3);
        n.ping_role_id = sqlite3_column_int64(stmt, 4);
        auto msg = sqlite3_column_text(stmt, 5);
        n.custom_message = msg ? reinterpret_cast<const char*>(msg) : "";
        auto vid = sqlite3_column_text(stmt, 6);
        n.last_video_id = vid ? reinterpret_cast<const char*>(vid) : "";
        notifications.push_back(n);
    }

    sqlite3_finalize(stmt);
    return notifications;
}

bool Database::add_youtube_notification(const YouTubeNotification& notif) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "INSERT OR REPLACE INTO youtube_notifications (guild_id, youtube_channel_id, discord_channel_id, ping_role_id, custom_message, last_video_id) VALUES (?, ?, ?, ?, ?, '')";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(notif.guild_id));
    sqlite3_bind_text(stmt, 2, notif.youtube_channel_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, static_cast<int64_t>(notif.discord_channel_id));
    sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(notif.ping_role_id));
    sqlite3_bind_text(stmt, 5, notif.custom_message.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::remove_youtube_notification(dpp::snowflake guild_id, const std::string& channel_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "DELETE FROM youtube_notifications WHERE guild_id = ? AND youtube_channel_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 2, channel_id.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool Database::update_youtube_last_video(dpp::snowflake guild_id, const std::string& channel_id, const std::string& video_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "UPDATE youtube_notifications SET last_video_id = ? WHERE guild_id = ? AND youtube_channel_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, video_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(guild_id));
    sqlite3_bind_text(stmt, 3, channel_id.c_str(), -1, SQLITE_TRANSIENT);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::vector<Database::YouTubeNotification> Database::get_all_youtube_notifications() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<YouTubeNotification> notifications;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, guild_id, youtube_channel_id, discord_channel_id, ping_role_id, custom_message, last_video_id FROM youtube_notifications";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return notifications;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        YouTubeNotification n;
        n.id = sqlite3_column_int64(stmt, 0);
        n.guild_id = sqlite3_column_int64(stmt, 1);
        n.youtube_channel_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        n.discord_channel_id = sqlite3_column_int64(stmt, 3);
        n.ping_role_id = sqlite3_column_int64(stmt, 4);
        auto msg = sqlite3_column_text(stmt, 5);
        n.custom_message = msg ? reinterpret_cast<const char*>(msg) : "";
        auto vid = sqlite3_column_text(stmt, 6);
        n.last_video_id = vid ? reinterpret_cast<const char*>(vid) : "";
        notifications.push_back(n);
    }

    sqlite3_finalize(stmt);
    return notifications;
}

// Global database instance
static std::unique_ptr<Database> g_database;
static std::once_flag g_database_init;

Database& get_database() {
    std::call_once(g_database_init, []() {
        g_database = std::make_unique<Database>();
    });
    return *g_database;
}

} // namespace bot
