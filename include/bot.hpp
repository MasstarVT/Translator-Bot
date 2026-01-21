#pragma once

#include <dpp/dpp.h>
#include <memory>
#include <vector>
#include <functional>

namespace bot {

// Forward declarations
class TranslationModule;
class ModerationModule;
class LevelingModule;
class CustomCommandsModule;
class WelcomeModule;
class MusicModule;
class ReactionRolesModule;
class LoggingModule;
class NotificationsModule;

class Bot {
public:
    Bot();
    ~Bot();

    // Initialize and run the bot
    bool initialize();
    void run();
    void shutdown();

    // Get the DPP cluster
    dpp::cluster& get_cluster() { return *cluster_; }

    // Register slash commands
    void register_commands();

    // Module access
    TranslationModule* get_translation_module() { return translation_module_.get(); }
    ModerationModule* get_moderation_module() { return moderation_module_.get(); }
    LevelingModule* get_leveling_module() { return leveling_module_.get(); }
    CustomCommandsModule* get_custom_commands_module() { return custom_commands_module_.get(); }
    WelcomeModule* get_welcome_module() { return welcome_module_.get(); }
    MusicModule* get_music_module() { return music_module_.get(); }
    ReactionRolesModule* get_reaction_roles_module() { return reaction_roles_module_.get(); }
    LoggingModule* get_logging_module() { return logging_module_.get(); }
    NotificationsModule* get_notifications_module() { return notifications_module_.get(); }

private:
    std::unique_ptr<dpp::cluster> cluster_;

    // Modules
    std::unique_ptr<TranslationModule> translation_module_;
    std::unique_ptr<ModerationModule> moderation_module_;
    std::unique_ptr<LevelingModule> leveling_module_;
    std::unique_ptr<CustomCommandsModule> custom_commands_module_;
    std::unique_ptr<WelcomeModule> welcome_module_;
    std::unique_ptr<MusicModule> music_module_;
    std::unique_ptr<ReactionRolesModule> reaction_roles_module_;
    std::unique_ptr<LoggingModule> logging_module_;
    std::unique_ptr<NotificationsModule> notifications_module_;

    // Event handlers
    void setup_event_handlers();
    void on_ready(const dpp::ready_t& event);
    void on_slashcommand(const dpp::slashcommand_t& event);
    void on_message_create(const dpp::message_create_t& event);
    void on_message_delete(const dpp::message_delete_t& event);
    void on_message_update(const dpp::message_update_t& event);
    void on_guild_member_add(const dpp::guild_member_add_t& event);
    void on_guild_member_remove(const dpp::guild_member_remove_t& event);
    void on_guild_ban_add(const dpp::guild_ban_add_t& event);
    void on_guild_ban_remove(const dpp::guild_ban_remove_t& event);
    void on_voice_state_update(const dpp::voice_state_update_t& event);
    void on_message_reaction_add(const dpp::message_reaction_add_t& event);
    void on_message_reaction_remove(const dpp::message_reaction_remove_t& event);

    // Initialize modules
    void init_modules();
};

// Global bot instance
Bot& get_bot();

} // namespace bot
