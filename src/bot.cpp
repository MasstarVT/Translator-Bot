#include "bot.hpp"
#include "config.hpp"
#include "database.hpp"
#include "utils/curl_helper.hpp"
#include "utils/thread_pool.hpp"
#include "modules/translation.hpp"
#include "modules/moderation.hpp"
#include "modules/leveling.hpp"
#include "modules/custom_commands.hpp"
#include "modules/welcome.hpp"
#include "modules/music.hpp"
#include "modules/reaction_roles.hpp"
#include "modules/logging.hpp"
#include "modules/notifications.hpp"
#include <iostream>

namespace bot {

Bot::Bot() {}

Bot::~Bot() {
    shutdown();
}

bool Bot::initialize() {
    // Load configuration
    if (!get_config().load()) {
        std::cerr << "Failed to load configuration" << std::endl;
        return false;
    }

    // Initialize CURL
    CurlHelper::global_init();

    // Initialize database
    if (!get_database().initialize(get_config().get_database_path())) {
        std::cerr << "Failed to initialize database" << std::endl;
        return false;
    }

    // Create bot cluster
    cluster_ = std::make_unique<dpp::cluster>(
        get_config().get_token(),
        dpp::i_default_intents | dpp::i_message_content | dpp::i_guild_members
    );

    cluster_->on_log(dpp::utility::cout_logger());

    // Initialize modules
    init_modules();

    // Setup event handlers
    setup_event_handlers();

    std::cout << "Bot initialized successfully" << std::endl;
    return true;
}

void Bot::run() {
    if (!cluster_) {
        std::cerr << "Bot not initialized" << std::endl;
        return;
    }

    // Start notifications module if enabled
    if (get_config().is_module_enabled("notifications") && notifications_module_) {
        notifications_module_->start();
    }

    cluster_->start(dpp::st_wait);
}

void Bot::shutdown() {
    if (notifications_module_) {
        notifications_module_->stop();
    }

    CurlHelper::global_cleanup();
    get_database().close();
}

void Bot::init_modules() {
    auto& config = get_config();

    if (config.is_module_enabled("translation")) {
        translation_module_ = std::make_unique<TranslationModule>(*cluster_);
        std::cout << "Translation module enabled" << std::endl;
    }

    if (config.is_module_enabled("moderation")) {
        moderation_module_ = std::make_unique<ModerationModule>(*cluster_);
        std::cout << "Moderation module enabled" << std::endl;
    }

    if (config.is_module_enabled("leveling")) {
        leveling_module_ = std::make_unique<LevelingModule>(*cluster_);
        std::cout << "Leveling module enabled" << std::endl;
    }

    if (config.is_module_enabled("custom_commands")) {
        custom_commands_module_ = std::make_unique<CustomCommandsModule>(*cluster_);
        std::cout << "Custom commands module enabled" << std::endl;
    }

    if (config.is_module_enabled("welcome")) {
        welcome_module_ = std::make_unique<WelcomeModule>(*cluster_);
        std::cout << "Welcome module enabled" << std::endl;
    }

    if (config.is_module_enabled("music")) {
        music_module_ = std::make_unique<MusicModule>(*cluster_);
        std::cout << "Music module enabled" << std::endl;
    }

    if (config.is_module_enabled("reaction_roles")) {
        reaction_roles_module_ = std::make_unique<ReactionRolesModule>(*cluster_);
        std::cout << "Reaction roles module enabled" << std::endl;
    }

    if (config.is_module_enabled("logging")) {
        logging_module_ = std::make_unique<LoggingModule>(*cluster_);
        std::cout << "Logging module enabled" << std::endl;
    }

    if (config.is_module_enabled("notifications")) {
        notifications_module_ = std::make_unique<NotificationsModule>(*cluster_);
        std::cout << "Notifications module enabled" << std::endl;
    }
}

void Bot::setup_event_handlers() {
    cluster_->on_ready([this](const dpp::ready_t& event) {
        on_ready(event);
    });

    cluster_->on_slashcommand([this](const dpp::slashcommand_t& event) {
        on_slashcommand(event);
    });

    cluster_->on_message_create([this](const dpp::message_create_t& event) {
        on_message_create(event);
    });

    cluster_->on_message_delete([this](const dpp::message_delete_t& event) {
        on_message_delete(event);
    });

    cluster_->on_message_update([this](const dpp::message_update_t& event) {
        on_message_update(event);
    });

    cluster_->on_guild_member_add([this](const dpp::guild_member_add_t& event) {
        on_guild_member_add(event);
    });

    cluster_->on_guild_member_remove([this](const dpp::guild_member_remove_t& event) {
        on_guild_member_remove(event);
    });

    cluster_->on_guild_ban_add([this](const dpp::guild_ban_add_t& event) {
        on_guild_ban_add(event);
    });

    cluster_->on_guild_ban_remove([this](const dpp::guild_ban_remove_t& event) {
        on_guild_ban_remove(event);
    });

    cluster_->on_voice_state_update([this](const dpp::voice_state_update_t& event) {
        on_voice_state_update(event);
    });

    cluster_->on_message_reaction_add([this](const dpp::message_reaction_add_t& event) {
        on_message_reaction_add(event);
    });

    cluster_->on_message_reaction_remove([this](const dpp::message_reaction_remove_t& event) {
        on_message_reaction_remove(event);
    });
}

void Bot::register_commands() {
    std::vector<dpp::slashcommand> commands;

    if (translation_module_) {
        auto cmds = translation_module_->get_commands();
        commands.insert(commands.end(), cmds.begin(), cmds.end());
    }

    if (moderation_module_) {
        auto cmds = moderation_module_->get_commands();
        commands.insert(commands.end(), cmds.begin(), cmds.end());
    }

    if (leveling_module_) {
        auto cmds = leveling_module_->get_commands();
        commands.insert(commands.end(), cmds.begin(), cmds.end());
    }

    if (custom_commands_module_) {
        auto cmds = custom_commands_module_->get_commands();
        commands.insert(commands.end(), cmds.begin(), cmds.end());
    }

    if (welcome_module_) {
        auto cmds = welcome_module_->get_commands();
        commands.insert(commands.end(), cmds.begin(), cmds.end());
    }

    if (music_module_) {
        auto cmds = music_module_->get_commands();
        commands.insert(commands.end(), cmds.begin(), cmds.end());
    }

    if (reaction_roles_module_) {
        auto cmds = reaction_roles_module_->get_commands();
        commands.insert(commands.end(), cmds.begin(), cmds.end());
    }

    if (logging_module_) {
        auto cmds = logging_module_->get_commands();
        commands.insert(commands.end(), cmds.begin(), cmds.end());
    }

    if (notifications_module_) {
        auto cmds = notifications_module_->get_commands();
        commands.insert(commands.end(), cmds.begin(), cmds.end());
    }

    cluster_->global_bulk_command_create(commands, [](const dpp::confirmation_callback_t& callback) {
        if (callback.is_error()) {
            std::cerr << "Failed to register commands: " << callback.get_error().message << std::endl;
        } else {
            std::cout << "Slash commands registered successfully" << std::endl;
        }
    });
}

void Bot::on_ready(const dpp::ready_t& event) {
    if (dpp::run_once<struct register_bot_commands>()) {
        std::cout << cluster_->me.username << " has connected to Discord!" << std::endl;
        std::cout << "Bot ID: " << cluster_->me.id << std::endl;

        register_commands();
    }
}

void Bot::on_slashcommand(const dpp::slashcommand_t& event) {
    std::string cmd = event.command.get_command_name();

    // Route to appropriate module
    if (translation_module_ &&
        (cmd == "translate" || cmd == "detectlanguage" || cmd == "languages" || cmd == "autotranslate")) {
        translation_module_->handle_command(event);
    }
    else if (moderation_module_ &&
             (cmd == "warn" || cmd == "warnings" || cmd == "clearwarnings" ||
              cmd == "mute" || cmd == "unmute" || cmd == "kick" || cmd == "ban" ||
              cmd == "unban" || cmd == "automod")) {
        moderation_module_->handle_command(event);
    }
    else if (leveling_module_ &&
             (cmd == "rank" || cmd == "leaderboard" || cmd == "setxp" || cmd == "addxp" ||
              cmd == "resetxp" || cmd == "levelconfig" || cmd == "levelreward")) {
        leveling_module_->handle_command(event);
    }
    else if (custom_commands_module_ &&
             (cmd == "customcommand" || cmd == "c")) {
        custom_commands_module_->handle_command(event);
    }
    else if (welcome_module_ &&
             (cmd == "welcome" || cmd == "goodbye")) {
        welcome_module_->handle_command(event);
    }
    else if (music_module_ &&
             (cmd == "play" || cmd == "pause" || cmd == "resume" || cmd == "skip" ||
              cmd == "stop" || cmd == "queue" || cmd == "nowplaying" || cmd == "volume" ||
              cmd == "shuffle" || cmd == "loop" || cmd == "remove" || cmd == "seek" ||
              cmd == "join" || cmd == "leave" || cmd == "playlist")) {
        music_module_->handle_command(event);
    }
    else if (reaction_roles_module_ && cmd == "reactionrole") {
        reaction_roles_module_->handle_command(event);
    }
    else if (logging_module_ && cmd == "logging") {
        logging_module_->handle_command(event);
    }
    else if (notifications_module_ &&
             (cmd == "twitch" || cmd == "youtube")) {
        notifications_module_->handle_command(event);
    }
}

void Bot::on_message_create(const dpp::message_create_t& event) {
    // Cache message for logging
    if (logging_module_) {
        logging_module_->cache_message(event.msg);
    }

    // Handle moderation (auto-mod)
    if (moderation_module_) {
        moderation_module_->handle_message(event);
    }

    // Handle leveling XP
    if (leveling_module_) {
        leveling_module_->handle_message(event);
    }

    // Handle auto-translation
    if (translation_module_) {
        translation_module_->handle_message(event);
    }
}

void Bot::on_message_delete(const dpp::message_delete_t& event) {
    if (logging_module_) {
        logging_module_->log_message_delete(event);
    }
}

void Bot::on_message_update(const dpp::message_update_t& event) {
    if (logging_module_) {
        logging_module_->log_message_update(event);
    }
}

void Bot::on_guild_member_add(const dpp::guild_member_add_t& event) {
    if (welcome_module_) {
        welcome_module_->handle_member_join(event);
    }

    if (logging_module_) {
        logging_module_->log_member_join(event);
    }
}

void Bot::on_guild_member_remove(const dpp::guild_member_remove_t& event) {
    if (welcome_module_) {
        welcome_module_->handle_member_leave(event);
    }

    if (logging_module_) {
        logging_module_->log_member_leave(event);
    }
}

void Bot::on_guild_ban_add(const dpp::guild_ban_add_t& event) {
    if (logging_module_) {
        logging_module_->log_member_ban(event);
    }
}

void Bot::on_guild_ban_remove(const dpp::guild_ban_remove_t& event) {
    if (logging_module_) {
        logging_module_->log_member_unban(event);
    }
}

void Bot::on_voice_state_update(const dpp::voice_state_update_t& event) {
    if (music_module_) {
        music_module_->handle_voice_state(event);
    }

    if (leveling_module_) {
        leveling_module_->handle_voice_state(event);
    }

    if (logging_module_) {
        logging_module_->log_voice_state(event);
    }
}

void Bot::on_message_reaction_add(const dpp::message_reaction_add_t& event) {
    if (reaction_roles_module_) {
        reaction_roles_module_->handle_reaction_add(event);
    }
}

void Bot::on_message_reaction_remove(const dpp::message_reaction_remove_t& event) {
    if (reaction_roles_module_) {
        reaction_roles_module_->handle_reaction_remove(event);
    }
}

// Global bot instance
static std::unique_ptr<Bot> g_bot;
static std::once_flag g_bot_init;

Bot& get_bot() {
    std::call_once(g_bot_init, []() {
        g_bot = std::make_unique<Bot>();
    });
    return *g_bot;
}

} // namespace bot
